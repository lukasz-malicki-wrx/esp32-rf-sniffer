import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

from . import CONF_RF_BRIDGE_ID, RfBridge, rf_bridge_ns

CaptureSlotSelect = rf_bridge_ns.class_("CaptureSlotSelect", select.Select)

CONF_CAPTURE_SLOT = "capture_slot"
CONF_HISTORY_SIZE = "history_size"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RF_BRIDGE_ID): cv.use_id(RfBridge),
        cv.Optional(CONF_CAPTURE_SLOT): select.select_schema(
            CaptureSlotSelect, icon="mdi:history"
        ),
    }
)


def _slot_options(count: int) -> list[str]:
    return ["1 (newest)"] + [str(i) for i in range(2, count + 1)]


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RF_BRIDGE_ID])
    if conf := config.get(CONF_CAPTURE_SLOT):
        history_size = _bridge_history_size(config[CONF_RF_BRIDGE_ID])
        var = await select.new_select(conf, options=_slot_options(history_size))
        await cg.register_parented(var, parent)
        cg.add(parent.set_capture_slot_select(var))


def _bridge_history_size(bridge_id) -> int:
    """Number of slots = the bridge's history_size, so every option maps to a slot."""
    for bridge in CORE.config.get("rf_bridge", []):
        if str(bridge[CONF_ID]) == str(bridge_id):
            return bridge[CONF_HISTORY_SIZE]
    raise cv.Invalid(f"rf_bridge '{bridge_id}' not found")
