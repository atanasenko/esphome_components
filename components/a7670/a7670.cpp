#include "a7670.h"
#include "esphome/core/log.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>

namespace esphome {
namespace a7670 {

static const char *const TAG = "a7670";
static const int init_delay = 10000;
static const int power_blink = 1000;
static const int call_check_interval = 500;
static const int modem_init_timeout = 30000;
static const int idle_timeout = 10000;
static const int rx_timeout = 5000;

const char CR = 0x0D;
const char LF = 0x0A;
const char CTRLZ = 0x1A;

void A7670Component::dump_config() {
  ESP_LOGCONFIG(TAG, "A7670:");
  if (power_pin_ != nullptr) {
    LOG_PIN("  Power Pin: ", power_pin_);
  }
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Registered", registered_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Rssi", rssi_sensor_);
#endif
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "Modem type: %d", modem_type_);
}

void A7670Component::setup() {
  if (power_pin_ != nullptr) {
    power_pin_->setup();  // OUTPUT
    power_pin_->digital_write(false);
  }
  init_done_ = false;
  pin_accepted_ = false; // until proven otherwise, pin is required
  registered_ = false;
}

void A7670Component::update() {
  ESP_LOGD(TAG, "UPD: %d (%d)", state_, expect_ack_);
  if (state_ != STATE_IDLE || expect_ack_ || call_state_ != CALL_DISCONNECTED)
    return;

  // give modem a bit of time on boot
  if (init_delay > millis()) {
    return;
  }

  if (debug_) {
    // stay silent during debug mode
    return;
  }
  ESP_LOGD(TAG, "*** Peforming periodic network checks");
  _periodic_check_();
}

void A7670Component::loop() {
  uint32_t now = millis();

  bool waiting = state_ == STATE_MODEM_WAIT || state_ == STATE_PIN_WAIT;

  // unblink power if needed
  if (power_cycle_ > 0 && power_cycle_ + power_blink < now) {
    power_pin_->digital_write(false);
    power_cycle_ = 0;
  }

  // modem initialization
  if (state_ == STATE_MODEM_WAIT) {
    
    if (modem_ready_) {
      ESP_LOGI(TAG, "Modem initialized");
      last_tx_ = now;
      state_ = STATE_IDLE;
      return;
    }

    if (last_tx_ + modem_init_timeout < now) {
      ESP_LOGW(TAG, "Modem failed to initialize in timely manner");
      state_ = STATE_IDLE;
      last_tx_ = now;
      return;
    }

  } else {

    if (last_tx_ > 0 && last_rx_ == 0 && last_tx_ + rx_timeout < now) {
      modem_ready_ = false;

      if (power_pin_ == nullptr) {
        ESP_LOGE(TAG, "Modem not responding");
        last_tx_ = now;
        return;
      }

      ESP_LOGW(TAG, "Modem not responding, power cycling it (this is fine on cold boot)");
      power_pin_->digital_write(true); // blink power
      power_cycle_ = now;
      expect_ack_ = false;
      state_ = STATE_MODEM_WAIT;
      return;
    }

    if (!debug_ && !waiting && last_tx_ > 0 && last_tx_ + idle_timeout < now) {
      send_cmd_ctrlz_("", STATE_IDLE);
      return;
    }

  }

  // Read message
  if (available()) {
    while (available()) {
      uint8_t byte;
      read_byte(&byte);

      if (read_pos_ == READ_BUFFER_LENGTH)
        read_pos_ = 0;

      ESP_LOGVV(TAG, "Buffer pos: %u %d", read_pos_, byte);  // NOLINT

      if (byte == CR)
        continue;
      if (byte >= 0x7F)
        byte = '?';  // need to be valid utf8 string for log functions.
      read_buffer_[read_pos_] = byte;

      if (read_buffer_[read_pos_] == LF) {
        read_buffer_[read_pos_] = 0;
        read_pos_ = 0;
        last_rx_ = millis();
        parse_cmd_(read_buffer_);
      } else {
        read_pos_++;
      }
    }
    return;
  }

  // don't send anything new while we're expecting a response
  if (expect_ack_)
    return;

  // allowed in only in idle, or in debug mode
  if (state_ == STATE_IDLE || debug_) {
    if (send_at_pending_) {
      send_at_pending_ = false;
      send_cmd_ack_(at_, STATE_IDLE);
      return;
    }
  }

  // don't react to pin notifications before init to ensure we only enter pin once
  if (state_ == STATE_PIN_WAIT) {
    if (pin_required_) {
      pin_required_ = false;
      if (pin_code_.empty()) {
        ESP_LOGE(TAG, "No pin_code configured");
      } else {
        ESP_LOGD(TAG, "Entering PIN");
        send_cmd_ack_("AT+CPIN=" + pin_code_, STATE_PIN_WAIT);
      }
    } else if (pin_accepted_) {
      _periodic_check_();
    }
    return;
  }

  // if waiting for network registration
  if (state_ == STATE_REG_WAIT) {
    if (registration_required_) {
      registration_required_ = false;
      send_cmd_ack_("AT+CREG=1", STATE_IDLE);
    } else if (registered_) {
      _check_csq_();
    }
    return;
  }

  // only perform user requested actions during idle time
  if (state_ != STATE_IDLE)
    return;

  // network operations follow
  if (!registered_)
    return;

  if (connect_pending_) {
    connect_pending_ = false;
    if (call_state_ == CALL_ACTIVE) {
      return;
    }
    if (call_state_ != CALL_INCOMING) {
      ESP_LOGW(TAG, "No incoming call in progress");
      return;
    }
    ESP_LOGD(TAG, "Connecting...");
    send_cmd_ack_("ATA", STATE_IDLE);
    return;
  }

  if (disconnect_pending_) {
    disconnect_pending_ = false;
    if (call_state_ == CALL_DISCONNECTED) {
      return;
    }
    ESP_LOGD(TAG, "Disconnecting...");
    send_cmd_ack_("AT+CHUP", STATE_IDLE); 
    return;
  }

  if (dial_pending_) {
    dial_pending_ = false;
    if (call_state_ != CALL_DISCONNECTED) {
      ESP_LOGW(TAG, "Call is already in progress");
      return;
    }

    _set_gsm_charset_("GSM", STATE_DIALING1);
    return;
  }

  // call in progress
  if (call_state_ != CALL_DISCONNECTED) {
    // don't spam call checks too often
    const uint32_t now = millis();
    if (last_call_check_ + call_check_interval < now) {
      last_call_check_ = now;
      send_cmd_ack_("AT+CLCC", STATE_IDLE);
    }
    return;
  }

  // message rx/tx will happen after the call is completed
  if (incoming_message_pending_) {
    incoming_message_pending_ = false;
    _check_sms_messages_();
    return;
  }

  if (send_pending_) {
    send_pending_ = false;
    _set_gsm_charset_("GSM", STATE_SENDING_SMS);
    return;
  }

  if (message_cleanup_pending_) {
    // Serial Buffer should have flushed.
    // Send cmd to delete received sms
    send_cmd_ack_("AT+CMGD=" + std::to_string(parse_index_), STATE_IDLE);
    if (--parse_index_ == 0) {
      message_cleanup_pending_ = false;
      state_ = STATE_CHECK_SMS1;
    }
    return;
  }

  if (send_ussd_pending_) {
    send_ussd_pending_ = false;
    _set_msg_mode_(1, STATE_SEND_USSD1);
    return;
  }
}

void A7670Component::parse_cmd_(const std::string &message) {
  if (message.empty())
    return;

  ESP_LOGD(TAG, "<: %s - %d (%d)", message.c_str(), state_, expect_ack_);

  // check state notifications
  if (message == "*ATREADY: 1") {
    modem_ready_ = true;
    return;
  }
  if (message == "PB DONE") {
    return;
  }

  // don't react to pin notifications before init
  if (message.compare(0, 6, "+CPIN:") == 0 && init_done_) {
    std::string code = message.substr(7, message.length() - 7);
    bool accepted = (code == "READY");
    if (accepted) {
      if (!pin_code_.empty() && !pin_accepted_) {
        ESP_LOGD(TAG, "PIN accepted");
      }
    } else {
      if (code == "SIM PIN") {
        ESP_LOGD(TAG, "PIN required");
      } else {
        ESP_LOGE(TAG, "Unsupported PIN entry required: %s", code.c_str());
      }
    }
    pin_accepted_ = accepted;
    pin_required_ = !pin_accepted_;
    return;
  }

  if (message.compare(0, 6, "+CREG:") == 0) {
  // Response: "+CREG: 0,1" -- the one there means registered ok
  //           "+CREG: -,-" means not registered ok
    if (message.length() < 10) {
      // skip "+CREG: 1" as a response to AT+CREG=1
      return;
    }
    bool registered = (message[9] == '1' || message[9] == '5');
    if (registered) {
      if (!registered_) {
        ESP_LOGI(TAG, "Registered on the network");
      }
    } else {
      if (registered_) {
        ESP_LOGW(TAG, "Network registration failed");
      }
      if (message[7] == '0') {  // Network registration is disable, enable it
        registration_required_ = true;
      }
    }
    set_registered_(registered);
    return;
  }

  // rssi info
  if (message.compare(0, 5, "+CSQ:") == 0) {
    size_t comma = message.find(',', 6);
    if (comma != 6) {
      int rssi = parse_number<int>(message.substr(6, comma - 6)).value_or(0);

#ifdef USE_SENSOR
      if (rssi_sensor_ != nullptr) {
        rssi_sensor_->publish_state(rssi);
      } else {
        ESP_LOGD(TAG, "RSSI: %d", rssi);
      }
#else
      ESP_LOGD(TAG, "RSSI: %d", rssi);
#endif
    }
    return;
  }
  
  // new message available
  if (message.compare(0, 6, "+CMTI:") == 0 && !debug_) {
    incoming_message_pending_ = true; // read messages asap
    return;
  }

  if (message.compare(0, 6, "+CUSD:") == 0) {
    // Incoming USSD MESSAGE
    ussd_ = parse_item_(message, 7, 2);
    ussd_ = from_hex_(ussd_);
    ESP_LOGD(TAG, "Received USSD message:");
    ESP_LOGD(TAG, ussd_.c_str());
    ussd_received_callback_.call(ussd_);
    return;
  }

  if (message.compare(0, 6, "+CLCC:") == 0) {
    // item 1 call index for +CHLD
    // item 2 dir 0 Mobile originated; 1 Mobile terminated
    // item 3 stat
    // item 6 caller_id
    CallState new_call_state = static_cast<CallState>(parse_number<uint8_t>(parse_item_(message, 7, 3)).value_or(6));
    std::string caller_id = parse_item_(message, 7, 6);

    if (new_call_state != call_state_) {
      ESP_LOGD(TAG, "Call state is now: %d", new_call_state);
      call_state_ = new_call_state;
      if (new_call_state == CALL_INCOMING) {
        ESP_LOGD(TAG, "Incoming call from %s", caller_id.c_str());
        incoming_call_callback_.call(caller_id);
      } else if (new_call_state == CALL_ACTIVE) {
        ESP_LOGD(TAG, "Call connected");
        call_connected_callback_.call();
      } else if (new_call_state == CALL_DISCONNECTED) {
        ESP_LOGD(TAG, "Call disconnected");
        call_disconnected_callback_.call();
      }
    }
    return;
  }

  if (message.compare(0, 6, "+CLIP:") == 0) {
    return;
  }
  if (message == "RING") {
    return;
  }
  if (message == "NO CARRIER") {
    return;
  }

  if (message == current_cmd_) {
    // echo is not disabled yet
    return;
  }

  bool ok = message == "OK";
  if (expect_ack_) {
    std::string cmd = current_cmd_;
    current_cmd_ = "";
    expect_ack_ = false;
    
    if (message == "ERROR") {
      ESP_LOGW(TAG, "Received ERROR from command %s", cmd.c_str());
      state_ = STATE_IDLE;
      return;
    }

    if (message.compare(0, 11, "+CME ERROR:") == 0) {
      ESP_LOGE(TAG, "Received CME ERROR from command %s: %s", cmd.c_str(), message.substr(12, message.length() - 12).c_str());
      //state_ = STATE_IDLE;
      return;
    }

    if (!ok) {
      ESP_LOGD(TAG, "Not ack. %d %s", state_, message.c_str());
      state_ = STATE_IDLE;  // Let it timeout
      return;
    }
  }

  switch (state_) {
    case STATE_IDLE: {
      // not expecting any solicited responses
      break;
    }
    case STATE_MODEM_WAIT:
    case STATE_REG_WAIT:
    case STATE_PIN_WAIT:
      // these are handled separately in loop()
      break;

    case STATE_MODEM_DETECT:
      // A7670E-LASE
      // SIMCOM_SIM868
      if (message.compare(0, 13, "SIMCOM_SIM868") == 0) {
        ESP_LOGD(TAG, "SIM8XX modem detected");
        modem_type_ = SIM8XX;
      }
      expect_ack_ = true;
      state_ = STATE_INIT2;
      break;

    case STATE_INIT:
      init_done_ = true;
      send_cmd_("AT+CGMM", STATE_MODEM_DETECT);
      break;
    case STATE_INIT2:
      send_cmd_ack_("AT+CPIN?", STATE_PIN_WAIT);
      break;

    case STATE_CSQ:
      _check_csq_();
      break;
    // case STATE_SETUP_CLIP:
    //   send_cmd_ack_("AT+CLIP=1", STATE_SETUP_USSD);
    //   break;
    // case STATE_SETUP_USSD:
    //   send_cmd_ack_("AT+CUSD=1", STATE_CHECK_SMS);
    //   break;

    case STATE_DIALING1:
      send_cmd_("ATD" + recipient_ + ';', STATE_DIALING2);
      break;
    case STATE_DIALING2:
      if (ok) {
        ESP_LOGD(TAG, "Dialing: '%s'", recipient_.c_str());
        state_ = STATE_IDLE;
      } else {
        _error_out_();
      }
      break;

    case STATE_CHECK_SMS:
      _check_sms_messages_();
      break;
    case STATE_CHECK_SMS1:
      _list_sms_messages_();
      break;
    case STATE_PARSE_SMS_RESPONSE:
      if (message.compare(0, 6, "+CMGL:") == 0) {
        _parse_sms_header_(message);
        state_ = STATE_RECEIVE_SMS;
      } else if (ok) {
        // Otherwise we receive another OK
        send_cmd_ack_("AT+CLCC", STATE_IDLE);
      }
      break;
    case STATE_RECEIVE_SMS: {
      if (message.compare(0, 6, "+CMGL:") == 0) {
        _parse_sms_header_(message);
      } else if (ok) {
        _handle_sms_message_();
        message_cleanup_pending_ = true;
        state_ = STATE_IDLE;
      } else {
        // actual message body
        if (parse_stat_ == 0) { // only unread messages
          _parse_sms_message_(message);
        }
      }
      break;
    }
    case STATE_SENDING_SMS:
      _send_sms_get_csca_();
      break;
    case STATE_SENDING_SMS_1:
      _send_sms_receive_csca_(message);
      break;
    case STATE_SENDING_SMS_2:
      _prepare_sms_multipart_();
      // set pdu mode
      _set_msg_mode_(0, STATE_SENDING_SMS_3);
      break;
    case STATE_SENDING_SMS_3:
      _send_sms_send_header_();
      break;
    case STATE_SENDING_SMS_4:
      _send_sms_send_body_(message);
      break;
    case STATE_SENDING_SMS_5:
      if (message.compare(0, 6, "+CMGS:") == 0) {
        ESP_LOGD(TAG, "SMS Sent OK: %s", message.c_str());

        if (outgoing_splits_.size() > 0) {
          if (outgoing_partnum_ <= outgoing_splits_.size()) {
            // continue sending message parts
            state_ = STATE_SENDING_SMS_3;
            expect_ack_ = true;
            break; 
          }
          outgoing_csms_ = 0;
          outgoing_partnum_ = 0;
          outgoing_force_16bit_ = false;
          outgoing_splits_.clear();
        }

        sms_sent_callback_.call(outgoing_message_, recipient_);
        state_ = STATE_CHECK_SMS;
        expect_ack_ = true;
      }
      break;
    case STATE_SEND_USSD1:
      // set text mode
      //_set_gsm_charset_("GSM", STATE_SEND_USSD2);
      send_cmd_ack_("AT+CSCS=?", STATE_SEND_USSD2);
      break;
    case STATE_SEND_USSD2: {
      send_cmd_ack_("AT+CUSD=1,\"" + to_hex_(ussd_) + "\"", STATE_SEND_USSD3);
      break;
    }
    case STATE_SEND_USSD3:
      if (ok) {
        // Dialing
        ESP_LOGD(TAG, "Sending ussd code: %s", ussd_.c_str());
        state_ = STATE_IDLE;
      } else {
        _error_out_();
      }
      break;
    default:
      ESP_LOGD(TAG, "Unhandled: %s - %d", message.c_str(), state_);
      break;
  }
}

void A7670Component::_periodic_check_() {
  if (!init_done_) {
    send_cmd_ack_("ATE0", STATE_INIT);
    return;
  }
  send_cmd_ack_("AT+CREG?", STATE_REG_WAIT);
}

void A7670Component::_check_csq_() {
  send_cmd_ack_("AT+CSQ", STATE_CHECK_SMS);
}

void A7670Component::_set_gsm_charset_(const std::string &alphabet, a7670::State state) {
  send_cmd_ack_("AT+CSCS=\"" + alphabet + "\"", state);
}

void A7670Component::_set_msg_mode_(uint8_t mode, a7670::State state) {
  send_cmd_ack_("AT+CMGF=" + std::to_string(mode), state);
}

void A7670Component::_send_sms_get_csca_() {
  send_cmd_("AT+CSCA?", STATE_SENDING_SMS_1); // find out service center address
}

void A7670Component::_send_sms_receive_csca_(const std::string &message) {
  if (message.compare(0, 6, "+CSCA:") == 0) {
    pdu_.setSCAnumber(parse_item_(message, 7, 1).c_str());
    expect_ack_ = true;
    state_ = STATE_SENDING_SMS_2;
  }
}

void A7670Component::_prepare_sms_multipart_() {
  std::u32string msg_enc = utf8ToUcs4_(outgoing_message_);
  std::size_t idx = 0;
  int consumed;
  bool msg16Bit = false;
  const char* msg = outgoing_message_.c_str();
  outgoing_splits_.clear();
  while((consumed = pdu_.checkMultipart(recipient_.c_str(), msg, idx, &msg16Bit)) != -1) {
    //ESP_LOGW(TAG, "Consumed %d", consumed);
    std::size_t splitIdx = idx + consumed;
    //std::string str = outgoing_message_.substr(idx, consumed);
    //ESP_LOGW(TAG, "Split msg at %d: %s", splitIdx, str.c_str());

    outgoing_splits_.emplace_back(splitIdx);
    if (idx == splitIdx) {
      ESP_LOGW(TAG, "Error splitting at %d", idx);
      break;
    }
    idx = splitIdx;
  };
  if (outgoing_splits_.size() > 0) {
    outgoing_csms_ = outgoing_csms_counter_++;
    outgoing_partnum_ = 0;
    outgoing_force_16bit_ = msg16Bit;
  }
}

void A7670Component::_send_sms_send_header_() {
  int encode_result;
  if (outgoing_splits_.size() == 0) {
    // just send the message
    encode_result = pdu_.encodePDU(recipient_.c_str(), outgoing_message_.c_str());
  } else {
    std::size_t begin = outgoing_partnum_ == 0 ? 0 : *std::next(outgoing_splits_.begin(), outgoing_partnum_-1);
    std::size_t end = outgoing_partnum_ >= outgoing_splits_.size() ? outgoing_message_.length() : *std::next(outgoing_splits_.begin(), outgoing_partnum_);
    std::string part = outgoing_message_.substr(begin, end - begin);

    outgoing_partnum_++;

    ESP_LOGI(TAG, "Sending sms %d part %d of %d: %s", outgoing_csms_, outgoing_partnum_, outgoing_splits_.size() + 1, part.c_str());

    encode_result = pdu_.encodePDU(
      recipient_.c_str(), 
      part.c_str(),
      outgoing_csms_,
      outgoing_splits_.size() + 1,
      outgoing_partnum_,
      outgoing_force_16bit_
    );
  }

  if (encode_result < 0) {
    ESP_LOGW(TAG, "Error encoding sms: %d", encode_result);
    state_ = STATE_IDLE;
    return;
  }
  send_cmd_("AT+CMGS=" + std::to_string(encode_result), STATE_SENDING_SMS_4);
  if (modem_type_ == SIM8XX) {
    _send_sms_send_body_("> ");
  }
}

void A7670Component::_send_sms_send_body_(const std::string &message) {
  if (message == "> ") {
    send_cmd_ctrlz_(pdu_.getSMS(), STATE_SENDING_SMS_5);
  }
}

void A7670Component::_check_sms_messages_() {
  // set pdu mode
  _set_msg_mode_(0, STATE_CHECK_SMS1);
}

void A7670Component::_list_sms_messages_() {
  send_cmd_("AT+CMGL=4", STATE_PARSE_SMS_RESPONSE); // 4: all messages
  parse_index_ = 0;
}

void A7670Component::_parse_sms_header_(const std::string &message) {
  parse_index_ = parse_number<uint8_t>(parse_item_(message, 7, 1)).value_or(0);
  parse_stat_ = parse_number<uint8_t>(parse_item_(message, 7, 2)).value_or(0);
}

void A7670Component::_parse_sms_message_(const std::string &message) {
  pdu_.decodePDU(message.c_str());
  int* concat = pdu_.getConcatInfo();
  ESP_LOGD(TAG, "SMS concat: CMCS=%d, part %d of %d", concat[0], concat[1], concat[2]);
  if (cmcs_ == 0 || cmcs_ != concat[0]) {
    // handle any previously cached message, if it existed
    _handle_sms_message_();
  }
  cmcs_ = concat[0];
  sender_ = pdu_.getSender();
  message_ += pdu_.getText();
}

void A7670Component::_handle_sms_message_() {
  if (message_.length() > 0) {
    ESP_LOGD(TAG, "Received SMS from: %s", sender_.c_str());
    ESP_LOGD(TAG, "%s", message_.c_str());
    sms_received_callback_.call(message_, sender_);
  }
  message_ = "";
  sender_ = "";
  cmcs_ = 0;
}

void A7670Component::_error_out_() {
  set_registered_(false);
  send_cmd_ctrlz_("AT+CMEE=2", STATE_IDLE);
}

void A7670Component::send_cmd_ack_(const std::string &message, a7670::State state) {
  send_cmd_(message, state);
  expect_ack_ = true;
  current_cmd_ = message;
}

void A7670Component::send_cmd_(const std::string &message, a7670::State state) {
  ESP_LOGD(TAG, "->>: %s - %d", message.c_str(), state_);
  current_cmd_ = "";
  write_str(message.c_str());
  write_byte(CR);
  write_byte(LF);
  last_tx_ = millis();
  state_ = state;
}

void A7670Component::send_cmd_ctrlz_(const std::string &message, a7670::State state) {
  ESP_LOGD(TAG, "->>: %s<ctrl-z> - %d", message.c_str(), state_);
  current_cmd_ = "";
  write_str(message.c_str());
  write(CTRLZ);
  last_tx_ = millis();
  state_ = state;
}

void A7670Component::send_sms(const std::string &recipient, const std::string &message) {
  recipient_ = recipient;
  outgoing_message_ = message;
  send_pending_ = true;
}

void A7670Component::send_ussd(const std::string &ussd_code) {
  ESP_LOGD(TAG, "Sending USSD code: %s", ussd_code.c_str());
  ussd_ = ussd_code;
  send_ussd_pending_ = true;
}

void A7670Component::send_at(const std::string &command) {
  ESP_LOGD(TAG, "Sending AT command: %s", command.c_str());
  at_ = command;
  send_at_pending_ = true;
}

void A7670Component::dial(const std::string &recipient) {
  recipient_ = recipient;
  dial_pending_ = true;
}

void A7670Component::connect() { 
  connect_pending_ = true;
}

void A7670Component::disconnect() {
  disconnect_pending_ = true;
}

std::string A7670Component::parse_item_(const std::string &data, size_t from, uint8_t item) {
  size_t start = from;
  size_t end = data.find(',', start);
  uint8_t cur_item = 0;
  while (end != start) {
    cur_item++;
    if (cur_item == item) {
      std::string found = data.substr(start, end - start);
      if (found[0] == '"' && found[found.length() - 1] == '"') {
        found = found.substr(1, found.length() - 2);
      }
      return found;
    }
    start = end + 1;
    end = data.find(',', start);
  }
  return "";
}

std::string A7670Component::to_hex_(const std::string &data) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (int i = 0; i < data.length(); i++) {
    ss << std::uppercase << std::setw(2) << static_cast<unsigned>(data[i]);
  }
  return ss.str();
}

std::string A7670Component::from_hex_(const std::string &data) {
  std::string result; 
  result.reserve(data.size() / 2);
   for (size_t i = 0; i < data.length(); i += 2) { 
    std::string byteString = data.substr(i, 2); 
    char byte = static_cast<char>(std::stoul(byteString, nullptr, 16)); 
    result.push_back(byte); 
  } 
  return result; 
}

std::string A7670Component::ucs4ToUtf8_(const std::u32string& in)
{
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    return conv.to_bytes(in);
}

std::u32string A7670Component::utf8ToUcs4_(const std::string& in)
{
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    return conv.from_bytes(in);
}

}  // namespace a7670
}  // namespace esphome
