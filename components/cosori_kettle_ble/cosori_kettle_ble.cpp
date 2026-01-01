#include "cosori_kettle_ble.h"
#include "cosori_kettle_state.h"
#include "protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cmath>
#include <cstring>

#ifdef USE_ESP32

namespace esphome {
namespace cosori_kettle_ble {

static const char *const TAG = "cosori_kettle_ble";

// Online/offline tracking
static constexpr uint8_t NO_RESPONSE_THRESHOLD = 10;

// BLE UUIDs
static const char *COSORI_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb";
static const char *COSORI_RX_CHAR_UUID = "0000fff1-0000-1000-8000-00805f9b34fb";
static const char *COSORI_TX_CHAR_UUID = "0000fff2-0000-1000-8000-00805f9b34fb";

std::string bytes_to_hex_string(const uint8_t *data, size_t len) {
  std::string hex_str;
  hex_str.reserve(len * 3);  // Pre-allocate: "xx:" per byte
  for (size_t i = 0; i < len; i++) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02x%s", data[i], (i < len - 1) ? ":" : "");
    hex_str += buf;
  }
  return hex_str;
}

// ============================================================================
// Setup and Configuration
// ============================================================================

void CosoriKettleBLE::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Cosori Kettle BLE...");

  // Set callbacks
  kettle_state_.set_send_data_callback([this](const uint8_t* data, size_t len) {
    this->send_ble_data_(data, len);
  });

  // Initialize BLE connection switch to ON (enabled by default)
  if (this->ble_connection_switch_ != nullptr) {
    this->ble_connection_switch_->publish_state(true);
  }

  // Initialize climate state (ESPHome climate expects Celsius)
  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_IDLE;
  const auto& state = kettle_state_.get_state();
  this->target_temperature = (state.target_setpoint_f - 32.0f) * 5.0f / 9.0f;
  this->current_temperature = (state.current_temp_f - 32.0f) * 5.0f / 9.0f;
}

void CosoriKettleBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "Cosori Kettle BLE:");
  ESP_LOGCONFIG(TAG, "  MAC Address: %s", this->parent_->address_str());
  ESP_LOGCONFIG(TAG, "  Update Interval: %ums", this->get_update_interval());
  LOG_BINARY_SENSOR("  ", "On Base", this->on_base_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Heating", this->heating_binary_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Kettle Setpoint", this->kettle_setpoint_sensor_);
  LOG_SENSOR("  ", "Hold Time Remaining", this->hold_time_remaining_sensor_);
  LOG_NUMBER("  ", "Target Setpoint", this->target_setpoint_number_);
  LOG_NUMBER("  ", "Hold Time", this->hold_time_number_);
  LOG_NUMBER("  ", "My Temp", this->my_temp_number_);
  LOG_SWITCH("  ", "Heating Control", this->heating_switch_);
  LOG_SWITCH("  ", "BLE Connection", this->ble_connection_switch_);
  LOG_SWITCH("  ", "Baby Formula", this->baby_formula_switch_);
  LOG_SWITCH("  ", "Register", this->register_switch_);
}

void CosoriKettleBLE::update() {
  if (!this->ble_enabled_) {
    return;
  }

  // Delegate to kettle state
  kettle_state_.update(millis(), this->is_connected(), this->registration_sent_);

  // Update entities based on current state
  update_entities_();
}

// ============================================================================
// BLE Event Handler
// ============================================================================

void CosoriKettleBLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                           esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      ESP_LOGI(TAG, "BLE connection opened");
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGW(TAG, "BLE disconnected");
      this->node_state = esp32_ble_tracker::ClientState::IDLE;
      this->rx_char_handle_ = 0;
      this->tx_char_handle_ = 0;
      this->notify_handle_ = 0;
      kettle_state_.reset();
      this->registration_sent_ = false;
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGI(TAG, "Service search complete");

      // These UUIDs are 16-bit UUIDs in Bluetooth base UUID format
      // 0000fff0-0000-1000-8000-00805f9b34fb = 0xfff0
      auto service_uuid = esp32_ble_tracker::ESPBTUUID::from_uint16(0xfff0);
      auto rx_uuid = esp32_ble_tracker::ESPBTUUID::from_uint16(0xfff1);
      auto tx_uuid = esp32_ble_tracker::ESPBTUUID::from_uint16(0xfff2);

      // Get RX characteristic (for notifications)
      auto *rx_chr = this->parent_->get_characteristic(service_uuid, rx_uuid);
      if (rx_chr == nullptr) {
        ESP_LOGE(TAG, "RX characteristic not found");
        break;
      }
      this->rx_char_handle_ = rx_chr->handle;

      // Get TX characteristic (for writes)
      auto *tx_chr = this->parent_->get_characteristic(service_uuid, tx_uuid);
      if (tx_chr == nullptr) {
        ESP_LOGE(TAG, "TX characteristic not found");
        break;
      }
      this->tx_char_handle_ = tx_chr->handle;

      // Register for notifications
      auto status = esp_ble_gattc_register_for_notify(gattc_if, this->parent_->get_remote_bda(), rx_chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }

      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "Registered for notifications, sending registration handshake");

      // Send registration handshake via kettle state
      kettle_state_.send_hello(this->use_register_command_);

      // Mark registration sent
      this->registration_sent_ = true;
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      // Handle write acknowledgment for chunked packets
      if (param->write.handle == this->tx_char_handle_) {
        bool success = (param->write.status == ESP_GATT_OK);
        if (!success) {
          ESP_LOGW(TAG, "Write failed, status=%d", param->write.status);
        }
        kettle_state_.on_write_ack(success);
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->rx_char_handle_)
        break;

      #ifdef ESPHOME_LOG_HAS_DEBUG
        ESP_LOGD(TAG, "RX: %s", bytes_to_hex_string(param->notify.value, param->notify.value_len).c_str());
      #endif

      // Delegate RX data processing to kettle state
      kettle_state_.process_rx_data(param->notify.value, param->notify.value_len);
      // Update entities after processing
      update_entities_();
      break;
    }

    default:
      break;
  }
}

// ============================================================================
// Platform-Specific Methods (Callbacks for CosoriKettleState)
// ============================================================================

void CosoriKettleBLE::send_ble_data_(const uint8_t* data, size_t len) {
  if (this->tx_char_handle_ == 0) {
    ESP_LOGW(TAG, "TX characteristic not ready");
    return;
  }

  auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                          this->tx_char_handle_, len,
                                          const_cast<uint8_t *>(data),
                                          ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending BLE data, status=%d", status);
  } else {
    #ifdef ESPHOME_LOG_HAS_DEBUG
      ESP_LOGD(TAG, "Sent BLE data (%zu bytes): %s", len, bytes_to_hex_string(data, len).c_str());
    #endif
  }
}

// ============================================================================
// Public Control Methods
// ============================================================================

void CosoriKettleBLE::set_target_setpoint(float temp_f) {
  kettle_state_.set_target_setpoint(temp_f);

  // Update number entity if it exists
  if (this->target_setpoint_number_ != nullptr) {
    this->target_setpoint_number_->publish_state(temp_f);
  }
}

void CosoriKettleBLE::set_register_enabled(bool enabled) {
  if (this->register_switch_ != nullptr) {
    this->use_register_command_ = enabled;
    this->register_switch_->publish_state(enabled);
  }

  if (enabled) {
    ESP_LOGI(TAG, "Registering device with kettle");
  } else {
    ESP_LOGI(TAG, "Sending hello command");
  }
  kettle_state_.send_hello(enabled);
}

void CosoriKettleBLE::set_hold_time(float seconds) {
  // Clamp to valid range (0-65535 seconds)
  if (seconds < 0.0f) seconds = 0.0f;
  if (seconds > 65535.0f) seconds = 65535.0f;

  uint16_t seconds_int = static_cast<uint16_t>(std::round(seconds));

  // Update number entity if it exists
  if (this->hold_time_number_ != nullptr) {
    this->hold_time_number_->publish_state(seconds);
  }

  // Delegate to kettle state (will handle sending command if connected)
  if (this->is_connected()) {
    kettle_state_.set_hold_time(seconds_int);
  }
}

void CosoriKettleBLE::set_my_temp(float temp_f) {
  // Clamp to valid range
  if (temp_f < MIN_TEMP_F) temp_f = MIN_TEMP_F;
  if (temp_f > MAX_TEMP_F) temp_f = MAX_TEMP_F;

  uint8_t temp_int = static_cast<uint8_t>(std::round(temp_f));

  // Update number entity if it exists
  if (this->my_temp_number_ != nullptr) {
    this->my_temp_number_->publish_state(temp_f);
  }

  // Delegate to kettle state (will handle sending command if connected)
  if (this->is_connected()) {
    kettle_state_.set_my_temp(temp_int);
  }
}

void CosoriKettleBLE::set_baby_formula_enabled(bool enabled) {
  // Update switch entity if it exists
  if (this->baby_formula_switch_ != nullptr) {
    this->baby_formula_switch_->publish_state(enabled);
  }

  // Delegate to kettle state (will handle sending command if connected)
  if (this->is_connected()) {
    kettle_state_.set_baby_formula_enabled(enabled);
  }
}

void CosoriKettleBLE::start_heating() {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot start heating: not connected");
    return;
  }

  kettle_state_.start_heating();
}

void CosoriKettleBLE::stop_heating() {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot stop heating: not connected");
    return;
  }

  kettle_state_.stop_heating();
}

void CosoriKettleBLE::enable_ble_connection(bool enable) {
  this->ble_enabled_ = enable;

  if (!enable && this->is_connected()) {
    ESP_LOGI(TAG, "Disabling BLE connection");
    // Disconnect
    this->parent_->set_enabled(false);
  } else if (enable && !this->is_connected()) {
    ESP_LOGI(TAG, "Enabling BLE connection");
    // Reconnect
    this->parent_->set_enabled(true);
  }

  // Update switch state to reflect actual setting
  if (this->ble_connection_switch_ != nullptr) {
    this->ble_connection_switch_->publish_state(enable);
  }
}

void CosoriKettleBLE::set_registration_key(const std::array<uint8_t, 16> &key) {
  kettle_state_.set_registration_key(key);
  ESP_LOGD(TAG, "Registration key set");
}

void CosoriKettleBLE::set_protocol_version(uint8_t version) {
  kettle_state_.set_protocol_version(version);
}

// ============================================================================
// Climate Interface
// ============================================================================

climate::ClimateTraits CosoriKettleBLE::traits() {
  auto traits = climate::ClimateTraits();

  // Temperature range in Celsius (ESPHome expects Celsius)
  // 104°F = 40°C, 212°F = 100°C
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(40.0f);
  traits.set_visual_max_temperature(100.0f);
  traits.set_visual_temperature_step(0.5f);

  // Supported modes
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_HEAT,
  });

  // Supported actions
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);

  return traits;
}

void CosoriKettleBLE::control(const climate::ClimateCall &call) {
  // Handle mode change
  if (call.get_mode().has_value()) {
    climate::ClimateMode mode = *call.get_mode();

    if (mode == climate::CLIMATE_MODE_OFF) {
      ESP_LOGI(TAG, "Climate: Setting mode to OFF");
      this->stop_heating();
      this->mode = climate::CLIMATE_MODE_OFF;
    } else if (mode == climate::CLIMATE_MODE_HEAT) {
      ESP_LOGI(TAG, "Climate: Setting mode to HEAT");
      this->mode = climate::CLIMATE_MODE_HEAT;
      // Start heating if we have a target temperature
      if (this->target_temperature > 0) {
        this->start_heating();
      }
    }
  }

  // Handle target temperature change
  if (call.get_target_temperature().has_value()) {
    float temp_c = *call.get_target_temperature();
    // Convert Celsius to Fahrenheit for the kettle
    float temp_f = temp_c * 9.0f / 5.0f + 32.0f;
    ESP_LOGI(TAG, "Climate: Setting target temperature to %.1f°C (%.0f°F)", temp_c, temp_f);
    this->target_temperature = temp_c;

    // Update kettle state
    this->set_target_setpoint(temp_f);

    // If in heat mode, apply the new temperature
    if (this->mode == climate::CLIMATE_MODE_HEAT) {
      this->start_heating();
    }
  }

  // Publish updated state
  this->publish_state();
}

// ============================================================================
// State Management - Entity Updates
// ============================================================================

void CosoriKettleBLE::update_sensors_() {
  const auto& state = kettle_state_.get_state();

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(state.current_temp_f);
  }

  if (this->kettle_setpoint_sensor_ != nullptr) {
    this->kettle_setpoint_sensor_->publish_state(state.kettle_setpoint_f);
  }

  if (this->hold_time_remaining_sensor_ != nullptr) {
    this->hold_time_remaining_sensor_->publish_state(static_cast<float>(state.remaining_hold_time_seconds));
  }

  if (this->on_base_binary_sensor_ != nullptr) {
    this->on_base_binary_sensor_->publish_state(state.on_base);
  }

  if (this->heating_binary_sensor_ != nullptr) {
    this->heating_binary_sensor_->publish_state(state.heating);
  }
}

void CosoriKettleBLE::update_mutable_entities_() {
  const auto& state = kettle_state_.get_state();

  if (!kettle_state_.is_idle()) {
    return;
  }

  if (this->target_setpoint_number_ != nullptr) {
    this->target_setpoint_number_->publish_state(state.target_setpoint_f);
  }

  if (this->hold_time_number_ != nullptr && !kettle_state_.is_pending_hold_time()) {
    this->hold_time_number_->publish_state(static_cast<float>(state.hold_time_seconds));
  }

  if (this->my_temp_number_ != nullptr && !kettle_state_.is_pending_my_temp()) {
    this->my_temp_number_->publish_state(static_cast<float>(state.my_temp_f));
  }

  if (this->baby_formula_switch_ != nullptr && !kettle_state_.is_pending_baby_formula()) {
    this->baby_formula_switch_->publish_state(state.baby_formula_enabled);
  }

  if (this->heating_switch_ != nullptr) {
    this->heating_switch_->publish_state(state.heating);
  }
}

void CosoriKettleBLE::update_entities_() {
  update_sensors_();
  update_mutable_entities_();
  update_climate_state_();

  // Handle offline status
  const auto& state = kettle_state_.get_state();
  if (state.no_response_count >= NO_RESPONSE_THRESHOLD && state.status_received) {
    ESP_LOGW(TAG, "No response from kettle, marking offline");

    // Publish unavailable state
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(NAN);
    if (this->kettle_setpoint_sensor_ != nullptr)
      this->kettle_setpoint_sensor_->publish_state(NAN);
    if (this->hold_time_remaining_sensor_ != nullptr)
      this->hold_time_remaining_sensor_->publish_state(NAN);
    if (this->on_base_binary_sensor_ != nullptr)
      this->on_base_binary_sensor_->invalidate_state();
    if (this->heating_binary_sensor_ != nullptr)
      this->heating_binary_sensor_->invalidate_state();

    if (this->hold_time_number_ != nullptr)
      this->hold_time_number_->publish_state(NAN);
    if (this->my_temp_number_ != nullptr)
      this->my_temp_number_->publish_state(NAN);
    if (this->target_setpoint_number_ != nullptr)
      this->target_setpoint_number_->publish_state(NAN);
  }
}

void CosoriKettleBLE::update_climate_state_() {
  const auto& state = kettle_state_.get_state();

  // Update current temperature (convert F to C for ESPHome climate)
  this->current_temperature = (state.current_temp_f - 32.0f) * 5.0f / 9.0f;

  // Initialize target temperature from kettle on first status
  // Use number entity's has_state() to determine if initialized
  bool target_initialized = (this->target_setpoint_number_ != nullptr) &&
                            this->target_setpoint_number_->has_state();
  if (!target_initialized) {
    this->target_temperature = (state.kettle_setpoint_f - 32.0f) * 5.0f / 9.0f;
    ESP_LOGI(TAG, "Climate: Initialized target temperature to %.0f°F (%.1f°C) from kettle",
             state.kettle_setpoint_f, this->target_temperature);
  }

  if (state.on_base && state.heating) {
    this->mode = climate::CLIMATE_MODE_HEAT;
    this->action = climate::CLIMATE_ACTION_HEATING;
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->action = climate::CLIMATE_ACTION_IDLE;
  }

  // Publish climate state
  this->publish_state();
}

}  // namespace cosori_kettle_ble
}  // namespace esphome

#endif  // USE_ESP32
