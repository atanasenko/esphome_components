#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/core/optional.h"
#include "sdfs.h"

namespace esphome {
namespace sdmmc {

class SdMmcComponent : public Component {
  public:
    void dump_config() override;
    void setup() override;
    void loop() override;
    
    void set_clk_pin(GPIOPin *pin) { this->clk_pin_ = pin; }
    void set_cmd_pin(GPIOPin *pin) { this->cmd_pin_ = pin; }
    void set_data0_pin(GPIOPin *pin) { this->data0_pin_ = pin; }
#ifdef USE_SD_MODE_4BIT
    void set_data1_pin(GPIOPin *pin) { this->data1_pin_ = pin; }
    void set_data2_pin(GPIOPin *pin) { this->data2_pin_ = pin; }
    void set_data3_pin(GPIOPin *pin) { this->data3_pin_ = pin; }
#endif

    void do_test();
    
    bool is_mounted() { return fs_ != nullptr; }
    SdFs *fs() { return fs_; }

  private:
    GPIOPin *clk_pin_;
    GPIOPin *cmd_pin_;
    GPIOPin *data0_pin_;
#ifdef USE_SD_MODE_4BIT
    GPIOPin *data1_pin_;
    GPIOPin *data2_pin_;
    GPIOPin *data3_pin_;
#endif
    SdImpl impl_;
    SdFs *fs_;

    int get_pin_no_(GPIOPin *pin) {
      if (pin == nullptr || !pin->is_internal())
        return -1;
      if (((InternalGPIOPin *) pin)->is_inverted())
        return -1;
      return ((InternalGPIOPin *) pin)->get_pin();
    }
};

template<typename... Ts> class SdMmcTestAction : public Action<Ts...> {
 public:
  SdMmcTestAction(SdMmcComponent *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->do_test(); }

 protected:
  SdMmcComponent *parent_;
  
};

}  // namespace sdmmc
}  // namespace esphome
