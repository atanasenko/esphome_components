#pragma once

#include <string>
#include <map>
#include <list>
#include <vector>

#include "esphome/components/sdmmc/sdfs.h"
#include "esphome/core/optional.h"

namespace esphome {
namespace acl {

struct AclEntry {
  AclEntry(
    const std::string &_name,
    const std::string &_key): name(_name), key(_key) {}
  std::string name;
  std::string key;
};

struct LogEntry {
  LogEntry(
    const std::string &_timestamp,
    const std::string &_message): timestamp(_timestamp), message(_message) {}
  std::string timestamp;
  std::string message;
};

class AclStore {
  public:
    void set_sdfs(sdmmc::SdFs *sdfs) { sdfs_ = sdfs; }
    void set_path(const std::string &path) { path_ = path; }

    optional<std::list<AclEntry>> load_acl();
    
    void store_acl(const std::list<AclEntry> &data);

    void store_logs(const std::vector<LogEntry> &logs);

    optional<std::string> load_acl_content();

    void store_acl_content(const std::string &data);

    optional<std::string> load_log_content(const std::string &period);

  private:
    sdmmc::SdFs *sdfs_;
    std::string path_;
    optional<std::string> find_latest_log_();
    bool load_acl_from_string_(const std::string &str, std::list<AclEntry> &data);
    bool load_acl_entry_(const std::string &str, std::list<AclEntry> &data);
};

}  // namespace acl
}  // namespace esphome
