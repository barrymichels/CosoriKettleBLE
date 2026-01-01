#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_ESP32

#include <vector>
#include <array>
#include "envelope.h"
#include "cosori_kettle_state.h"

namespace esphome {
namespace cosori_kettle_ble {

class CosoriKettleBLE : public esphome::ble_client::BLEClientNode, public PollingComponent, public climate::Climate {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                          esp_ble_gattc_cb_param_t *param) override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Sensor setters
  void set_temperature_sensor(sensor::Sensor *sensor) { temperature_sensor_ = sensor; }
  void set_kettle_setpoint_sensor(sensor::Sensor *sensor) { kettle_setpoint_sensor_ = sensor; }
  void set_hold_time_remaining_sensor(sensor::Sensor *sensor) { hold_time_remaining_sensor_ = sensor; }

  // Binary sensor setters
  void set_on_base_binary_sensor(binary_sensor::BinarySensor *sensor) { on_base_binary_sensor_ = sensor; }
  void set_heating_binary_sensor(binary_sensor::BinarySensor *sensor) { heating_binary_sensor_ = sensor; }

  // Number setters
  void set_target_setpoint_number(number::Number *number) { target_setpoint_number_ = number; }
  void set_hold_time_number(number::Number *number) { hold_time_number_ = number; }
  void set_my_temp_number(number::Number *number) { my_temp_number_ = number; }

  // Switch setters
  void set_heating_switch(switch_::Switch *sw) { heating_switch_ = sw; }
  void set_ble_connection_switch(switch_::Switch *sw) { ble_connection_switch_ = sw; }
  void set_baby_formula_switch(switch_::Switch *sw) { baby_formula_switch_ = sw; }
  void set_register_switch(switch_::Switch *sw) { register_switch_ = sw; }

  // Public control methods (called by switches/numbers/buttons)
  void set_target_setpoint(float temp_f);
  void set_hold_time(float seconds);
  void set_my_temp(float temp_f);
  void set_baby_formula_enabled(bool enabled);
  void start_heating();
  void stop_heating();
  void enable_ble_connection(bool enable);
  void set_register_enabled(bool enabled);

  // Connection state queries
  bool is_connected() const { return this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED; }
  bool is_ble_enabled() const { return ble_enabled_; }

  // Registration key configuration (16-byte key for hello/reconnect)
  void set_registration_key(const std::array<uint8_t, 16> &key);

  // Protocol version configuration (0 or 1)
  void set_protocol_version(uint8_t version);

  // Climate interface
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

 protected:
  // BLE characteristics
  uint16_t rx_char_handle_{0};
  uint16_t tx_char_handle_{0};
  uint16_t notify_handle_{0};

  // Kettle state (platform-independent logic)
  CosoriKettleState kettle_state_;

  // Connection management
  bool ble_enabled_{true};
  bool registration_sent_{false};
  bool use_register_command_{false};  // Flag to use register (0x80) vs hello (0x81)

  // Entity pointers
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *kettle_setpoint_sensor_{nullptr};
  sensor::Sensor *hold_time_remaining_sensor_{nullptr};
  binary_sensor::BinarySensor *on_base_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *heating_binary_sensor_{nullptr};
  number::Number *target_setpoint_number_{nullptr};
  number::Number *hold_time_number_{nullptr};
  number::Number *my_temp_number_{nullptr};
  switch_::Switch *heating_switch_{nullptr};
  switch_::Switch *ble_connection_switch_{nullptr};
  switch_::Switch *baby_formula_switch_{nullptr};
  switch_::Switch *register_switch_{nullptr};

  // Platform-specific methods
  void send_ble_data_(const uint8_t* data, size_t len);

  // Entity update methods
  void update_entities_();
  void update_sensors_();
  void update_mutable_entities_();
  void update_climate_state_();
};

// ============================================================================
// Helper classes for Number and Switch entities
// ============================================================================

class CosoriKettleNumber : public number::Number, public Component {
 public:
  void set_parent(CosoriKettleBLE *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override {
    if (this->parent_ != nullptr) {
      this->parent_->set_target_setpoint(value);
    }
    this->publish_state(value);
  }

  CosoriKettleBLE *parent_{nullptr};
};

class CosoriKettleHoldTimeNumber : public number::Number, public Component {
 public:
  void set_parent(CosoriKettleBLE *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override {
    if (this->parent_ != nullptr) {
      this->parent_->set_hold_time(value);
    }
    this->publish_state(value);
  }

  CosoriKettleBLE *parent_{nullptr};
};

class CosoriKettleMyTempNumber : public number::Number, public Component {
 public:
  void set_parent(CosoriKettleBLE *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override {
    if (this->parent_ != nullptr) {
      this->parent_->set_my_temp(value);
    }
    this->publish_state(value);
  }

  CosoriKettleBLE *parent_{nullptr};
};

class CosoriKettleHeatingSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(CosoriKettleBLE *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;

    if (state) {
      this->parent_->start_heating();
    } else {
      this->parent_->stop_heating();
    }
    // Note: Don't call publish_state here - the parent will update us via the status frames
  }

  CosoriKettleBLE *parent_{nullptr};
};

class CosoriKettleBLEConnectionSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(CosoriKettleBLE *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;

    this->parent_->enable_ble_connection(state);
    this->publish_state(state);
  }

  CosoriKettleBLE *parent_{nullptr};
};

class CosoriKettleBabyFormulaSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(CosoriKettleBLE *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;

    this->parent_->set_baby_formula_enabled(state);
    // Note: Don't call publish_state here - the parent will update us via the status frames
  }

  CosoriKettleBLE *parent_{nullptr};
};

class CosoriKettleRegisterSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(CosoriKettleBLE *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override {
    if (this->parent_ != nullptr) {
      this->parent_->set_register_enabled(state);
    }
    // Note: Don't call publish_state here - the parent will update us when registration completes
  }

  CosoriKettleBLE *parent_{nullptr};
};

}  // namespace cosori_kettle_ble
}  // namespace esphome

#endif  // USE_ESP32