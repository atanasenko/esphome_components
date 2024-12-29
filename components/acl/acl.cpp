#include "acl.h"
#include "util.h"

#include <sstream>
#include <iomanip>

#include "esphome/core/log.h"

namespace esphome {
namespace acl {

static const char *const TAG = "acl";

void AclComponent::dump_config() {
}

void AclComponent::setup() {
  store_.set_path(path_);
  if (sdmmc_ != nullptr) {
    store_.set_sdfs(sdmmc_->fs());
  }
  //webserver_->add_handler(this);
  //server_.set_acl(this);
  server_.set_path(path_);
  server_.set_store(&store_);
  server_.set_reload([this]() -> void {
    this->reload_acl();
  });
  reload_acl();
}

void AclComponent::start_server() {
  server_.start(88);
}

void AclComponent::loop() {
  //uint32_t now = millis();
  if (!server_started_) {
    server_started_ = true;
    start_server();
  }
 
  if (store_required_) {
    store_required_ = false;
    store_acl_();
    load_acl_();
    return;
  }
  if (reload_required_) {
    reload_required_ = false;
    load_acl_();
    return;
  }
  if (!pending_logs_.empty()) {
    store_logs_();
    pending_logs_.clear();
    return;
  }
}

optional<AclEntry*> AclComponent::check(const std::string &key) {
  auto res = acl_.find(key);
  if (res == acl_.end()) {
    ESP_LOGI(TAG, "*** [%s] ACL <UNAUTHORIZED>: %s", path_.c_str(), key.c_str());
    append_log(string_format("<UNAUTHORIZED>: %s", path_.c_str(), key.c_str()));
    return {};
  }
  ESP_LOGI(TAG, "*** [%s] ACL %s: %s", path_.c_str(), res->second.name.c_str(), key.c_str());
  append_log(string_format("%s: %s", res->second.name.c_str(), key.c_str()));
  return &res->second;
}

void AclComponent::append_log(const std::string &message) {
  LogEntry log(timestamp_(), std::move(message));
  pending_logs_.insert(pending_logs_.end(), std::move(log));
}

void AclComponent::add_acl(const std::string &name, const std::string &key) {
  AclEntry entry(name, key);
  acl_.emplace(key, std::move(entry));
  ESP_LOGI(TAG, "[%s] ACL added name=%s, key=%s", path_.c_str(), name.c_str(), key.c_str());
  reload_acl();
}

void AclComponent::remove_acl(const std::string &name) {
  for(auto it = acl_.begin(); it != acl_.end();) {
    if(it->second.name == name) {
      ESP_LOGI(TAG, "[%s] ACL removed name=%s, key=%s", path_.c_str(), it->second.name.c_str(), it->second.key.c_str());
      it = acl_.erase(it);
      reload_acl();
    } else {
      ++it;
    }
  }
}

void AclComponent::clear_acl() {
  if (!acl_.empty()) {
    acl_.clear();
    ESP_LOGI(TAG, "[%s] ACL cleared", path_.c_str());
    reload_acl();
  }
}

void AclComponent::print_acl() {
  if (acl_.empty()) {
    ESP_LOGI(TAG, "[%s] ACL list empty", path_.c_str());  
    return;
  }
  ESP_LOGI(TAG, "[%s] ACL list:", path_.c_str());
  uint16_t i = 0;
  for (auto const& entry: acl_) {
    ESP_LOGI(TAG, "%d: name=%s, key=%s", ++i, entry.second.name.c_str(), entry.first.c_str());
  }
}

void AclComponent::reload_acl() {
  reload_required_ = true;
}

void AclComponent::load_acl_() {
  auto part_data = store_.load_acl();
  ESP_LOGI(TAG, "[%s] Reloaded ACL from acl.json", path_.c_str());
  acl_.clear();
  for (auto const& entry: part_data) {
    acl_.emplace(entry.key, entry);
  }
}

void AclComponent::store_acl_() {
  std::list<AclEntry> to_store;
  for (auto const& pair: acl_) {
    to_store.insert(to_store.end(), pair.second);
  }
  store_.store_acl(to_store);
}

void AclComponent::store_logs_() {
  store_.store_logs(pending_logs_);
}

std::string AclComponent::timestamp_() {
  if (clock_ == nullptr) {
    return ("" + millis());
  }
  return clock_->now().strftime("%Y-%m-%d %H:%M:%S");
}

/*
bool AclComponent::canHandle(AsyncWebServerRequest *request) { 
  const std::string url = request->url().c_str();
  if (request->method() == HTTP_GET && url == "/acl/acl.json") {
    return true;
  }
  if (request->method() == HTTP_POST && url == "/acl/acl.json") {
    return true;
  }
  if (request->method() == HTTP_GET && url.length() > 14 && url.compare(0, 10, "/acl/logs/") == 0 && url.compare(url.length() - 4, 4, ".log") == 0) {
    return true;
  }
  return false;
}

void AclComponent::handleRequest(AsyncWebServerRequest *request) {
  const std::string url = request->url().c_str();
  if (request->method() == HTTP_GET && url == "/acl/acl.json") {
    optional<std::string> content = store_.load_acl_content();
    if (!content.has_value()) {
      request->send(404);
      return;
    }
    request->send(200, "application/json", content->c_str());
    return;
  }

  if (request->method() == HTTP_POST && url == "/acl/acl.json") {
    //ESP_LOGW(TAG, "Handling acl.json post from %s", request->client()->remoteIP().toString().c_str());
    ESP_LOGW(TAG, "Handling acl.json post");
    std::string *data = (std::string*) _tempObject;
    if (data == nullptr) {
      request->send(404);  
      return;
    }
    _tempObject = nullptr;
    store_.store_acl_content(*data);
    delete data;
    reload_acl();
    request->send(201);
    return;
  }

  if (request->method() == HTTP_GET && url.compare(0, 10, "/acl/logs/") == 0) {
    std::string period = url.substr(10, url.length() - 10 - 4);
    optional<std::string> content = store_.load_log_content(period);
    if (!content.has_value()) {
      request->send(404);
      return;
    }
    request->send(200, "text/plain", content->c_str());
    return;
  }
  request->send(404);
}

void AclComponent::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  ESP_LOGI(TAG, "handleBody: %d, %d, %d", len, index, total);
  std::string *result;
  if (_tempObject == nullptr) {
    result = new std::string;
    _tempObject = result;
  } else {
    result = (std::string*) _tempObject;
  }
  result->append(data, data+len);
  ESP_LOGI(TAG, "handleBody2: %d", result->length());
}
*/

}  // namespace acl
}  // namespace esphome
