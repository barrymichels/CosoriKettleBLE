"""Climate platform for Cosori Kettle BLE."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID
from . import COSORI_KETTLE_BLE_COMPONENT_SCHEMA, CONF_COSORI_KETTLE_BLE_ID, CosoriKettleBLE

CONF_KETTLE_CLIMATE = "kettle_climate"

CONFIG_SCHEMA = COSORI_KETTLE_BLE_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_KETTLE_CLIMATE): climate.CLIMATE_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(CosoriKettleBLE),
            }
        ),
    }
)


async def to_code(config):
    """Code generation for climate platform."""
    parent = await cg.get_variable(config[CONF_COSORI_KETTLE_BLE_ID])

    if CONF_KETTLE_CLIMATE in config:
        conf = config[CONF_KETTLE_CLIMATE]
        # The parent IS the climate entity (it inherits from climate::Climate)
        # Just register it as a climate component
        await climate.register_climate(parent, conf)
