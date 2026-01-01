#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace esphome {
namespace cosori_kettle_ble {

// Protocol constants
static constexpr uint8_t PROTOCOL_VERSION_V0 = 0x00;
static constexpr uint8_t PROTOCOL_VERSION_V1 = 0x01;

// Command IDs
static constexpr uint8_t CMD_REGISTER = 0x80;
static constexpr uint8_t CMD_HELLO = 0x81;
static constexpr uint8_t CMD_POLL = 0x40;
static constexpr uint8_t CMD_CTRL = 0x41;
static constexpr uint8_t CMD_SET_MODE = 0xF0;
static constexpr uint8_t CMD_SET_HOLD_TIME = 0xF2;
static constexpr uint8_t CMD_SET_MY_TEMP = 0xF3;
static constexpr uint8_t CMD_STOP = 0xF4;
static constexpr uint8_t CMD_SET_BABY_FORMULA = 0xF5;

// Command types
static constexpr uint8_t CMD_TYPE_D1 = 0xD1;  // Hello/registration
static constexpr uint8_t CMD_TYPE_A3 = 0xA3;  // Control commands
static constexpr uint8_t CMD_TYPE_40 = 0x40;  // Status requests

// Temperature limits (Fahrenheit)
static constexpr uint8_t MIN_TEMP_F = 104;
static constexpr uint8_t MAX_TEMP_F = 212;
static constexpr uint8_t MIN_VALID_READING_F = 40;
static constexpr uint8_t MAX_VALID_READING_F = 230;

// Operating modes
static constexpr uint8_t MODE_BOIL = 0x04;
static constexpr uint8_t MODE_HEAT = 0x06;
static constexpr uint8_t MODE_GREEN_TEA = 0x01;
static constexpr uint8_t MODE_GREEN_TEA_F = 180;
static constexpr uint8_t MODE_OOLONG = 0x02;
static constexpr uint8_t MODE_OOLONG_F = 195;
static constexpr uint8_t MODE_COFFEE = 0x03;
static constexpr uint8_t MODE_COFFEE_F = 205;
static constexpr uint8_t MODE_MY_TEMP = 0x05;

// Status packet structures
struct CompactStatus {
  uint8_t stage;      // Heating stage
  uint8_t mode;       // Operating mode
  uint8_t setpoint;   // Setpoint temperature (°F)
  uint8_t temp;       // Current temperature (°F)
  bool valid;         // Whether the status is valid
};

struct ExtendedStatus {
  uint8_t stage;                 // Heating stage
  uint8_t mode;                  // Operating mode
  uint8_t setpoint;              // Setpoint temperature (°F)
  uint8_t temp;                  // Current temperature (°F)
  uint8_t my_temp;               // My temp setting (°F)
  uint16_t configured_hold_time; // Total hold time (seconds)
  uint16_t remaining_hold_time;  // Remaining hold time (seconds)
  bool on_base;                  // On base status
  bool baby_formula_enabled;     // Baby formula mode
  bool valid;                    // Whether the status is valid
};

// ============================================================================
// Packet Generation Functions
// ============================================================================

/**
 * Build register/pairing payload
 * @param protocol_version Protocol version (0 or 1)
 * @param registration_key 16-byte registration key
 * @param payload Output buffer (must be at least 36 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_register_payload(uint8_t protocol_version, 
                              const std::array<uint8_t, 16> &registration_key,
                              uint8_t *payload);

/**
 * Build hello/reconnect payload
 * @param protocol_version Protocol version (0 or 1)
 * @param registration_key 16-byte registration key
 * @param payload Output buffer (must be at least 36 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_hello_payload(uint8_t protocol_version, 
                           const std::array<uint8_t, 16> &registration_key,
                           uint8_t *payload);

/**
 * Build status request (POLL) payload
 * @param protocol_version Protocol version (0 or 1)
 * @param payload Output buffer (must be at least 4 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_status_request_payload(uint8_t protocol_version, uint8_t *payload);

/**
 * Build compact status request (CTRL) payload
 * @param protocol_version Protocol version (0 or 1)
 * @param payload Output buffer (must be at least 4 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_compact_status_request_payload(uint8_t protocol_version, uint8_t *payload);

/**
 * Build set my temp payload
 * @param protocol_version Protocol version (0 or 1)
 * @param temp_f Temperature in Fahrenheit
 * @param payload Output buffer (must be at least 5 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_set_my_temp_payload(uint8_t protocol_version, uint8_t temp_f, uint8_t *payload);

/**
 * Build set baby formula payload
 * @param protocol_version Protocol version (0 or 1)
 * @param enabled Whether baby formula mode is enabled
 * @param payload Output buffer (must be at least 5 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_set_baby_formula_payload(uint8_t protocol_version, bool enabled, uint8_t *payload);

/**
 * Build set hold time payload
 * @param protocol_version Protocol version (0 or 1)
 * @param seconds Hold time in seconds
 * @param payload Output buffer (must be at least 8 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_set_hold_time_payload(uint8_t protocol_version, uint16_t seconds, uint8_t *payload);

/**
 * Build set mode payload
 * @param protocol_version Protocol version (0 or 1)
 * @param mode Operating mode
 * @param temp_f Target temperature in Fahrenheit
 * @param hold_time_seconds Hold time in seconds
 * @param payload Output buffer (must be at least 9 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_set_mode_payload(uint8_t protocol_version, uint8_t mode, uint8_t temp_f,
                              uint16_t hold_time_seconds, uint8_t *payload);

/**
 * Build stop payload
 * @param protocol_version Protocol version (0 or 1)
 * @param payload Output buffer (must be at least 4 bytes)
 * @return Size of payload, or 0 on error
 */
size_t build_stop_payload(uint8_t protocol_version, uint8_t *payload);

// ============================================================================
// Packet Parsing Functions
// ============================================================================

/**
 * Parse compact status packet
 * @param payload Packet payload (must be at least 9 bytes)
 * @param len Payload length
 * @return Parsed status, or invalid status if parsing fails
 */
CompactStatus parse_compact_status(const uint8_t *payload, size_t len);

/**
 * Parse extended status packet
 * @param payload Packet payload (must be at least 8 bytes)
 * @param len Payload length
 * @return Parsed status, or invalid status if parsing fails
 */
ExtendedStatus parse_extended_status(const uint8_t *payload, size_t len);

}  // namespace cosori_kettle_ble
}  // namespace esphome
