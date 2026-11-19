import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_PERCENT,
)

from . import CONF_RF_BRIDGE_ID, RfBridge

CONF_QUEUE_DEPTH = "queue_depth"
CONF_FRAMES_RECEIVED = "frames_received"
CONF_DUTY_CYCLE_USED = "duty_cycle_used"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RF_BRIDGE_ID): cv.use_id(RfBridge),
        cv.Optional(CONF_QUEUE_DEPTH): sensor.sensor_schema(
            icon="mdi:tray-full",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_FRAMES_RECEIVED): sensor.sensor_schema(
            icon="mdi:counter",
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DUTY_CYCLE_USED): sensor.sensor_schema(
            icon="mdi:timer-sand",
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RF_BRIDGE_ID])
    if conf := config.get(CONF_QUEUE_DEPTH):
        cg.add(parent.set_queue_depth_sensor(await sensor.new_sensor(conf)))
    if conf := config.get(CONF_FRAMES_RECEIVED):
        cg.add(parent.set_frames_received_sensor(await sensor.new_sensor(conf)))
    if conf := config.get(CONF_DUTY_CYCLE_USED):
        cg.add(parent.set_duty_cycle_sensor(await sensor.new_sensor(conf)))
