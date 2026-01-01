#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <functional>
#include "envelope.h"

namespace esphome {
namespace cosori_kettle_ble {

/**
 * Platform-independent kettle state and protocol logic.
 *
 * This class manages:
 * - Kettle state (temperature, mode, settings)
 * - Protocol state (sequence numbers, acknowledgments)
 * - Command state machine
 * - Packet processing (RX/TX)
 * - Buffer management
 *
 * It does NOT contain any ESP32 or ESPHome specific code.
 * All platform-specific operations are handled via callbacks.
 */
class CosoriKettleState {
 public:
  // ============================================================================
  // Configuration and Callbacks
  // ============================================================================

  struct Config {
    std::array<uint8_t, 16> registration_key;
    uint8_t protocol_version;
    bool registration_key_set;

    Config() : registration_key{}, protocol_version(0), registration_key_set(false) {}
  };

  // Callback for sending BLE data chunks
  using SendDataCallback = std::function<void(const uint8_t* data, size_t len)>;

  // ============================================================================
  // Kettle State Structure
  // ============================================================================

  struct KettleState {
    // Temperature and setpoint
    float current_temp_f;
    float kettle_setpoint_f;
    float target_setpoint_f;

    // Hold time
    uint16_t hold_time_seconds;
    uint16_t remaining_hold_time_seconds;

    // Settings
    uint8_t my_temp_f;
    bool baby_formula_enabled;

    // Physical state
    bool on_base;
    bool heating;

    // Connection state
    bool status_received;
    uint8_t no_response_count;

    KettleState() :
      current_temp_f(0.0f),
      kettle_setpoint_f(0.0f),
      target_setpoint_f(212.0f),
      hold_time_seconds(0),
      remaining_hold_time_seconds(0),
      my_temp_f(179),
      baby_formula_enabled(false),
      on_base(false),
      heating(false),
      status_received(false),
      no_response_count(0) {}
  };

  // ============================================================================
  // Command State Machine
  // ============================================================================

  enum class CommandState {
    IDLE,
    HANDSHAKE_START,
    HANDSHAKE_WAIT_CHUNKS,
    HANDSHAKE_POLL,
    HEAT_START,
    HEAT_SET_TEMP,
    HEAT_POLL,
    HEAT_POLL_REPEAT,
    HEAT_COMPLETE,
    STOP,
    STOP_POLL,
    STOP_REPEAT
  };

  // ============================================================================
  // Constructor and Configuration
  // ============================================================================

  CosoriKettleState();
  CosoriKettleState(const Config& config);

  void set_send_data_callback(SendDataCallback callback) { send_data_callback_ = callback; }

  // Update configuration
  void set_registration_key(const std::array<uint8_t, 16>& key);
  void set_protocol_version(uint8_t version) { config_.protocol_version = version; }

  // ============================================================================
  // Data Processing
  // ============================================================================

  // Process incoming BLE notification data
  void process_rx_data(const uint8_t* data, size_t len);

  // Handle BLE write acknowledgment
  void on_write_ack(bool success);

  // ============================================================================
  // Update Loop
  // ============================================================================

  // Call this periodically to process state machine and timeouts
  // now_ms: current time in milliseconds
  // connected: whether BLE is currently connected
  // registration_sent: whether initial registration has been sent
  void update(uint32_t now_ms, bool connected, bool registration_sent);

  // ============================================================================
  // Command Methods
  // ============================================================================

  void start_heating();
  void stop_heating();
  void set_target_setpoint(float temp_f);
  void set_hold_time(uint16_t seconds);
  void set_my_temp(uint8_t temp_f);
  void set_baby_formula_enabled(bool enabled);
  void send_hello(bool use_register_command);

  // Send status request (called by update loop)
  void send_status_request();

  // ============================================================================
  // State Queries
  // ============================================================================

  const KettleState& get_state() const { return state_; }
  KettleState& get_state_mutable() { return state_; }

  CommandState get_command_state() const { return command_state_; }
  bool is_idle() const { return command_state_ == CommandState::IDLE; }
  bool can_send_command() const;

  // Pending update flags (for UI to know when to ignore incoming status updates)
  bool is_pending_hold_time() const { return pending_hold_time_; }
  bool is_pending_my_temp() const { return pending_my_temp_; }
  bool is_pending_baby_formula() const { return pending_baby_formula_; }

  // Clear pending flags (called after status update confirms change)
  void clear_pending_hold_time() { pending_hold_time_ = false; }
  void clear_pending_my_temp() { pending_my_temp_ = false; }
  void clear_pending_baby_formula() { pending_baby_formula_ = false; }

  // Reset state (called on disconnect)
  void reset();

  // Track online status (increments no_response_count)
  void track_online_status();

  // Reset online status (called when data received)
  void reset_online_status();

  // Get last ACK error code
  uint8_t get_last_ack_error_code() const { return last_ack_error_code_; }

  // Get last RX sequence number
  uint8_t get_last_rx_seq() const { return last_rx_seq_; }

 private:
  // ============================================================================
  // Internal State
  // ============================================================================

  Config config_;
  KettleState state_;

  // Callbacks
  SendDataCallback send_data_callback_;

  // Protocol state
  uint8_t last_rx_seq_;
  uint8_t tx_seq_;
  uint8_t last_ack_error_code_;
  bool waiting_for_ack_complete_;
  uint8_t waiting_for_ack_seq_;
  uint8_t last_status_seq_;

  // Static buffers (shared across instances in original, but per-instance here)
  Envelope send_buffer_;
  Envelope recv_buffer_;

  // Chunking state
  size_t send_chunk_index_;
  size_t send_total_chunks_;
  bool waiting_for_write_ack_;

  // Pending update flags
  bool pending_hold_time_;
  bool pending_my_temp_;
  bool pending_baby_formula_;

  // Command state machine
  CommandState command_state_;
  uint32_t command_state_time_;
  uint8_t pending_mode_;
  uint8_t pending_temp_f_;
  bool use_register_command_;

  // ============================================================================
  // Internal Methods - Protocol
  // ============================================================================

  void send_set_my_temp(uint8_t temp_f);
  void send_set_baby_formula(bool enabled);
  void send_set_hold_time(uint16_t seconds);
  void send_set_mode(uint8_t mode, uint8_t temp_f);
  void send_stop();
  void send_request_compact_status(uint8_t seq_base);

  // Send command with payload
  bool send_command(uint8_t seq, const uint8_t* payload, size_t payload_len, bool is_ack = false);

  // Send next chunk
  void send_next_chunk();

  // ============================================================================
  // Internal Methods - Frame Processing
  // ============================================================================

  void process_frame_buffer();
  void parse_compact_status(const uint8_t* payload, size_t len);
  void parse_status_ack(const uint8_t* payload, size_t len);

  // ============================================================================
  // Internal Methods - State Machine
  // ============================================================================

  void process_command_state_machine(uint32_t now_ms);
  uint8_t next_tx_seq();

  // State machine helpers
  inline void transition_state(CommandState new_state, uint32_t now_ms) {
    command_state_ = new_state;
    command_state_time_ = now_ms;
  }

  bool check_timeout_and_idle(uint32_t elapsed, uint32_t timeout_ms, const char* timeout_name);

  // State machine handlers
  void handle_handshake_start(uint32_t now_ms);
  void handle_handshake_wait_chunks(uint32_t now_ms, uint32_t elapsed);
  void handle_handshake_poll(uint32_t now_ms, uint32_t elapsed);
  void handle_heat_set_temp(uint32_t now_ms, uint32_t elapsed);
  void handle_heat_start(uint32_t now_ms, uint32_t elapsed);
  void handle_heat_poll(uint32_t now_ms, uint32_t elapsed);
  void handle_heat_poll_repeat(uint32_t now_ms, uint32_t elapsed);
  void handle_heat_complete(uint32_t now_ms, uint32_t elapsed);
  void handle_stop(uint32_t now_ms);
  void handle_stop_poll(uint32_t now_ms, uint32_t elapsed);
  void handle_stop_repeat(uint32_t now_ms, uint32_t elapsed);
};

}  // namespace cosori_kettle_ble
}  // namespace esphome
