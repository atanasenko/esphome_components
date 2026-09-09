"""Apply HTTP and network SDK settings after ESPHome component defaults."""

import logging

import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.coroutine import CoroPriority, coroutine_with_priority

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["web_server"]
CONF_MAX_REQUEST_HEADER_LENGTH = "max_request_header_length"
CONF_MAX_URI_LENGTH = "max_uri_length"
CONF_MAX_SOCKETS = "max_sockets"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_MAX_REQUEST_HEADER_LENGTH): cv.int_range(
                min=1024, max=65535
            ),
            cv.Required(CONF_MAX_URI_LENGTH): cv.int_range(min=256, max=4096),
            cv.Required(CONF_MAX_SOCKETS): cv.int_range(min=1, max=64),
        }
    ),
    cv.only_on_esp32,
)


@coroutine_with_priority(CoroPriority.FINAL)
async def to_code(config):
    # ESPHome components set defaults for these options during code generation.
    # Apply the project values last so they also configure the separate ACL
    # server's HTTPD_DEFAULT_CONFIG and the shared lwIP socket pool.
    add_idf_sdkconfig_option(
        "CONFIG_HTTPD_MAX_REQ_HDR_LEN", config[CONF_MAX_REQUEST_HEADER_LENGTH]
    )
    add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_URI_LEN", config[CONF_MAX_URI_LENGTH])
    add_idf_sdkconfig_option("CONFIG_LWIP_MAX_SOCKETS", config[CONF_MAX_SOCKETS])
    _LOGGER.info(
        "Applied SDK limits: HTTP request headers=%d, URI=%d, lwIP sockets=%d",
        config[CONF_MAX_REQUEST_HEADER_LENGTH],
        config[CONF_MAX_URI_LENGTH],
        config[CONF_MAX_SOCKETS],
    )
