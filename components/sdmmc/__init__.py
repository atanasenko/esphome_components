import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome import automation, pins
from esphome.core import CORE
from esphome.const import (
    CONF_ID,
    CONF_CLK_PIN,
    CONF_INPUT,
    CONF_OUTPUT,
)

CODEOWNERS = ["@atanasenko"]
MULTI_CONF = True

CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"

sdmmc_ns = cg.esphome_ns.namespace("sdmmc")
SdMmcComponent = sdmmc_ns.class_("SdMmcComponent", cg.Component)

# Actions
SdMmcTestAction = sdmmc_ns.class_("SdMmcTestAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdMmcComponent),
        cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CMD_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DATA0_PIN): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA1_PIN): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA2_PIN): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA3_PIN): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
}
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    if CORE.is_esp32 and CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_FATFS_LFN_HEAP", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk))

    cmd = await cg.gpio_pin_expression(config[CONF_CMD_PIN])
    cg.add(var.set_cmd_pin(cmd))

    data0 = await cg.gpio_pin_expression(config[CONF_DATA0_PIN])
    cg.add(var.set_data0_pin(data0))

    if (config[CONF_MODE_1BIT] == False):
        cg.add_define('USE_SD_MODE_4BIT')
        data1 = await cg.gpio_pin_expression(config[CONF_DATA1_PIN])
        cg.add(var.set_data1_pin(data1))

        data2 = await cg.gpio_pin_expression(config[CONF_DATA2_PIN])
        cg.add(var.set_data2_pin(data2))

        data3 = await cg.gpio_pin_expression(config[CONF_DATA3_PIN])
        cg.add(var.set_data3_pin(data3))

@automation.register_action(
    "sdmmc.test",
    SdMmcTestAction,
    cv.Schema({cv.GenerateID(): cv.use_id(SdMmcComponent)}),
)
async def sdmmc_test_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
