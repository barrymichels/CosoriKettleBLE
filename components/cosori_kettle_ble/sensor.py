"""Sensor platform for Cosori Kettle BLE."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_DURATION,
)
from . import COSORI_KETTLE_BLE_COMPONENT_SCHEMA, CONF_COSORI_KETTLE_BLE_ID

CONF_KETTLE_SETPOINT = "kettle_setpoint"
CONF_HOLD_TIME_REMAINING = "hold_time_remaining"

CONFIG_SCHEMA = COSORI_KETTLE_BLE_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement="°F",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:thermometer",
        ),
        cv.Optional(CONF_KETTLE_SETPOINT): sensor.sensor_schema(
            unit_of_measurement="°F",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:thermometer",
        ),
        cv.Optional(CONF_HOLD_TIME_REMAINING): sensor.sensor_schema(
            unit_of_measurement="s",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:timer",
        ),
    }
)


async def to_code(config):
    """Code generation for sensor platform."""
    parent = await cg.get_variable(config[CONF_COSORI_KETTLE_BLE_ID])

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(parent.set_temperature_sensor(sens))

    if CONF_KETTLE_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_KETTLE_SETPOINT])
        cg.add(parent.set_kettle_setpoint_sensor(sens))

    if CONF_HOLD_TIME_REMAINING in config:
        sens = await sensor.new_sensor(config[CONF_HOLD_TIME_REMAINING])
        cg.add(parent.set_hold_time_remaining_sensor(sens))
