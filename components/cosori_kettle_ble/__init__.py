"""ESPHome component for Cosori Kettle BLE."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, climate
from esphome.const import CONF_ID

CODEOWNERS = ["@barrymichels"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "binary_sensor", "number", "switch", "climate"]

CONF_COSORI_KETTLE_BLE_ID = "cosori_kettle_ble_id"

cosori_kettle_ble_ns = cg.esphome_ns.namespace("cosori_kettle_ble")
CosoriKettleBLE = cosori_kettle_ble_ns.class_(
    "CosoriKettleBLE", ble_client.BLEClientNode, cg.PollingComponent, climate.Climate
)

COSORI_KETTLE_BLE_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_COSORI_KETTLE_BLE_ID): cv.use_id(CosoriKettleBLE),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CosoriKettleBLE),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    """Code generation for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
