"""RF bridge: sniffer, capture history and serialised transmitter for one radio."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUTTON,
    CONF_CHANNEL,
    CONF_CHECK,
    CONF_CODE,
    CONF_ID,
    CONF_NBITS,
    CONF_PROTOCOL,
    CONF_REPEAT,
    CONF_TIMES,
    CONF_WAIT_TIME,
)

CODEOWNERS = ["@lukasz-malicki-wrx"]
DEPENDENCIES = ["remote_base"]
MULTI_CONF = True

CONF_RECEIVER_ID = remote_base.CONF_RECEIVER_ID
CONF_TRANSMITTER_ID = remote_base.CONF_TRANSMITTER_ID
CONF_RF_BRIDGE_ID = "rf_bridge_id"
CONF_BAND = "band"
CONF_QUEUE_SIZE = "queue_size"
CONF_FRAME_GAP = "frame_gap"
CONF_DEDUP_WINDOW = "dedup_window"
CONF_HISTORY_SIZE = "history_size"
CONF_DUTY_CYCLE = "duty_cycle"
CONF_LIMIT = "limit"
CONF_WINDOW = "window"
CONF_CAPTURE_RAW = "capture_raw"
CONF_RAW_MIN_PULSES = "raw_min_pulses"
CONF_DOOYA = "dooya"
CONF_RC_SWITCH = "rc_switch"
CONF_RAW = "raw"
CONF_ON_FRAME = "on_frame"
CONF_ON_DOOYA = "on_dooya"
CONF_REMOTE_ID = "remote_id"
CONF_SLOT = "slot"

# Decoders tried on every received frame, in this order (raw capture is the fallback).
_PROTOCOLS = ("dooya", "rc_switch")

rf_bridge_ns = cg.esphome_ns.namespace("rf_bridge")
RfBridge = rf_bridge_ns.class_(
    "RfBridge", cg.Component, remote_base.RemoteReceiverListener
)
RfFrame = rf_bridge_ns.struct("RfFrame")
DooyaData = remote_base.remote_base_ns.struct("DooyaData")

TransmitDooyaAction = rf_bridge_ns.class_("TransmitDooyaAction", automation.Action)
TransmitRcSwitchAction = rf_bridge_ns.class_(
    "TransmitRcSwitchAction", automation.Action
)
TransmitCaptureAction = rf_bridge_ns.class_("TransmitCaptureAction", automation.Action)
ClearCapturesAction = rf_bridge_ns.class_("ClearCapturesAction", automation.Action)


def _repeat_schema(times: int, wait: str | None):
    schema = {cv.Optional(CONF_TIMES, default=times): cv.int_range(min=1, max=50)}
    if wait is not None:
        schema[cv.Optional(CONF_WAIT_TIME, default=wait)] = (
            cv.positive_time_period_microseconds
        )
    return cv.Schema(schema)


def _validate_duty_cycle(value):
    if isinstance(value, dict):
        return DUTY_CYCLE_SCHEMA(value)
    return DUTY_CYCLE_SCHEMA({CONF_LIMIT: value})


DUTY_CYCLE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_LIMIT): cv.All(cv.percentage, cv.Range(min=0.0001, max=1.0)),
        cv.Optional(CONF_WINDOW, default="1h"): cv.positive_time_period_milliseconds,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RfBridge),
        cv.Required(CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
        cv.Optional(CONF_TRANSMITTER_ID): cv.use_id(remote_base.RemoteTransmitterBase),
        cv.Optional(CONF_BAND, default="RF"): cv.string_strict,
        cv.Optional(CONF_QUEUE_SIZE, default=16): cv.int_range(min=1, max=64),
        cv.Optional(
            CONF_FRAME_GAP, default="100ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_DEDUP_WINDOW, default="1500ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HISTORY_SIZE, default=8): cv.int_range(min=1, max=32),
        cv.Optional(CONF_DUTY_CYCLE): _validate_duty_cycle,
        cv.Optional(CONF_CAPTURE_RAW, default=True): cv.boolean,
        cv.Optional(CONF_RAW_MIN_PULSES, default=24): cv.int_range(min=4, max=2048),
        cv.Optional(CONF_REPEAT, default={}): cv.Schema(
            {
                cv.Optional(CONF_DOOYA, default={}): _repeat_schema(6, "8ms"),
                cv.Optional(CONF_RC_SWITCH, default={}): _repeat_schema(10, None),
                cv.Optional(CONF_RAW, default={}): _repeat_schema(5, "10ms"),
            }
        ),
        cv.Optional(CONF_ON_FRAME): automation.validate_automation({}),
        cv.Optional(CONF_ON_DOOYA): automation.validate_automation({}),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await remote_base.register_listener(var, config)
    # remote_base only compiles the protocol sources a configuration asks for.
    for protocol in _PROTOCOLS:
        remote_base.request_protocol(protocol)

    if CONF_TRANSMITTER_ID in config:
        transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter))

    cg.add(var.set_band(config[CONF_BAND]))
    cg.add(var.set_queue_size(config[CONF_QUEUE_SIZE]))
    cg.add(var.set_frame_gap(config[CONF_FRAME_GAP]))
    cg.add(var.set_dedup_window(config[CONF_DEDUP_WINDOW]))
    cg.add(var.set_history_size(config[CONF_HISTORY_SIZE]))
    cg.add(var.set_capture_raw(config[CONF_CAPTURE_RAW]))
    cg.add(var.set_raw_min_pulses(config[CONF_RAW_MIN_PULSES]))

    if duty := config.get(CONF_DUTY_CYCLE):
        cg.add(var.set_duty_cycle(duty[CONF_LIMIT], duty[CONF_WINDOW]))

    repeat = config[CONF_REPEAT]
    cg.add(var.set_dooya_send_times(repeat[CONF_DOOYA][CONF_TIMES]))
    cg.add(var.set_dooya_send_wait(repeat[CONF_DOOYA][CONF_WAIT_TIME]))
    cg.add(var.set_rc_switch_send_times(repeat[CONF_RC_SWITCH][CONF_TIMES]))
    cg.add(var.set_raw_send_times(repeat[CONF_RAW][CONF_TIMES]))
    cg.add(var.set_raw_send_wait(repeat[CONF_RAW][CONF_WAIT_TIME]))

    await automation.build_callback_automations(
        var,
        config,
        (
            automation.CallbackAutomation(
                CONF_ON_FRAME,
                "add_on_frame_callback",
                [(RfFrame.operator("const").operator("ref"), "x")],
            ),
            automation.CallbackAutomation(
                CONF_ON_DOOYA, "add_on_dooya_callback", [(DooyaData, "x")]
            ),
        ),
    )


# --- actions ---------------------------------------------------------------------

_PARENT = {cv.GenerateID(): cv.use_id(RfBridge)}


@automation.register_action(
    "rf_bridge.transmit_dooya",
    TransmitDooyaAction,
    cv.Schema(
        {
            **_PARENT,
            cv.Required(CONF_REMOTE_ID): cv.templatable(cv.hex_uint32_t),
            cv.Required(CONF_CHANNEL): cv.templatable(cv.hex_uint8_t),
            cv.Required(CONF_BUTTON): cv.templatable(cv.uint8_t),
            cv.Optional(CONF_CHECK): cv.templatable(cv.uint8_t),
            cv.Optional(CONF_TIMES): cv.templatable(cv.int_range(min=1, max=50)),
            cv.Optional(CONF_WAIT_TIME): cv.templatable(
                cv.positive_time_period_microseconds
            ),
        }
    ),
    synchronous=True,
)
async def transmit_dooya_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(
        var.set_remote_id(await cg.templatable(config[CONF_REMOTE_ID], args, cg.uint32))
    )
    cg.add(var.set_channel(await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)))
    cg.add(var.set_button(await cg.templatable(config[CONF_BUTTON], args, cg.uint8)))
    if CONF_CHECK in config:
        cg.add(var.set_check(await cg.templatable(config[CONF_CHECK], args, cg.uint8)))
    if CONF_TIMES in config:
        cg.add(
            var.set_send_times(
                await cg.templatable(config[CONF_TIMES], args, cg.uint32)
            )
        )
    if CONF_WAIT_TIME in config:
        cg.add(
            var.set_send_wait(
                await cg.templatable(config[CONF_WAIT_TIME], args, cg.uint32)
            )
        )
    return var


@automation.register_action(
    "rf_bridge.transmit_rc_switch",
    TransmitRcSwitchAction,
    cv.Schema(
        {
            **_PARENT,
            cv.Required(CONF_PROTOCOL): cv.templatable(cv.int_range(min=1, max=8)),
            cv.Required(CONF_CODE): cv.templatable(cv.hex_uint64_t),
            cv.Required(CONF_NBITS): cv.templatable(cv.int_range(min=1, max=64)),
            cv.Optional(CONF_TIMES): cv.templatable(cv.int_range(min=1, max=50)),
        }
    ),
    synchronous=True,
)
async def transmit_rc_switch_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(
        var.set_protocol(await cg.templatable(config[CONF_PROTOCOL], args, cg.uint8))
    )
    cg.add(var.set_code(await cg.templatable(config[CONF_CODE], args, cg.uint64)))
    cg.add(var.set_nbits(await cg.templatable(config[CONF_NBITS], args, cg.uint8)))
    if CONF_TIMES in config:
        cg.add(
            var.set_send_times(
                await cg.templatable(config[CONF_TIMES], args, cg.uint32)
            )
        )
    return var


@automation.register_action(
    "rf_bridge.transmit_capture",
    TransmitCaptureAction,
    # `rf_bridge.transmit_capture: bridge_433` replays the newest capture.
    automation.maybe_simple_id(
        {
            **_PARENT,
            cv.Optional(CONF_SLOT, default=1): cv.templatable(
                cv.int_range(min=1, max=32)
            ),
        }
    ),
    synchronous=True,
)
async def transmit_capture_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_slot(await cg.templatable(config[CONF_SLOT], args, cg.uint8)))
    return var


@automation.register_action(
    "rf_bridge.clear_captures",
    ClearCapturesAction,
    automation.maybe_simple_id(_PARENT),
    synchronous=True,
)
async def clear_captures_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
