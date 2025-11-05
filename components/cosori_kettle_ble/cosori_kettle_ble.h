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

  // Binary sensor setters
  void set_on_base_binary_sensor(binary_sensor::BinarySensor *sensor) { on_base_binary_sensor_ = sensor; }
  void set_heating_binary_sensor(binary_sensor::BinarySensor *sensor) { heating_binary_sensor_ = sensor; }

  // Number setter (target setpoint)
  void set_target_setpoint_number(number::Number *number) { target_setpoint_number_ = number; }

  // Switch setters
  void set_heating_switch(switch_::Switch *sw) { heating_switch_ = sw; }
  void set_ble_connection_switch(switch_::Switch *sw) { ble_connection_switch_ = sw; }

  // Public control methods (called by switches/numbers)
  void set_target_setpoint(float temp_f);
  void start_heating();
  void stop_heating();
  void enable_ble_connection(bool enable);

  // Connection state queries
  bool is_connected() const { return this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED; }
  bool is_ble_enabled() const { return ble_enabled_; }

  // Climate interface
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

 protected:
  // BLE characteristics
  uint16_t rx_char_handle_{0};
  uint16_t tx_char_handle_{0};
  uint16_t notify_handle_{0};

  // Protocol state
  uint8_t last_rx_seq_{0};
  uint8_t tx_seq_{0};
  uint8_t last_status_seq_{0};
  bool status_received_{false};
  std::vector<uint8_t> frame_buffer_;

  // Kettle state
  float current_temp_f_{0.0};
  float kettle_setpoint_f_{0.0};
  float target_setpoint_f_{212.0};
  bool on_base_{false};
  bool heating_{false};

  // Connection management
  bool ble_enabled_{true};
  uint8_t no_response_count_{0};
  uint32_t last_poll_time_{0};
  bool registration_sent_{false};
  bool target_setpoint_initialized_{false};

  // Entity pointers
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *kettle_setpoint_sensor_{nullptr};
  binary_sensor::BinarySensor *on_base_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *heating_binary_sensor_{nullptr};
  number::Number *target_setpoint_number_{nullptr};
  switch_::Switch *heating_switch_{nullptr};
  switch_::Switch *ble_connection_switch_{nullptr};

  // Protocol methods
  void send_registration_();
  void send_poll_();
  void send_hello5_();
  void send_setpoint_(uint8_t mode, uint8_t temp_f);
  void send_f4_();
  void send_ctrl_(uint8_t seq_base);
  void send_packet_(const uint8_t *data, size_t len);

  // Packet builders
  std::vector<uint8_t> build_a5_22_(uint8_t seq, const uint8_t *payload, size_t payload_len, uint8_t checksum);
  std::vector<uint8_t> build_a5_12_(uint8_t seq, const uint8_t *payload, size_t payload_len, uint8_t checksum);
  std::vector<uint8_t> make_poll_(uint8_t seq);
  std::vector<uint8_t> make_hello5_(uint8_t seq);
  std::vector<uint8_t> make_setpoint_(uint8_t seq, uint8_t mode, uint8_t temp_f);
  std::vector<uint8_t> make_f4_(uint8_t seq);
  std::vector<uint8_t> make_ctrl_(uint8_t seq);

  // Frame parsing
  void process_frame_buffer_();
  void parse_compact_status_(const uint8_t *payload, size_t len);
  void parse_extended_status_(const uint8_t *payload, size_t len);

  // State management
  uint8_t next_tx_seq_();
  void update_entities_();
  void update_climate_state_();
  void track_online_status_();
  void reset_online_status_();
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

}  // namespace cosori_kettle_ble
}  // namespace esphome

#endif  // USE_ESP32
