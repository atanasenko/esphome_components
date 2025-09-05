import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE,
    STATE_CLASS_MEASUREMENT,
    UNIT_BYTES,
    ICON_MEMORY,
)
from . import SdMmcComponent

DEPENDENCIES = ["sdmmc"]

CONF_SDMMC_ID = "sdmmc_id"
CONF_TOTAL_SPACE = "total_space"
CONF_USED_SPACE = "used_space"

SIMPLE_TYPES = [CONF_TOTAL_SPACE, CONF_USED_SPACE]

BASE_CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_BYTES,
    icon=ICON_MEMORY,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(CONF_SDMMC_ID): cv.use_id(SdMmcComponent),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_TOTAL_SPACE : BASE_CONFIG_SCHEMA,
        CONF_USED_SPACE : BASE_CONFIG_SCHEMA,
    },
    lower=True,
)


async def to_code(config):
    sdmmc_component = await cg.get_variable(config[CONF_SDMMC_ID])
    var = await sensor.new_sensor(config)
    if config[CONF_TYPE] in SIMPLE_TYPES:
        func = getattr(sdmmc_component, f"set_{config[CONF_TYPE]}_sensor")
        cg.add(func(var))