import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_MESSAGE,
    CONF_TRIGGER_ID,
)
from esphome.components import uart
from esphome import pins

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@atanasenko"]
MULTI_CONF = True

a7670_ns = cg.esphome_ns.namespace("a7670")
A7670Component = a7670_ns.class_("A7670Component", cg.Component)

A7670ReceivedMessageTrigger = a7670_ns.class_(
    "A7670ReceivedMessageTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)
A7670SentMessageTrigger = a7670_ns.class_(
    "A7670SentMessageTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)
A7670SendMessageFailedTrigger = a7670_ns.class_(
    "A7670SendMessageFailedTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)
A7670IncomingCallTrigger = a7670_ns.class_(
    "A7670IncomingCallTrigger",
    automation.Trigger.template(cg.std_string),
)
A7670CallConnectedTrigger = a7670_ns.class_(
    "A7670CallConnectedTrigger",
    automation.Trigger.template(),
)
A7670CallDisconnectedTrigger = a7670_ns.class_(
    "A7670CallDisconnectedTrigger",
    automation.Trigger.template(),
)
A7670ReceivedUssdTrigger = a7670_ns.class_(
    "A7670ReceivedUssdTrigger",
    automation.Trigger.template(cg.std_string),
)

# Actions
A7670SendSmsAction = a7670_ns.class_("A7670SendSmsAction", automation.Action)
A7670SendUssdAction = a7670_ns.class_("A7670SendUssdAction", automation.Action)
A7670DialAction = a7670_ns.class_("A7670DialAction", automation.Action)
A7670ConnectAction = a7670_ns.class_("A7670ConnectAction", automation.Action)
A7670DisconnectAction = a7670_ns.class_("A7670DisconnectAction", automation.Action)
A7670SendAtAction = a7670_ns.class_("A7670SendAtAction", automation.Action)
A7670DebugOnAction = a7670_ns.class_("A7670DebugOnAction", automation.Action)
A7670DebugOffAction = a7670_ns.class_("A7670DebugOffAction", automation.Action)

CONF_A7670_ID = "a7670_id"
CONF_POWER_PIN = "power_pin"
CONF_PIN_CODE = "pin_code"
CONF_ON_SMS_RECEIVED = "on_sms_received"
CONF_ON_SMS_SENT = "on_sms_sent"
CONF_ON_SMS_SEND_FAILED = "on_sms_send_failed"
CONF_ON_USSD_RECEIVED = "on_ussd_received"
CONF_ON_INCOMING_CALL = "on_incoming_call"
CONF_ON_CALL_CONNECTED = "on_call_connected"
CONF_ON_CALL_DISCONNECTED = "on_call_disconnected"
CONF_RECIPIENT = "recipient"
CONF_USSD = "ussd"
CONF_COMMAND = "command"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(A7670Component),
            cv.Optional(CONF_POWER_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_PIN_CODE): cv.string_strict,
            cv.Optional(CONF_ON_SMS_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7670ReceivedMessageTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_SMS_SENT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7670SentMessageTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_SMS_SEND_FAILED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7670SendMessageFailedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_INCOMING_CALL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7670IncomingCallTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_CONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7670CallConnectedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_DISCONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7670CallDisconnectedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_USSD_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7670ReceivedUssdTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "a7670", require_tx=True, require_rx=True
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_POWER_PIN in config:
        power_pin = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
        cg.add(var.set_power_pin(power_pin))

    if CONF_PIN_CODE in config:
        cg.add(var.set_pin_code(config[CONF_PIN_CODE]))

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for conf in config.get(CONF_ON_SMS_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "message"), (cg.std_string, "sender")], conf
        )
    for conf in config.get(CONF_ON_SMS_SENT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "message"), (cg.std_string, "recipient")], conf
        )
    for conf in config.get(CONF_ON_SMS_SEND_FAILED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "error"), (cg.std_string, "recipient")], conf
        )
    for conf in config.get(CONF_ON_INCOMING_CALL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "caller_id")], conf)
    for conf in config.get(CONF_ON_CALL_CONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CALL_DISCONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_USSD_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "ussd")], conf)

A7670_SEND_SMS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(A7670Component),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
    }
)

@automation.register_action(
    "a7670.send_sms", A7670SendSmsAction, A7670_SEND_SMS_SCHEMA
)
async def a7670_send_sms_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    return var

A7670_DIAL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(A7670Component),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
    }
)

@automation.register_action("a7670.dial", A7670DialAction, A7670_DIAL_SCHEMA)
async def a7670_dial_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(template_))
    return var

@automation.register_action(
    "a7670.connect",
    A7670ConnectAction,
    cv.Schema({cv.GenerateID(): cv.use_id(A7670Component)}),
)
async def a7670_connect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

@automation.register_action(
    "a7670.disconnect",
    A7670DisconnectAction,
    cv.Schema({cv.GenerateID(): cv.use_id(A7670Component)}),
)
async def a7670_disconnect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

A7670_SEND_USSD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(A7670Component),
        cv.Required(CONF_USSD): cv.templatable(cv.string_strict),
    }
)

@automation.register_action(
    "a7670.send_ussd", A7670SendUssdAction, A7670_SEND_USSD_SCHEMA
)
async def a7670_send_ussd_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_USSD], args, cg.std_string)
    cg.add(var.set_ussd(template_))
    return var


A7670_SEND_AT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(A7670Component),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string_strict),
    }
)

@automation.register_action(
    "a7670.send_at", A7670SendAtAction, A7670_SEND_AT_SCHEMA
)
async def a7670_send_at_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(template_))
    return var

@automation.register_action(
    "a7670.debug_on",
    A7670DebugOnAction,
    cv.Schema({cv.GenerateID(): cv.use_id(A7670Component)}),
)
async def a7670_debug_on_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

@automation.register_action(
    "a7670.debug_off",
    A7670DebugOffAction,
    cv.Schema({cv.GenerateID(): cv.use_id(A7670Component)}),
)
async def a7670_debug_off_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
