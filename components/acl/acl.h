#pragma once

#include "acl_store.h"
#include "acl_server.h"

#include <map>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/sdmmc/sdmmc.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace acl {

class AclComponent : public Component /*, public AsyncWebHandler*/ {
  public:
    void set_clock(time::RealTimeClock *clock) { clock_ = clock; }
    void set_sdmmc(sdmmc::SdMmcComponent *sdmmc) { sdmmc_ = sdmmc; }
    void set_path(const std::string &path) { path_ = path; }

    void dump_config() override;
    void setup() override;
    void loop() override;
    void start_server();

    optional<AclEntry*> check(const std::string &key);

    void add_acl(
      const std::string &name,
      const std::string &key);
    
    void remove_acl(const std::string &name);
    
    void clear_acl();

    void print_acl();

    void reload_acl();
    
    void append_log(const std::string &message);

    /*
    void set_webserver(web_server_base::WebServerBase *webserver) { webserver_ = webserver; }
    bool canHandle(AsyncWebServerRequest *request) override;
    void handleRequest(AsyncWebServerRequest *request) override;
    void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;
    */

  protected:
    time::RealTimeClock *clock_{nullptr};
    sdmmc::SdMmcComponent *sdmmc_{nullptr};
    std::string path_;

    AclStore store_;
    AclServer server_;
    std::map<std::string, AclEntry> acl_;
    bool server_started_{false};
    bool store_required_{false};
    bool reload_required_{false};
    std::vector<LogEntry> pending_logs_;

    void load_acl_();
    void store_acl_();
    void store_logs_();
    std::string timestamp_();

    /*
    web_server_base::WebServerBase *webserver_{nullptr};
    void* _tempObject;
    */
};

}  // namespace acl
}  // namespace esphome
