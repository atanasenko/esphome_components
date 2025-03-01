import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome import automation
from esphome.core import CORE
from esphome.const import (
    CONF_ID,
)
from esphome.components import time, web_server_base, sdmmc
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID


AUTO_LOAD = ["time", "json", "web_server_base", "sdmmc"]
CODEOWNERS = ["@atanasenko"]
MULTI_CONF = True

acl_ns = cg.esphome_ns.namespace("acl")
AclComponent = acl_ns.class_("AclComponent", cg.Component)

# Actions
AclTestAction = acl_ns.class_("AclTestAction", automation.Action)

CONF_CLOCK_ID = "clock_id"
CONF_SDMMC_ID = "sdmmc_id"
CONF_PATH = "path"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AclComponent),
        cv.Required(CONF_CLOCK_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_SDMMC_ID): cv.use_id(sdmmc.SdMmcComponent),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
        # cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
        #     web_server_base.WebServerBase
        # ),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # increase max sockets from default 10 to 16 (16 is the actual max)
    if CORE.is_esp32 and CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_LWIP_MAX_SOCKETS", 16)
        add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 8192)
        add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_URI_LEN", 4096)
    
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    clock = await cg.get_variable(config[CONF_CLOCK_ID])
    cg.add(var.set_clock(clock))
    sdmmc = await cg.get_variable(config[CONF_SDMMC_ID])
    cg.add(var.set_sdmmc(sdmmc))
    path_ = await cg.templatable(config[CONF_PATH], [], cg.std_string)
    cg.add(var.set_path(path_))

    # web_base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    # cg.add(var.set_webserver(web_base))
