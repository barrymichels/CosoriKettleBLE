"""Switch platform for Cosori Kettle BLE."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, DEVICE_CLASS_HEAT, DEVICE_CLASS_SWITCH
from . import COSORI_KETTLE_BLE_COMPONENT_SCHEMA, CONF_COSORI_KETTLE_BLE_ID, cosori_kettle_ble_ns

CONF_HEATING_SWITCH = "heating_switch"
CONF_BLE_CONNECTION_SWITCH = "ble_connection_switch"
CONF_BABY_FORMULA_SWITCH = "baby_formula_switch"
CONF_REGISTER_SWITCH = "register_switch"

CosoriKettleHeatingSwitch = cosori_kettle_ble_ns.class_("CosoriKettleHeatingSwitch", switch.Switch, cg.Component)
CosoriKettleBLEConnectionSwitch = cosori_kettle_ble_ns.class_("CosoriKettleBLEConnectionSwitch", switch.Switch, cg.Component)
CosoriKettleBabyFormulaSwitch = cosori_kettle_ble_ns.class_("CosoriKettleBabyFormulaSwitch", switch.Switch, cg.Component)
CosoriKettleRegisterSwitch = cosori_kettle_ble_ns.class_("CosoriKettleRegisterSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = COSORI_KETTLE_BLE_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_HEATING_SWITCH): switch.switch_schema(
            CosoriKettleHeatingSwitch,
            icon="mdi:kettle-steam",
            device_class=DEVICE_CLASS_SWITCH,
        ),
        cv.Optional(CONF_BLE_CONNECTION_SWITCH): switch.switch_schema(
            CosoriKettleBLEConnectionSwitch,
            icon="mdi:bluetooth",
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode="RESTORE_DEFAULT_ON",
        ),
        cv.Optional(CONF_BABY_FORMULA_SWITCH): switch.switch_schema(
            CosoriKettleBabyFormulaSwitch,
            icon="mdi:baby-bottle",
            device_class=DEVICE_CLASS_SWITCH,
        ),
        cv.Optional(CONF_REGISTER_SWITCH): switch.switch_schema(
            CosoriKettleRegisterSwitch,
            icon="mdi:key-plus",
            device_class=DEVICE_CLASS_SWITCH,
        ),
    }
)


async def to_code(config):
    """Code generation for switch platform."""
    parent = await cg.get_variable(config[CONF_COSORI_KETTLE_BLE_ID])

    if CONF_HEATING_SWITCH in config:
        conf = config[CONF_HEATING_SWITCH]
        sw = await switch.new_switch(conf)
        await cg.register_parented(sw, config[CONF_COSORI_KETTLE_BLE_ID])
        cg.add(parent.set_heating_switch(sw))

    if CONF_BLE_CONNECTION_SWITCH in config:
        conf = config[CONF_BLE_CONNECTION_SWITCH]
        sw = await switch.new_switch(conf)
        await cg.register_parented(sw, config[CONF_COSORI_KETTLE_BLE_ID])
        cg.add(parent.set_ble_connection_switch(sw))

    if CONF_BABY_FORMULA_SWITCH in config:
        conf = config[CONF_BABY_FORMULA_SWITCH]
        sw = await switch.new_switch(conf)
        await cg.register_parented(sw, config[CONF_COSORI_KETTLE_BLE_ID])
        cg.add(parent.set_baby_formula_switch(sw))

    if CONF_REGISTER_SWITCH in config:
        conf = config[CONF_REGISTER_SWITCH]
        sw = await switch.new_switch(conf)
        await cg.register_parented(sw, config[CONF_COSORI_KETTLE_BLE_ID])
        cg.add(parent.set_register_switch(sw))
