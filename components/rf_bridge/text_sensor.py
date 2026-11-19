import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import CONF_RF_BRIDGE_ID, RfBridge

CONF_LAST_FRAME = "last_frame"
CONF_SELECTED_CAPTURE = "selected_capture"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RF_BRIDGE_ID): cv.use_id(RfBridge),
        cv.Optional(CONF_LAST_FRAME): text_sensor.text_sensor_schema(
            icon="mdi:radio-tower"
        ),
        cv.Optional(CONF_SELECTED_CAPTURE): text_sensor.text_sensor_schema(
            icon="mdi:history"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RF_BRIDGE_ID])
    if conf := config.get(CONF_LAST_FRAME):
        cg.add(
            parent.set_last_frame_text_sensor(await text_sensor.new_text_sensor(conf))
        )
    if conf := config.get(CONF_SELECTED_CAPTURE):
        cg.add(
            parent.set_selected_capture_text_sensor(
                await text_sensor.new_text_sensor(conf)
            )
        )
