#pragma once

#include <string>
#include "acl_store.h"

#include <esp_http_server.h>

namespace esphome {
namespace acl {

class AclServer {
  public:
    ~AclServer() { this->stop(); }

    void set_path(const std::string &path) { path_ = path; }
    void set_store(AclStore *store) { this->store_ = store; }
    void set_reload(std::function<void()> reload) { this->reload_ = reload; }

    void start(uint16_t port);
    void stop();

  protected:
    std::string path_;
    AclStore *store_;
    std::function<void()> reload_;

    httpd_handle_t server_{};

    esp_err_t logs_get(httpd_req_t *r, const std::string &logfile);
    esp_err_t acl_get(httpd_req_t *r);
    esp_err_t acl_post(httpd_req_t *r, std::string&& post_body);

    static esp_err_t handle_get(httpd_req_t *r);
    static esp_err_t handle_post(httpd_req_t *r);
    static bool request_has_header(httpd_req_t *req, const char *name);
    static optional<std::string> request_get_header(httpd_req_t *req, const char *name);
};

}  // namespace acl
}  // namespace esphome
