#include "acl_server.h"

#include "esphome/core/log.h"

namespace esphome {
namespace acl {

static const char *const TAG = "acl_serve";

void AclServer::stop() {
  if (this->server_) {
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }
}

void AclServer::start(uint16_t port) {
  stop();

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  config.ctrl_port = 16384;
  config.max_open_sockets = 3;
  config.uri_match_fn = [](const char * /*unused*/, const char * /*unused*/, size_t /*unused*/) { return true; };
  ESP_LOGI(TAG, "Starting server on port %d", port);

  if (httpd_start(&this->server_, &config) == ESP_OK) {
    const httpd_uri_t get_handler = {
        .uri = "",
        .method = HTTP_GET,
        .handler = AclServer::handle_get,
        .user_ctx = this,
    };
    const httpd_uri_t post_handler = {
        .uri = "",
        .method = HTTP_POST,
        .handler = AclServer::handle_post,
        .user_ctx = this,
    };
    httpd_register_uri_handler(this->server_, &get_handler);
    httpd_register_uri_handler(this->server_, &post_handler);
  }
}

esp_err_t AclServer::handle_get(httpd_req_t *r) {
  std::string url = r->uri;

  AclServer *server = static_cast<AclServer *>(r->user_ctx);
  if (url == "/" + server->path_ + "/acl.json") {
    return server->acl_get(r);
  } else if(url.compare(0, 7 + server->path_.length(), "/" + server->path_ + "/logs/") == 0 && url.compare(url.length() - 4, url.length(), ".log") == 0) {
    std::string logfile = url.substr(7 + server->path_.length(), url.length() - 4 - 7 - server->path_.length());
    return server->logs_get(r, logfile);
  }
  httpd_resp_set_status(r, HTTPD_404);
  httpd_resp_send(r, "Not found", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t AclServer::handle_post(httpd_req_t *r) {
  std::string url = r->uri;
  std::string post_body;
  if (r->content_len > 0) {
    ESP_LOGI(TAG, "Receiving %d bytes", r->content_len);
    post_body.resize(r->content_len);
    int read = 0;
    while (read < r->content_len) {
      const int ret = httpd_req_recv(r, &post_body[read], r->content_len + 1 - read);
      if (ret <= 0) {  // 0 return value indicates connection closed
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
          httpd_resp_send_err(r, HTTPD_408_REQ_TIMEOUT, nullptr);
          return ESP_ERR_TIMEOUT;
        }
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, nullptr);
        return ESP_FAIL;
      }
      read += ret;
    }
  }
  ESP_LOGI(TAG, "Received %d bytes", post_body.length());

  AclServer *server = static_cast<AclServer *>(r->user_ctx);
  if (url == "/" + server->path_ + "/acl.json") {
    return server->acl_post(r, post_body);
  }
  httpd_resp_set_status(r, HTTPD_404);
  httpd_resp_send(r, "Not found", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t AclServer::logs_get(httpd_req_t *r, const std::string &logfile) {
  optional<std::string> res = store_->load_log_content(logfile);
  if (!res.has_value()) {
    httpd_resp_set_status(r, HTTPD_404);
    httpd_resp_send(r, "Not found", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  httpd_resp_set_hdr(r, "Content-Type", "text/plain");
  httpd_resp_set_hdr(r, "Connection", "close");
  httpd_resp_set_status(r, HTTPD_200);
  httpd_resp_send(r, res.value().c_str(), HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t AclServer::acl_get(httpd_req_t *r) {
  optional<std::string> res = store_->load_acl_content();
  if (!res.has_value()) {
    httpd_resp_set_status(r, HTTPD_404);
    httpd_resp_send(r, "Not found", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  httpd_resp_set_hdr(r, "Content-Type", "application/json");
  httpd_resp_set_hdr(r, "Connection", "close");
  httpd_resp_set_status(r, HTTPD_200);
  httpd_resp_send(r, res.value().c_str(), HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t AclServer::acl_post(httpd_req_t *r, const std::string &post_body) {
  store_->store_acl_content(post_body);
  reload_();
  httpd_resp_set_hdr(r, "Connection", "close");
  httpd_resp_set_status(r, HTTPD_200);
  httpd_resp_send(r, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

bool AclServer::request_has_header(httpd_req_t *req, const char *name) { return httpd_req_get_hdr_value_len(req, name); }

optional<std::string> AclServer::request_get_header(httpd_req_t *req, const char *name) {
  size_t len = httpd_req_get_hdr_value_len(req, name);
  if (len == 0) {
    return {};
  }

  std::string str;
  str.resize(len);

  auto res = httpd_req_get_hdr_value_str(req, name, &str[0], len + 1);
  if (res != ESP_OK) {
    return {};
  }

  return {str};
}

} // acl
} // esphome
