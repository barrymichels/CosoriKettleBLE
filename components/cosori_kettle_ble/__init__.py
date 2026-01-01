"""ESPHome component for Cosori Kettle BLE."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, climate
from esphome.const import CONF_ID

CODEOWNERS = ["@barrymichels"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "binary_sensor", "number", "switch", "climate"]

CONF_COSORI_KETTLE_BLE_ID = "cosori_kettle_ble_id"
CONF_REGISTRATION_KEY = "registration_key"
CONF_PROTOCOL_VERSION = "protocol_version"

cosori_kettle_ble_ns = cg.esphome_ns.namespace("cosori_kettle_ble")
CosoriKettleBLE = cosori_kettle_ble_ns.class_(
    "CosoriKettleBLE", ble_client.BLEClientNode, cg.PollingComponent, climate.Climate
)

COSORI_KETTLE_BLE_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_COSORI_KETTLE_BLE_ID): cv.use_id(CosoriKettleBLE),
        cv.Optional(cv.CONF_DEVICE_ID): cv.sub_device_id,
    }
)


def validate_registration_key(value):
    """Validate that registration key is a 32-character hex string (16 bytes)."""
    value = cv.string(value)
    # Remove any spaces, colons, or 0x prefixes
    cleaned = value.replace(" ", "").replace(":", "").replace("0x", "").lower()
    if len(cleaned) != 32:
        raise cv.Invalid(f"Registration key must be exactly 32 hex characters (16 bytes), got {len(cleaned)} characters")
    try:
        key_bytes = bytes.fromhex(cleaned)
        if len(key_bytes) != 16:
            raise cv.Invalid(f"Registration key must be 16 bytes, got {len(key_bytes)} bytes")
    except ValueError as e:
        raise cv.Invalid(f"Invalid hex string: {e}")
    return cleaned


CONFIG_SCHEMA = (
    climate._CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(CosoriKettleBLE),
            cv.Required(CONF_REGISTRATION_KEY): validate_registration_key,
            cv.Optional(CONF_PROTOCOL_VERSION, default=0): cv.one_of(0, 1, int=True),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    """Code generation for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    await climate.register_climate(var, config)

    # Convert hex string to array of 16 bytes
    hex_str = config[CONF_REGISTRATION_KEY]
    key_bytes = bytes.fromhex(hex_str)
    if len(key_bytes) != 16:
        raise cv.Invalid(f"Registration key must be 16 bytes, got {len(key_bytes)} bytes")
    
    # Create array<uint8_t, 16> in C++ with double-brace initialization
    # Format: std::array<uint8_t, 16>{{1,2,3,...}}
    byte_list = ','.join(str(b) for b in key_bytes)
    key_array = cg.RawExpression(f"std::array<uint8_t, 16>{{{{{byte_list}}}}}")
    cg.add(var.set_registration_key(key_array))
    
    # Set protocol version
    protocol_version = config.get(CONF_PROTOCOL_VERSION, 0)
    cg.add(var.set_protocol_version(protocol_version))