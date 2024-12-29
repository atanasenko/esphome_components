#pragma once

#include <string>
#include <optional>
#include <functional>

#include "esphome/core/hal.h"
#include "esphome/core/optional.h"

namespace esphome {
namespace sdmmc {

enum CardType {
  SDIO = 0,
  MMC,
  SDSC,
  SDHC_SDXC,
  SDHC
};

class SdFs;

class SdImpl {
  public:
    SdFs *mount(
      int clk_pin, 
      int cmd_pin, 
#ifdef USE_SD_MODE_4BIT
      int data0_pin, 
      int data1_pin, 
      int data2_pin, 
      int data3_pin);
#else
      int data0_pin);
#endif

};

class SdFs {
  public:
    SdFs(CardType card_type) : card_type_(card_type) {
      update_usage_();
    }
    CardType card_type() { return card_type_; };
    uint64_t total_bytes() { return total_bytes_; }
    uint64_t used_bytes() { return used_bytes_; }

    bool exists(const std::string &path);
    bool is_directory(const std::string &path);
    bool create_dir(const std::string &path);
    bool remove_dir(const std::string &path);
    bool read_file(const std::string &path, std::function<bool(const char*, const size_t)> callback);
    bool write_file(const std::string &path, const std::string &message);
    bool append_file(const std::string &path, const std::string &message);
    bool rename_file(const std::string &path1, const std::string &path2);
    bool delete_file(const std::string &path);
    bool list_dir(const std::string &dirname, std::function<bool(const std::string&)> callback);

    optional<std::string> read_file_string(const std::string &path) {
      std::string content;
      if(!read_file(path, [&content](const char* data, const size_t length) -> bool {
        content.append(data, length);
        return true;
      })) {
        return {};
      }
      return content;
    }

  protected:
    CardType card_type_;
    uint64_t total_bytes_{0};
    uint64_t used_bytes_{0};

    std::string full_path_(const std::string &path);
    void update_usage_();

};

}  // namespace sdmmc
}  // namespace esphome
