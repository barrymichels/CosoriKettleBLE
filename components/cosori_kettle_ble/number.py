"""Number platform for Cosori Kettle BLE."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_MODE, DEVICE_CLASS_TEMPERATURE, DEVICE_CLASS_DURATION
from . import COSORI_KETTLE_BLE_COMPONENT_SCHEMA, CONF_COSORI_KETTLE_BLE_ID, cosori_kettle_ble_ns

CONF_TARGET_SETPOINT = "target_setpoint"
CONF_HOLD_TIME = "hold_time"
CONF_MY_TEMP = "my_temp"
CONF_MIN_VALUE = "min_value"
CONF_MAX_VALUE = "max_value"
CONF_STEP = "step"

CosoriKettleNumber = cosori_kettle_ble_ns.class_("CosoriKettleNumber", number.Number, cg.Component)
CosoriKettleHoldTimeNumber = cosori_kettle_ble_ns.class_("CosoriKettleHoldTimeNumber", number.Number, cg.Component)
CosoriKettleMyTempNumber = cosori_kettle_ble_ns.class_("CosoriKettleMyTempNumber", number.Number, cg.Component)

CONFIG_SCHEMA = COSORI_KETTLE_BLE_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_TARGET_SETPOINT): number.number_schema(
            CosoriKettleNumber,
            unit_of_measurement="°F",
            icon="mdi:thermometer",
            device_class=DEVICE_CLASS_TEMPERATURE,
        ).extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=104.0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=212.0): cv.float_,
                cv.Optional(CONF_STEP, default=1.0): cv.float_,
                cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
            }
        ),
        cv.Optional(CONF_HOLD_TIME): number.number_schema(
            CosoriKettleHoldTimeNumber,
            unit_of_measurement="s",
            icon="mdi:timer",
            device_class=DEVICE_CLASS_DURATION,
        ).extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=65535.0): cv.float_,
                cv.Optional(CONF_STEP, default=1.0): cv.float_,
                cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
            }
        ),
        cv.Optional(CONF_MY_TEMP): number.number_schema(
            CosoriKettleMyTempNumber,
            unit_of_measurement="°F",
            icon="mdi:thermometer",
            device_class=DEVICE_CLASS_TEMPERATURE,
        ).extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=104.0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=212.0): cv.float_,
                cv.Optional(CONF_STEP, default=1.0): cv.float_,
                cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
            }
        ),
    }
)


async def to_code(config):
    """Code generation for number platform."""
    parent = await cg.get_variable(config[CONF_COSORI_KETTLE_BLE_ID])

    if CONF_TARGET_SETPOINT in config:
        conf = config[CONF_TARGET_SETPOINT]
        # TODO: does this propagate automatically, or do we need to do it manually?
        # conf[DEVICE_ID] = config[CONF_COSORI_KETTLE_BLE_ID][DEVICE_ID]
        num = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_parented(num, config[CONF_COSORI_KETTLE_BLE_ID])
        cg.add(parent.set_target_setpoint_number(num))

    if CONF_HOLD_TIME in config:
        conf = config[CONF_HOLD_TIME]
        num = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_parented(num, config[CONF_COSORI_KETTLE_BLE_ID])
        cg.add(parent.set_hold_time_number(num))

    if CONF_MY_TEMP in config:
        conf = config[CONF_MY_TEMP]
        num = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_parented(num, config[CONF_COSORI_KETTLE_BLE_ID])
        cg.add(parent.set_my_temp_number(num))
