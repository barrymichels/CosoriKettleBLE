"""Binary sensor platform for Cosori Kettle BLE."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import COSORI_KETTLE_BLE_COMPONENT_SCHEMA, CONF_COSORI_KETTLE_BLE_ID
from esphome.const import DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_HEAT

CONF_ON_BASE = "on_base"
CONF_HEATING = "heating"

CONFIG_SCHEMA = COSORI_KETTLE_BLE_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_ON_BASE): binary_sensor.binary_sensor_schema(
          device_class=DEVICE_CLASS_CONNECTIVITY,
        ),
        cv.Optional(CONF_HEATING): binary_sensor.binary_sensor_schema(
          device_class=DEVICE_CLASS_HEAT,
          icon="mdi:kettle-steam",
        ),
    }
)


async def to_code(config):
    """Code generation for binary sensor platform."""
    parent = await cg.get_variable(config[CONF_COSORI_KETTLE_BLE_ID])

    if CONF_ON_BASE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ON_BASE])
        cg.add(parent.set_on_base_binary_sensor(sens))

    if CONF_HEATING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HEATING])
        cg.add(parent.set_heating_binary_sensor(sens))
