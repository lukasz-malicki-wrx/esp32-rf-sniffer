import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_RF_BRIDGE_ID, RfBridge, rf_bridge_ns

RfBridgeButton = rf_bridge_ns.class_("RfBridgeButton", button.Button)
Kind = RfBridgeButton.enum("Kind", is_class=True)

CONF_TRANSMIT_SELECTED = "transmit_selected"
CONF_TRANSMIT_LAST = "transmit_last"
CONF_CLEAR_CAPTURES = "clear_captures"

BUTTONS = {
    CONF_TRANSMIT_SELECTED: (Kind.TRANSMIT_SELECTED, "mdi:send", cv.UNDEFINED),
    CONF_TRANSMIT_LAST: (Kind.TRANSMIT_LAST, "mdi:replay", cv.UNDEFINED),
    CONF_CLEAR_CAPTURES: (
        Kind.CLEAR_CAPTURES,
        "mdi:delete-sweep",
        ENTITY_CATEGORY_CONFIG,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RF_BRIDGE_ID): cv.use_id(RfBridge),
        **{
            cv.Optional(key): button.button_schema(
                RfBridgeButton, icon=icon, entity_category=category
            )
            for key, (_, icon, category) in BUTTONS.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RF_BRIDGE_ID])
    for key, (kind, _, _) in BUTTONS.items():
        if conf := config.get(key):
            var = await button.new_button(conf, kind)
            await cg.register_parented(var, parent)
