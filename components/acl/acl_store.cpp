#include "acl_store.h"

#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace acl {

  static const char *const TAG = "acl_store";

  optional<std::list<AclEntry>> AclStore::load_acl() {
    std::list<AclEntry> data;
    if (sdfs_ == nullptr) {
      return data;
    }

    //ESP_LOGI(TAG, "Loading /acl.json");
    std::list<AclEntry>* d = &data;
    optional<std::string> contents = load_acl_content();
    if (contents.has_value()) {
        if(!json::parse_json(contents.value(), [d](JsonObject root) -> bool {
          for (auto const& item: root) {
            std::string name = item.key().c_str();
            JsonObject obj = item.value().as<JsonObject>();
            AclEntry entry(name, obj["key"].as<const char*>());
            d->emplace_back(std::move(entry));
          }
          return true;
        })) {
          return {};
        };
    }
    return data;
  }

  void AclStore::store_acl(const std::list<AclEntry> &data) {
    if (sdfs_ == nullptr) {
      return;
    }

    std::string res = json::build_json([data](JsonObject root) -> void {
      for (auto const& entry: data) {
        auto map = root[entry.name.c_str()];
        map["key"] = entry.key.c_str();
      }
    });
    store_acl_content(res);
  }

  void AclStore::store_logs(const std::vector<LogEntry> &logs) {
    if (sdfs_ == nullptr) {
      return;
    }
    if (!sdfs_->exists("/" + path_)) {
      sdfs_->create_dir("/" + path_);
    }
    if (!sdfs_->exists("/" + path_ + "/logs")) {
      sdfs_->create_dir("/" + path_ + "/logs");
    }

    std::string curfile;
    std::string content;
    for (auto const& log: logs) {
      size_t space = log.timestamp.find(' ');
      std::string time;
      if (space == std::string::npos) {
        continue;
      }
      std::string file = log.timestamp.substr(0, space);
      //time = log.timestamp.substr(space + 1, log.timestamp.length() - space - 1);
      if (curfile != file) {
        if (!content.empty()) {
          sdfs_->append_file("/" + path_ + "/logs/" + curfile + ".log", content);
          content.clear();
        }
        curfile = file;
      }
      content += "[" + log.timestamp + "] " + log.message + "\r\n";
    }

    if (!content.empty()) {
      sdfs_->append_file("/" + path_ + "/logs/" + curfile + ".log", content);
      content.clear();
    }
  }

  optional<std::string> AclStore::load_acl_content() {
    if (sdfs_ == nullptr) {
      return {};
    }
    return sdfs_->read_file_string("/" + path_ + "/acl.json");
  }

  void AclStore::store_acl_content(const std::string &data) {
    if (sdfs_ == nullptr) {
      return;
    }

    //ESP_LOGI(TAG, "Saving /acl.json");
    if (!sdfs_->exists("/" + path_)) {
      sdfs_->create_dir("/" + path_);
    }
    if (!sdfs_->write_file("/" + path_ + "/acl.json", data)) {
      ESP_LOGE(TAG, "Error saving acl.json file");
    }
  }

  optional<std::string> AclStore::find_latest_log_() {
    if (sdfs_ == nullptr) {
      return {};
    }
    optional<std::string> result = {};
    sdfs_->list_dir("/" + path_ + "/logs", [&result](const std::string &name) -> bool {
      if (!result.has_value() || result.value().compare(name) < 0) {
        result = name;
      }
      return true;
    });
    return result;
  }

  optional<std::string> AclStore::load_log_content(const std::string &period) {
    if (sdfs_ == nullptr) {
      return {};
    }

    std::string fname;
    if (period == "latest") {
      optional<std::string> latest = find_latest_log_();
      if (!latest.has_value()) {
        return latest;
      }
      fname = latest.value();
    } else {
      fname = period + ".log";
    }
    return sdfs_->read_file_string("/" + path_ + "/logs/" + fname);
  }


}  // namespace acl
}  // namespace esphome
