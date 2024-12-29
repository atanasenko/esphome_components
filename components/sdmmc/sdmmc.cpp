#include "sdmmc.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sdmmc {

static const char *const TAG = "sdmmc";

void SdMmcComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SDMMC Component");
  LOG_PIN("CLK Pin: ", this->clk_pin_);
  LOG_PIN("CMD Pin: ", this->cmd_pin_);
  LOG_PIN("DATA0 Pin: ", this->data0_pin_);
#ifdef USE_SD_MODE_4BIT
  LOG_PIN("DATA1 Pin: ", this->data1_pin_);
  LOG_PIN("DATA2 Pin: ", this->data2_pin_);
  LOG_PIN("DATA3 Pin: ", this->data3_pin_);
#endif
  ESP_LOGCONFIG(TAG, "Mounted: %d", this->is_mounted());
}

void SdMmcComponent::setup() {
#ifdef USE_SD_MODE_4BIT
  this->fs_ = impl_.mount(
    get_pin_no_(this->clk_pin_), 
    get_pin_no_(this->cmd_pin_), 
    get_pin_no_(this->data0_pin_), 
    get_pin_no_(this->data1_pin_), 
    get_pin_no_(this->data2_pin_), 
    get_pin_no_(this->data3_pin_));
#else
  this->fs_ = impl_.mount(
    get_pin_no_(this->clk_pin_), 
    get_pin_no_(this->cmd_pin_), 
    get_pin_no_(this->data0_pin_));
#endif
}

void SdMmcComponent::loop() {

}

void SdMmcComponent::do_test() {
  ESP_LOGI(TAG, "Dir list:");
  this->fs_->list_dir("/", [](const std::string &name) -> bool {
    ESP_LOGI(TAG, name.c_str());
    return true;
  });
}

} // namespace acl
} // namespace esphome
