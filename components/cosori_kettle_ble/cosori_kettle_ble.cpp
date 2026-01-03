#include "cosori_kettle_ble.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome {
namespace cosori_kettle_ble {

static const char *const TAG = "cosori_kettle_ble";

// BLE UUIDs
static const char *COSORI_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb";
static const char *COSORI_RX_CHAR_UUID = "0000fff1-0000-1000-8000-00805f9b34fb";
static const char *COSORI_TX_CHAR_UUID = "0000fff2-0000-1000-8000-00805f9b34fb";

// Registration handshake (HELLO_MIN)
static const uint8_t HELLO_MIN_1[] = {0xa5, 0x22, 0x00, 0x24, 0x00, 0x8a, 0x00, 0x81, 0xd1, 0x00,
                                       0x36, 0x34, 0x32, 0x38, 0x37, 0x61, 0x39, 0x31, 0x37, 0x65};
static const uint8_t HELLO_MIN_2[] = {0x37, 0x34, 0x36, 0x61, 0x30, 0x37, 0x33, 0x31, 0x31, 0x36,
                                       0x36, 0x62, 0x37, 0x36, 0x66, 0x34, 0x33, 0x64, 0x35, 0x63};
static const uint8_t HELLO_MIN_3[] = {0x62, 0x62};

void CosoriKettleBLE::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Cosori Kettle BLE...");
  // Initialize state
  this->tx_seq_ = 0;
  this->last_rx_seq_ = 0;
  this->last_status_seq_ = 0;
  this->status_received_ = false;
  this->registration_sent_ = false;
  this->no_response_count_ = 0;

  // Initialize BLE connection switch to ON (enabled by default)
  if (this->ble_connection_switch_ != nullptr) {
    this->ble_connection_switch_->publish_state(true);
  }

  // Initialize climate state (ESPHome climate expects Celsius)
  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_IDLE;
  this->target_temperature = (this->target_setpoint_f_ - 32.0f) * 5.0f / 9.0f;
  this->current_temperature = (this->current_temp_f_ - 32.0f) * 5.0f / 9.0f;
}

void CosoriKettleBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "Cosori Kettle BLE:");
  ESP_LOGCONFIG(TAG, "  MAC Address: %s", this->parent_->address_str());
  ESP_LOGCONFIG(TAG, "  Update Interval: %ums", this->get_update_interval());
  LOG_BINARY_SENSOR("  ", "On Base", this->on_base_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Heating", this->heating_binary_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Kettle Setpoint", this->kettle_setpoint_sensor_);
  LOG_NUMBER("  ", "Target Setpoint", this->target_setpoint_number_);
  LOG_SWITCH("  ", "Heating Control", this->heating_switch_);
  LOG_SWITCH("  ", "BLE Connection", this->ble_connection_switch_);
}

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
      this->frame_buffer_.clear();
      this->registration_sent_ = false;
      this->status_received_ = false;
      this->no_response_count_ = 0;
      this->target_setpoint_initialized_ = false;
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

      // Send registration handshake
      this->send_registration_();

      // Mark registration sent
      this->registration_sent_ = true;
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->rx_char_handle_)
        break;

      // Log full RX packet as hex dump
      std::string hex_str;
      for (uint16_t i = 0; i < param->notify.value_len; i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02x%s", param->notify.value[i], (i < param->notify.value_len - 1) ? ":" : "");
        hex_str += buf;
      }
      ESP_LOGD(TAG, "RX: %s", hex_str.c_str());

      // Append to frame buffer
      this->frame_buffer_.insert(this->frame_buffer_.end(), param->notify.value,
                                 param->notify.value + param->notify.value_len);

      // Process complete frames
      this->process_frame_buffer_();
      break;
    }

    default:
      break;
  }
}

void CosoriKettleBLE::update() {
  this->track_online_status_();

  if (!this->ble_enabled_) {
    return;
  }

  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "Not connected, skipping poll");
    return;
  }

  if (!this->registration_sent_) {
    ESP_LOGD(TAG, "Registration not complete, skipping poll");
    return;
  }

  // Send poll
  this->send_poll_();
}

// ============================================================================
// Protocol Implementation - Packet Sending
// ============================================================================

void CosoriKettleBLE::set_handshake_packet(int index, const std::vector<uint8_t> &data) {
  switch (index) {
    case 0:
      this->custom_hello_1_ = data;
      break;
    case 1:
      this->custom_hello_2_ = data;
      break;
    case 2:
      this->custom_hello_3_ = data;
      break;
    default:
      ESP_LOGW(TAG, "Invalid handshake packet index: %d", index);
      return;
  }
  this->use_custom_handshake_ = true;
  ESP_LOGD(TAG, "Custom handshake packet %d set (%d bytes)", index, data.size());
}

void CosoriKettleBLE::send_registration_() {
  if (this->use_custom_handshake_) {
    ESP_LOGI(TAG, "Sending registration handshake (custom)");
    this->send_packet_(this->custom_hello_1_.data(), this->custom_hello_1_.size());
    delay(80);
    this->send_packet_(this->custom_hello_2_.data(), this->custom_hello_2_.size());
    delay(80);
    this->send_packet_(this->custom_hello_3_.data(), this->custom_hello_3_.size());
  } else {
    ESP_LOGI(TAG, "Sending registration handshake (HELLO_MIN)");
    this->send_packet_(HELLO_MIN_1, sizeof(HELLO_MIN_1));
    delay(80);
    this->send_packet_(HELLO_MIN_2, sizeof(HELLO_MIN_2));
    delay(80);
    this->send_packet_(HELLO_MIN_3, sizeof(HELLO_MIN_3));
  }
  delay(80);

  // Send initial poll
  this->send_poll_();
}

void CosoriKettleBLE::send_poll_() {
  uint8_t seq = this->next_tx_seq_();
  auto pkt = this->make_poll_(seq);
  ESP_LOGV(TAG, "Sending POLL (seq=%02x)", seq);
  this->send_packet_(pkt.data(), pkt.size());
}

void CosoriKettleBLE::send_hello5_() {
  uint8_t seq = this->next_tx_seq_();
  auto pkt = this->make_hello5_(seq);
  ESP_LOGD(TAG, "Sending HELLO5 (seq=%02x)", seq);
  this->send_packet_(pkt.data(), pkt.size());
}

void CosoriKettleBLE::send_setpoint_(uint8_t mode, uint8_t temp_f) {
  uint8_t seq = this->next_tx_seq_();
  auto pkt = this->make_setpoint_(seq, mode, temp_f);
  ESP_LOGD(TAG, "Sending SETPOINT %d°F (seq=%02x, mode=%02x)", temp_f, seq, mode);
  this->send_packet_(pkt.data(), pkt.size());
}

void CosoriKettleBLE::send_f4_() {
  uint8_t seq = this->next_tx_seq_();
  auto pkt = this->make_f4_(seq);
  ESP_LOGD(TAG, "Sending F4 (seq=%02x)", seq);
  this->send_packet_(pkt.data(), pkt.size());
}

void CosoriKettleBLE::send_ctrl_(uint8_t seq_base) {
  auto pkt = this->make_ctrl_(seq_base);
  ESP_LOGD(TAG, "Sending CTRL (seq=%02x)", seq_base);
  this->send_packet_(pkt.data(), pkt.size());
}

void CosoriKettleBLE::send_packet_(const uint8_t *data, size_t len) {
  if (this->tx_char_handle_ == 0) {
    ESP_LOGW(TAG, "TX characteristic not ready");
    return;
  }

  // Log full TX packet as hex dump
  std::string hex_str;
  for (size_t i = 0; i < len; i++) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02x%s", data[i], (i < len - 1) ? ":" : "");
    hex_str += buf;
  }
  ESP_LOGD(TAG, "TX: %s", hex_str.c_str());

  auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                          this->tx_char_handle_, len, const_cast<uint8_t *>(data),
                                          ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending packet, status=%d", status);
  }
}

// ============================================================================
// Packet Builders
// ============================================================================

std::vector<uint8_t> CosoriKettleBLE::build_a5_22_(uint8_t seq, const uint8_t *payload, size_t payload_len,
                                                    uint8_t checksum) {
  std::vector<uint8_t> pkt;
  pkt.push_back(0xA5);
  pkt.push_back(0x22);
  pkt.push_back(seq);
  pkt.push_back(payload_len & 0xFF);
  pkt.push_back((payload_len >> 8) & 0xFF);
  pkt.push_back(checksum);
  pkt.insert(pkt.end(), payload, payload + payload_len);
  return pkt;
}

std::vector<uint8_t> CosoriKettleBLE::build_a5_12_(uint8_t seq, const uint8_t *payload, size_t payload_len,
                                                    uint8_t checksum) {
  std::vector<uint8_t> pkt;
  pkt.push_back(0xA5);
  pkt.push_back(0x12);
  pkt.push_back(seq);
  pkt.push_back(payload_len & 0xFF);
  pkt.push_back((payload_len >> 8) & 0xFF);
  pkt.push_back(checksum);
  pkt.insert(pkt.end(), payload, payload + payload_len);
  return pkt;
}

std::vector<uint8_t> CosoriKettleBLE::make_poll_(uint8_t seq) {
  const uint8_t payload[] = {0x00, 0x40, 0x40, 0x00};
  uint8_t checksum = (0xB4 - seq) & 0xFF;
  return this->build_a5_22_(seq, payload, sizeof(payload), checksum);
}

std::vector<uint8_t> CosoriKettleBLE::make_hello5_(uint8_t seq) {
  const uint8_t payload[] = {0x00, 0xF2, 0xA3, 0x00, 0x00, 0x01, 0x10, 0x0E};
  uint8_t checksum = (0x7C - seq) & 0xFF;
  return this->build_a5_22_(seq, payload, sizeof(payload), checksum);
}

std::vector<uint8_t> CosoriKettleBLE::make_setpoint_(uint8_t seq, uint8_t mode, uint8_t temp_f) {
  const uint8_t payload[] = {0x00, 0xF0, 0xA3, 0x00, mode, temp_f, 0x01, 0x10, 0x0E};
  uint8_t checksum = (0x7D - seq - mode - temp_f) & 0xFF;
  return this->build_a5_22_(seq, payload, sizeof(payload), checksum);
}

std::vector<uint8_t> CosoriKettleBLE::make_f4_(uint8_t seq) {
  const uint8_t payload[] = {0x00, 0xF4, 0xA3, 0x00};
  uint8_t checksum = (0x9D - seq) & 0xFF;
  return this->build_a5_22_(seq, payload, sizeof(payload), checksum);
}

std::vector<uint8_t> CosoriKettleBLE::make_ctrl_(uint8_t seq) {
  uint8_t checksum = (0xC3 - seq) & 0xFF;
  const uint8_t payload[] = {0x00, 0x41, 0x40, 0x00};
  return this->build_a5_12_(seq, payload, sizeof(payload), checksum);
}

// ============================================================================
// Frame Processing
// ============================================================================

void CosoriKettleBLE::process_frame_buffer_() {
  while (true) {
    // Find frame start (0xA5)
    size_t start_idx = 0;
    while (start_idx < this->frame_buffer_.size() && this->frame_buffer_[start_idx] != 0xA5) {
      start_idx++;
    }

    // Discard any bytes before frame start
    if (start_idx > 0) {
      this->frame_buffer_.erase(this->frame_buffer_.begin(), this->frame_buffer_.begin() + start_idx);
    }

    // Need at least 6 bytes for header
    if (this->frame_buffer_.size() < 6)
      break;

    // Parse header
    uint8_t frame_type = this->frame_buffer_[1];
    uint8_t seq = this->frame_buffer_[2];
    uint16_t payload_len = this->frame_buffer_[3] | (this->frame_buffer_[4] << 8);
    size_t frame_len = 6 + payload_len;

    // Wait for complete frame
    if (this->frame_buffer_.size() < frame_len)
      break;

    // Extract payload
    const uint8_t *payload = this->frame_buffer_.data() + 6;

    // Update last RX sequence
    this->last_rx_seq_ = seq;

    // Parse based on frame type
    if (frame_type == 0x22) {
      // Compact status (A5 22)
      this->parse_compact_status_(payload, payload_len);
    } else if (frame_type == 0x12) {
      // Extended status (A5 12)
      this->parse_extended_status_(payload, payload_len);
    }

    // Remove processed frame
    this->frame_buffer_.erase(this->frame_buffer_.begin(), this->frame_buffer_.begin() + frame_len);
  }
}

void CosoriKettleBLE::parse_compact_status_(const uint8_t *payload, size_t len) {
  // Compact status: 01 41 40 00 <stage> <mode> <sp> <temp> <status> ...
  if (len < 9 || payload[0] != 0x01 || payload[1] != 0x41)
    return;

  uint8_t stage = payload[4];     // Heating stage
  uint8_t mode = payload[5];      // Mode
  uint8_t sp = payload[6];        // Setpoint temperature
  uint8_t temp = payload[7];      // Current temperature
  uint8_t status = payload[8];    // Heating status

  // Validate temperature range
  if (temp < 40 || temp > 230)
    return;

  // Update state (temp, setpoint, heating only - no on-base detection from compact packets)
  this->current_temp_f_ = temp;
  this->kettle_setpoint_f_ = sp;
  this->heating_ = (status != 0);
  this->status_received_ = true;
  this->last_status_seq_ = this->last_rx_seq_;

  // Reset offline counter
  this->reset_online_status_();

  // Update entities
  this->update_entities_();
}

void CosoriKettleBLE::parse_extended_status_(const uint8_t *payload, size_t len) {
  // Extended status: 01 40 40 00 <stage> <mode> <sp> <temp> ... <on_base> ...
  // NOTE: Extended packets (A5 12, len=29) contain on-base detection at payload[14] (byte 20)
  // Compact packets (A5 22, len=12) do NOT contain on-base information
  if (len < 8 || payload[0] != 0x01 || payload[1] != 0x40)
    return;

  uint8_t stage = payload[4];
  uint8_t mode = payload[5];
  uint8_t sp = payload[6];
  uint8_t temp = payload[7];

  // Validate temperature range
  if (temp < 40 || temp > 230)
    return;

  // Update state (temp, setpoint, heating)
  this->current_temp_f_ = temp;
  this->kettle_setpoint_f_ = sp;
  this->heating_ = (stage != 0);
  this->status_received_ = true;
  this->last_status_seq_ = this->last_rx_seq_;

  // On-base detection from payload[14] (byte 20 in full packet)
  if (len >= 15) {
    uint8_t on_base_byte = payload[14];
    bool prev_on_base = this->on_base_;
    this->on_base_ = (on_base_byte == 0x00);  // 0x00=on-base, 0x01=off-base

    if (prev_on_base != this->on_base_) {
      ESP_LOGI(TAG, "On-base: %s (payload[14]=0x%02x)",
               this->on_base_ ? "ON" : "OFF", on_base_byte);
    }
  }

  // Reset offline counter
  this->reset_online_status_();

  // Update entities
  this->update_entities_();
}

// ============================================================================
// Public Control Methods
// ============================================================================

void CosoriKettleBLE::set_target_setpoint(float temp_f) {
  // Clamp to valid range
  if (temp_f < 104)
    temp_f = 104;
  if (temp_f > 212)
    temp_f = 212;

  this->target_setpoint_f_ = temp_f;
  ESP_LOGI(TAG, "Target setpoint changed to %.0f°F", temp_f);
}

void CosoriKettleBLE::start_heating() {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot start heating: not connected");
    return;
  }

  uint8_t temp_f = (uint8_t) this->target_setpoint_f_;
  uint8_t mode = (temp_f == 212) ? 0x04 : 0x06;

  ESP_LOGI(TAG, "Starting kettle at %.0f°F", this->target_setpoint_f_);

  // Send HELLO5
  this->send_hello5_();
  delay(60);

  // Send SETPOINT
  this->send_setpoint_(mode, temp_f);
  delay(100);

  // Wait for status (or timeout)
  uint32_t start = millis();
  while (millis() - start < 2000) {
    // Process any pending notifications
    App.feed_wdt();
    delay(50);
    if (this->status_received_)
      break;
  }

  // Send START control
  uint8_t seq_base = (this->last_status_seq_ != 0) ? this->last_status_seq_ : this->last_rx_seq_;
  this->send_ctrl_(seq_base);
  delay(50);

  // Send reinforce control
  uint8_t seq_ack = this->next_tx_seq_();
  this->send_ctrl_(seq_ack);
}

void CosoriKettleBLE::stop_heating() {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot stop heating: not connected");
    return;
  }

  ESP_LOGI(TAG, "Stopping kettle");

  // Send PRE_STOP (F4)
  this->send_f4_();
  delay(50);

  // Send CTRL(stop)
  uint8_t seq_ctrl = (this->last_status_seq_ != 0) ? this->last_status_seq_ : this->last_rx_seq_;
  this->send_ctrl_(seq_ctrl);
  delay(50);

  // Send POST_STOP (F4)
  this->send_f4_();
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

// ============================================================================
// Climate Interface
// ============================================================================

climate::ClimateTraits CosoriKettleBLE::traits() {
  auto traits = climate::ClimateTraits();

  // Temperature range in Celsius (ESPHome expects Celsius)
  // 104°F = 40°C, 212°F = 100°C
  traits.set_visual_min_temperature(40.0f);
  traits.set_visual_max_temperature(100.0f);
  traits.set_visual_temperature_step(0.5f);

  // Supported modes
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_HEAT,
  });

  // Supported features
  traits.add_supported_feature(climate::ClimateFeature::CLIMATE_FEATURE_CURRENT_TEMPERATURE);
  traits.add_supported_feature(climate::ClimateFeature::CLIMATE_FEATURE_ACTION);

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
    this->target_setpoint_f_ = temp_f;

    // Update number entity if it exists
    if (this->target_setpoint_number_ != nullptr) {
      this->target_setpoint_number_->publish_state(temp_f);
    }

    // If in heat mode, apply the new temperature
    if (this->mode == climate::CLIMATE_MODE_HEAT) {
      this->start_heating();
    }
  }

  // Publish updated state
  this->publish_state();
}

// ============================================================================
// State Management
// ============================================================================

uint8_t CosoriKettleBLE::next_tx_seq_() {
  if (this->tx_seq_ == 0 && this->last_rx_seq_ != 0) {
    this->tx_seq_ = (this->last_rx_seq_ + 1) & 0xFF;
  } else {
    this->tx_seq_ = (this->tx_seq_ + 1) & 0xFF;
  }
  return this->tx_seq_;
}

void CosoriKettleBLE::update_entities_() {
  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(this->current_temp_f_);
  }

  if (this->kettle_setpoint_sensor_ != nullptr) {
    this->kettle_setpoint_sensor_->publish_state(this->kettle_setpoint_f_);
  }

  // Initialize target setpoint number with kettle's setpoint on first status
  if (this->target_setpoint_number_ != nullptr && !this->target_setpoint_initialized_) {
    this->target_setpoint_f_ = this->kettle_setpoint_f_;
    this->target_setpoint_number_->publish_state(this->target_setpoint_f_);
    this->target_setpoint_initialized_ = true;
    ESP_LOGI(TAG, "Initialized target setpoint to %d°F from kettle", (int)this->target_setpoint_f_);
  }

  if (this->on_base_binary_sensor_ != nullptr) {
    this->on_base_binary_sensor_->publish_state(this->on_base_);
  }

  if (this->heating_binary_sensor_ != nullptr) {
    this->heating_binary_sensor_->publish_state(this->heating_);
  }

  // Update heating switch state (without triggering action)
  if (this->heating_switch_ != nullptr) {
    this->heating_switch_->publish_state(this->heating_);
  }

  // Update climate state
  this->update_climate_state_();
}

void CosoriKettleBLE::update_climate_state_() {
  // Update current temperature (convert F to C for ESPHome climate)
  this->current_temperature = (this->current_temp_f_ - 32.0f) * 5.0f / 9.0f;

  // Initialize target temperature from kettle on first status
  if (!this->target_setpoint_initialized_) {
    this->target_setpoint_f_ = this->kettle_setpoint_f_;
    this->target_temperature = (this->kettle_setpoint_f_ - 32.0f) * 5.0f / 9.0f;
    this->target_setpoint_initialized_ = true;
    ESP_LOGI(TAG, "Climate: Initialized target temperature to %.0f°F (%.1f°C) from kettle",
             this->target_setpoint_f_, this->target_temperature);
  }

  // Update mode based on heating state and base status
  if (!this->on_base_) {
    // Off base - set to OFF mode
    this->mode = climate::CLIMATE_MODE_OFF;
    this->action = climate::CLIMATE_ACTION_IDLE;
  } else if (this->heating_) {
    // On base and heating
    this->mode = climate::CLIMATE_MODE_HEAT;
    this->action = climate::CLIMATE_ACTION_HEATING;
  } else {
    // On base but not heating - set to OFF mode
    this->mode = climate::CLIMATE_MODE_OFF;
    this->action = climate::CLIMATE_ACTION_IDLE;
  }

  // Publish climate state
  this->publish_state();
}

void CosoriKettleBLE::track_online_status_() {
  if (this->no_response_count_ < 10) {
    this->no_response_count_++;
  }

  if (this->no_response_count_ >= 10 && this->status_received_) {
    ESP_LOGW(TAG, "No response from kettle, marking offline");
    this->status_received_ = false;
    // Publish unavailable state
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(NAN);
    if (this->kettle_setpoint_sensor_ != nullptr)
      this->kettle_setpoint_sensor_->publish_state(NAN);
  }
}

void CosoriKettleBLE::reset_online_status_() {
  this->no_response_count_ = 0;
}

}  // namespace cosori_kettle_ble
}  // namespace esphome

#endif  // USE_ESP32
