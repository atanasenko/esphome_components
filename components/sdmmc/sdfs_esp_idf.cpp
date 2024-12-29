//#ifdef USE_ESP_IDF
#include "sdfs.h"
#include "esphome/core/log.h"

#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>

#define SD_OCR_S18_RA                   (1<<24)
#define SD_OCR_SDHC_CAP                 (1<<30)  

namespace esphome {
namespace sdmmc {

static const char *const mount_point = "/sdcard";
static const char *const TAG = "sdfs_esp_idf";

#ifdef USE_SD_MODE_4BIT
  SdFs *SdImpl::mount(
    int clk_pin, 
    int cmd_pin, 
    int data0_pin,
    int data1_pin, 
    int data2_pin, 
    int data3_pin) {
#else
  SdFs *SdImpl::mount(
    int clk_pin, 
    int cmd_pin, 
    int data0_pin) {
#endif

  esp_err_t ret;

  // Options for mounting the filesystem.
  // If format_if_mount_failed is set to true, SD card will be partitioned and
  // formatted in case when mounting fails.
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };
  sdmmc_card_t *card;
  ESP_LOGD(TAG, "Initializing SD card");

  // Use settings defined above to initialize SD card and mount FAT filesystem.
  // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
  // Please check its source code and implement error recovery when developing
  // production applications.

  ESP_LOGD(TAG, "Using SDMMC peripheral");

  // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
  // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
  // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
#if USE_SD_SDMMC_SPEED_HS
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
#elif USE_SD_SDMMC_SPEED_UHS_I_SDR50
  host.slot = SDMMC_HOST_SLOT_0;
  host.max_freq_khz = SDMMC_FREQ_SDR50;
  host.flags &= ~SDMMC_HOST_FLAG_DDR;
#elif USE_SD_SDMMC_SPEED_UHS_I_DDR50
  host.slot = SDMMC_HOST_SLOT_0;
  host.max_freq_khz = SDMMC_FREQ_DDR50;
#endif

  // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
  // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
  // and the internal LDO power supply, we need to initialize the power supply first.
#if USE_SD_PWR_CTRL_LDO_INTERNAL_IO
  sd_pwr_ctrl_ldo_config_t ldo_config = {
    .ldo_chan_id = USE_SD_PWR_CTRL_LDO_IO_ID,
  };
  sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

  ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
    return;
  }
  host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#if USE_SD_IS_UHS1
  slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
#endif

  // Set bus width to use:
#ifdef USE_SD_MODE_4BIT
  slot_config.width = 4;
#else
  slot_config.width = 1;
#endif

  // On chips where the GPIOs used for SD card can be configured, set them in
  // the slot_config structure:
  slot_config.clk = (gpio_num_t) clk_pin;
  slot_config.cmd = (gpio_num_t) cmd_pin;
  slot_config.d0 = (gpio_num_t) data0_pin;
#ifdef USE_SD_MODE_4BIT
  slot_config.d1 = (gpio_num_t) data1_pin;
  slot_config.d2 = (gpio_num_t) data2_pin;
  slot_config.d3 = (gpio_num_t) data3_pin;
#endif  // USE_SD_MODE_4BIT

  // Enable internal pullups on enabled pins. The internal pullups
  // are insufficient however, please make sure 10k external pullups are
  // connected on the bus. This is for debug / example purpose only.
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  ESP_LOGD(TAG, "Mounting filesystem");
  ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
    } else {
      ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
#ifdef USE_SD_DEBUG_PIN_CONNECTIONS
      check_sd_card_pins(&config, pin_count);
#endif
    }
    return nullptr;
  }
  ESP_LOGD(TAG, "Filesystem mounted");

  CardType type;
  if (card->is_sdio) {
    type = SDIO;
  } else if (card->is_mmc) {
    type = MMC;
  } else {
    if ((card->ocr & SD_OCR_SDHC_CAP) == 0) {
      type = SDSC;
    } else {
      if (card->ocr & SD_OCR_S18_RA) {
        // SDHC/SDXC (UHS-I)
        type = SDHC_SDXC;
      } else {
        type = SDHC;
      }
    }
  }
  return new SdFs(type);
}

void SdFs::update_usage_() {
  FATFS *fsinfo;
  DWORD fre_clust;
  if (f_getfree("0:", &fre_clust, &fsinfo) != 0) {
    total_bytes_ = 0;
    used_bytes_ = 0;
  } else {
    total_bytes_ = ((uint64_t)(fsinfo->csize)) * (fsinfo->n_fatent - 2) * (fsinfo->ssize);
    used_bytes_ = ((uint64_t)(fsinfo->csize)) * ((fsinfo->n_fatent - 2) - (fsinfo->free_clst)) * (fsinfo->ssize);
  }
}

std::string SdFs::full_path_(const std::string &path) {
  std::string fpath = mount_point;
  fpath += path;
  return fpath;
}

bool SdFs::exists(const std::string &path) {
  const std::string fpath = full_path_(path);
  struct stat st;
  return !stat(fpath.c_str(), &st);
}

bool SdFs::is_directory(const std::string &path) {
  const std::string fpath = full_path_(path);
  struct stat st;
  if (!stat(fpath.c_str(), &st)) {
    return S_ISDIR(st.st_mode);
  }
  return false;
}

bool SdFs::list_dir(const std::string &path, std::function<bool(const std::string&)> callback) {
  const std::string fpath = full_path_(path);

  struct dirent *dp;
  DIR *dir = opendir(fpath.c_str());
  if (dir == NULL) {
    ESP_LOGE(TAG, "Failed to open dir %s", fpath.c_str());
    return false;
  }
  while ((dp = readdir (dir)) != NULL) {
    if(!callback(dp->d_name)) {
      break;
    }
  }
  closedir (dir);
  return true;
}

bool SdFs::read_file(const std::string &path, std::function<bool(const char*, const size_t)> callback) {
  const std::string fpath = full_path_(path);
  ESP_LOGD(TAG, "Reading file %s", fpath.c_str());
  FILE *f = fopen(fpath.c_str(), "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file %s for reading", fpath.c_str());
    return false;
  }

  char buffer[1024];
  size_t s;
  while(s = fread(&buffer, sizeof buffer[0], sizeof buffer, f)) {
     if (!callback(buffer, s)) {
       break;
     }
  }

  fclose(f);
  return true;
}

bool SdFs::create_dir(const std::string &path) {
  if (is_directory(path)) {
    return true;
  } else if(exists(path)) {
    // it's a file
    return false;
  }
  const std::string fpath = full_path_(path);
  auto rc = mkdir(fpath.c_str(), ACCESSPERMS);
  update_usage_();
  return rc == 0;
}

bool SdFs::remove_dir(const std::string &path) {
  if (!is_directory(path)) {
    return false;
  }
  const std::string fpath = full_path_(path);
  auto rc = rmdir(fpath.c_str());
  update_usage_();
  return rc == 0;
}

bool SdFs::rename_file(const std::string &path1, const std::string &path2) {
  const std::string fpath1 = full_path_(path1);
  const std::string fpath2 = full_path_(path2);
  if (!exists(fpath1) || exists(fpath2)) {
    return false;
  }

  auto rc = rename(fpath1.c_str(), fpath2.c_str());
  return rc == 0;
}

bool SdFs::delete_file(const std::string &path) {
  if (!exists(path) || is_directory(path)) {
    return false;
  }
  const std::string fpath = full_path_(path);
  auto rc = unlink(fpath.c_str());
  update_usage_();
  return rc == 0;
}

bool SdFs::write_file(const std::string &path, const std::string &data) {
  const std::string fpath = full_path_(path);

  ESP_LOGD(TAG, "Writing file %s", fpath.c_str());
  FILE *f = fopen(fpath.c_str(), "w");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file %s for reading", fpath.c_str());
    return false;
  }

  ESP_LOGD(TAG, "Writing %d bytes", data.length());
  auto rc = fwrite(data.c_str(), 1, data.length(), f);
  fclose(f);

  update_usage_();
  return rc == data.length();
}

bool SdFs::append_file(const std::string &path, const std::string &data) {
  const std::string fpath = full_path_(path);

  ESP_LOGD(TAG, "Appending file %s", fpath.c_str());
  FILE *f = fopen(fpath.c_str(), "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file %s for reading", fpath.c_str());
    return false;
  }

  auto rc = fwrite(data.c_str(), 1, data.length(), f);
  fclose(f);

  update_usage_();
  return rc == data.length();
}

}  // namespace sdmmc
}  // namespace esphome

//#endif //USE_ESP_IDF
