import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from . import CONF_A7670_ID, A7670Component

DEPENDENCIES = ["a7670"]

CONF_REGISTERED = "registered"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_A7670_ID): cv.use_id(A7670Component),
    cv.Optional(CONF_REGISTERED): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    a7670_component = await cg.get_variable(config[CONF_A7670_ID])

    if CONF_REGISTERED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_REGISTERED])
        cg.add(a7670_component.set_registered_binary_sensor(sens))
