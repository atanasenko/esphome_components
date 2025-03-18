#pragma once

#include <utility>
#include <list>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "pdulib.h"

namespace esphome {
namespace a7670 {

const uint16_t READ_BUFFER_LENGTH = 1024;

enum BootState {
  BOOT_START = 0,
  BOOT_POWERON,
  BOOT_DONE,
};

enum ModemType {
  A76XX,
  SIM8XX
};

enum State {
  STATE_IDLE = 0,
  STATE_INIT,
  STATE_INIT2,

  STATE_MODEM_WAIT,
  STATE_MODEM_DETECT,
  STATE_PIN_WAIT,
  STATE_REG_WAIT,

  STATE_CSQ,
  STATE_SETUP_CLIP,

  STATE_SENDING_SMS,
  STATE_SENDING_SMS_1,
  STATE_SENDING_SMS_2,
  STATE_SENDING_SMS_3,
  STATE_SENDING_SMS_4,
  STATE_SENDING_SMS_5,
  STATE_CHECK_SMS,
  STATE_CHECK_SMS1,
  STATE_PARSE_SMS_RESPONSE,
  STATE_RECEIVE_SMS,
  STATE_RECEIVED_SMS,
  STATE_DIALING1,
  STATE_DIALING2,
  STATE_SETUP_USSD,
  STATE_SEND_USSD1,
  STATE_SEND_USSD2,
  STATE_SEND_USSD3
};

enum CallState {
  CALL_ACTIVE = 0,
  CALL_HELD,
  CALL_DIALING,
  CALL_ALERTING,
  CALL_INCOMING,
  CALL_WAITING,
  CALL_DISCONNECTED
};

class A7670Component : public uart::UARTDevice, public PollingComponent {
 public:
  /// Retrieve the latest sensor values. This operation takes approximately 16ms.
  void dump_config() override;
  void setup() override;
  void loop() override;
  void update() override;
  void set_power_pin(GPIOPin* power_pin) { this->power_pin_ = power_pin; }
  void set_pin_code(std::string pin_code) { this->pin_code_ = pin_code; }

#ifdef USE_BINARY_SENSOR
  void set_registered_binary_sensor(binary_sensor::BinarySensor *registered_binary_sensor) {
    registered_binary_sensor_ = registered_binary_sensor;
  }
#endif
#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { rssi_sensor_ = rssi_sensor; }
#endif
  void add_on_sms_received_callback(std::function<void(std::string, std::string)> callback) {
    this->sms_received_callback_.add(std::move(callback));
  }
  void add_on_sms_sent_callback(std::function<void(std::string, std::string)> callback) {
    this->sms_sent_callback_.add(std::move(callback));
  }
  void add_on_sms_send_failed_callback(std::function<void(std::string, std::string)> callback) {
    this->sms_send_failed_callback_.add(std::move(callback));
  }
  void add_on_incoming_call_callback(std::function<void(std::string)> callback) {
    this->incoming_call_callback_.add(std::move(callback));
  }
  void add_on_call_connected_callback(std::function<void()> callback) {
    this->call_connected_callback_.add(std::move(callback));
  }
  void add_on_call_disconnected_callback(std::function<void()> callback) {
    this->call_disconnected_callback_.add(std::move(callback));
  }
  void add_on_ussd_received_callback(std::function<void(std::string)> callback) {
    this->ussd_received_callback_.add(std::move(callback));
  }
  void send_sms(const std::string &recipient, const std::string &message);
  void send_ussd(const std::string &ussd_code);
  void dial(const std::string &recipient);
  void connect();
  void disconnect();

  void set_debug(bool debug) { this->debug_ = debug; }
  void send_at(const std::string &command);

 protected:
  void send_cmd_(const std::string &message, a7670::State state);
  void send_cmd_ack_(const std::string &message, a7670::State state);
  void send_cmd_ctrlz_(const std::string &message, a7670::State state);

  void parse_cmd_(const std::string &message);
  std::string parse_item_(const std::string &data, size_t from, uint8_t item);

  void _periodic_check_();
  void _check_csq_();

  void _set_gsm_charset_(const std::string &message, a7670::State state);
  void _set_msg_mode_(uint8_t mode, a7670::State state);
  void _send_sms_get_csca_();
  void _send_sms_receive_csca_(const std::string &message);
  void _prepare_sms_multipart_();
  void _send_sms_send_header_();
  void _send_sms_send_body_(const std::string &message);

  void _check_sms_messages_();
  void _list_sms_messages_();
  void _parse_sms_header_(const std::string &message);
  void _parse_sms_message_(const std::string &message);
  void _handle_sms_message_();
  void _error_out_();

  std::string to_hex_(const std::string &data);
  std::string from_hex_(const std::string &data);
  std::string ucs4ToUtf8_(const std::u32string& in);
  std::u32string utf8ToUcs4_(const std::string& in);

  void set_registered_(bool registered) {
    this->registered_ = registered;
#ifdef USE_BINARY_SENSOR
    if (this->registered_binary_sensor_ != nullptr) {
      this->registered_binary_sensor_->publish_state(registered);
    }
#endif
  }

  GPIOPin *power_pin_{nullptr};
  std::string pin_code_;
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *registered_binary_sensor_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
#endif

  bool modem_ready_{false};
  uint32_t power_cycle_{0};

  bool pin_accepted_{false};
  bool pin_required_{false};
  bool registered_{false};
  bool registration_required_{false};
  bool init_done_{false};

  a7670::ModemType modem_type_{A76XX};
  a7670::State state_{STATE_IDLE};

  char read_buffer_[READ_BUFFER_LENGTH];
  size_t read_pos_{0};

  uint8_t parse_index_{0};
  uint8_t parse_stat_{0};
  uint32_t last_tx_{0};
  uint32_t last_rx_{0};
  
  bool expect_ack_{false};
  std::string current_cmd_;
  
  PDU pdu_{16384};
  int cmcs_;
  std::string sender_;
  std::string message_;
  std::string whole_message_;
  std::string recipient_;
  std::string outgoing_message_;

  std::list<std::size_t> outgoing_splits_;
  int outgoing_csms_counter_{1};
  int outgoing_csms_{0};
  int outgoing_partnum_{0}; // current part of message
  bool outgoing_force_16bit_{false};

  std::string ussd_;
  std::string sca_;
  std::string at_;
  bool send_pending_;
  bool dial_pending_;
  bool connect_pending_;
  bool disconnect_pending_;
  bool send_ussd_pending_;
  bool send_at_pending_;
  bool incoming_message_pending_;
  bool message_cleanup_pending_;
  bool debug_{false};
 
  a7670::CallState call_state_{CALL_DISCONNECTED};
  uint32_t last_call_check_{0};

  CallbackManager<void(std::string, std::string)> sms_received_callback_;
  CallbackManager<void(std::string, std::string)> sms_sent_callback_;
  CallbackManager<void(std::string, std::string)> sms_send_failed_callback_;
  CallbackManager<void(std::string)> incoming_call_callback_;
  CallbackManager<void()> call_connected_callback_;
  CallbackManager<void()> call_disconnected_callback_;
  CallbackManager<void(std::string)> ussd_received_callback_;
};

class A7670ReceivedMessageTrigger : public Trigger<std::string, std::string> {
 public:
  explicit A7670ReceivedMessageTrigger(A7670Component *parent) {
    parent->add_on_sms_received_callback(
        [this](const std::string &message, const std::string &sender) { this->trigger(message, sender); });
  }
};

class A7670SentMessageTrigger : public Trigger<std::string, std::string> {
  public:
   explicit A7670SentMessageTrigger(A7670Component *parent) {
     parent->add_on_sms_sent_callback(
         [this](const std::string &message, const std::string &recipient) { this->trigger(message, recipient); });
   }
 };
 
class A7670SendMessageFailedTrigger : public Trigger<std::string, std::string> {
  public:
   explicit A7670SendMessageFailedTrigger(A7670Component *parent) {
     parent->add_on_sms_send_failed_callback(
         [this](const std::string &error, const std::string &recipient) { this->trigger(error, recipient); });
   }
};
 

 class A7670IncomingCallTrigger : public Trigger<std::string> {
 public:
  explicit A7670IncomingCallTrigger(A7670Component *parent) {
    parent->add_on_incoming_call_callback([this](const std::string &caller_id) { this->trigger(caller_id); });
  }
};

class A7670CallConnectedTrigger : public Trigger<> {
 public:
  explicit A7670CallConnectedTrigger(A7670Component *parent) {
    parent->add_on_call_connected_callback([this]() { this->trigger(); });
  }
};

class A7670CallDisconnectedTrigger : public Trigger<> {
 public:
  explicit A7670CallDisconnectedTrigger(A7670Component *parent) {
    parent->add_on_call_disconnected_callback([this]() { this->trigger(); });
  }
};
class A7670ReceivedUssdTrigger : public Trigger<std::string> {
 public:
  explicit A7670ReceivedUssdTrigger(A7670Component *parent) {
    parent->add_on_ussd_received_callback([this](const std::string &ussd) { this->trigger(ussd); });
  }
};

template<typename... Ts> class A7670SendSmsAction : public Action<Ts...> {
 public:
  A7670SendSmsAction(A7670Component *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, recipient)
  TEMPLATABLE_VALUE(std::string, message)

  void play(Ts... x) {
    auto recipient = this->recipient_.value(x...);
    auto message = this->message_.value(x...);
    this->parent_->send_sms(recipient, message);
  }

 protected:
  A7670Component *parent_;
};

template<typename... Ts> class A7670SendUssdAction : public Action<Ts...> {
 public:
  A7670SendUssdAction(A7670Component *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, ussd)

  void play(Ts... x) {
    auto ussd_code = this->ussd_.value(x...);
    this->parent_->send_ussd(ussd_code);
  }

 protected:
  A7670Component *parent_;
};

template<typename... Ts> class A7670SendAtAction : public Action<Ts...> {
 public:
  A7670SendAtAction(A7670Component *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, command)

  void play(Ts... x) {
    auto command = this->command_.value(x...);
    this->parent_->send_at(command);
  }

 protected:
  A7670Component *parent_;
};

template<typename... Ts> class A7670DialAction : public Action<Ts...> {
 public:
  A7670DialAction(A7670Component *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, recipient)

  void play(Ts... x) {
    auto recipient = this->recipient_.value(x...);
    this->parent_->dial(recipient);
  }

 protected:
  A7670Component *parent_;
};
template<typename... Ts> class A7670ConnectAction : public Action<Ts...> {
 public:
  A7670ConnectAction(A7670Component *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->connect(); }

 protected:
  A7670Component *parent_;
};

template<typename... Ts> class A7670DisconnectAction : public Action<Ts...> {
 public:
  A7670DisconnectAction(A7670Component *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->disconnect(); }

 protected:
  A7670Component *parent_;
};

template<typename... Ts> class A7670DebugOnAction : public Action<Ts...> {
 public:
  A7670DebugOnAction(A7670Component *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->set_debug(true); }

 protected:
  A7670Component *parent_;
};

template<typename... Ts> class A7670DebugOffAction : public Action<Ts...> {
 public:
  A7670DebugOffAction(A7670Component *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->set_debug(false); }

 protected:
  A7670Component *parent_;
};



}  // namespace a7670
}  // namespace esphome
