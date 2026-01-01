#include "protocol.h"
#include <cstring>

namespace esphome {
namespace cosori_kettle_ble {

// ============================================================================
// Packet Generation Functions
// ============================================================================

size_t build_register_payload(uint8_t protocol_version, 
                               const std::array<uint8_t, 16> &registration_key,
                               uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_REGISTER;
  payload[2] = CMD_TYPE_D1;
  payload[3] = 0x00;
  
  // Convert 16-byte binary key to 32-byte ASCII hex
  static const char hex_chars[] = "0123456789abcdef";
  for (size_t i = 0; i < 16; i++) {
    uint8_t byte = registration_key[i];
    payload[4 + i * 2] = hex_chars[(byte >> 4) & 0x0F];
    payload[4 + i * 2 + 1] = hex_chars[byte & 0x0F];
  }
  
  return 36;  // 4-byte header + 32-byte hex key
}

size_t build_hello_payload(uint8_t protocol_version, 
                           const std::array<uint8_t, 16> &registration_key,
                           uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_HELLO;
  payload[2] = CMD_TYPE_D1;
  payload[3] = 0x00;
  
  // Convert 16-byte binary key to 32-byte ASCII hex
  static const char hex_chars[] = "0123456789abcdef";
  for (size_t i = 0; i < 16; i++) {
    uint8_t byte = registration_key[i];
    payload[4 + i * 2] = hex_chars[(byte >> 4) & 0x0F];
    payload[4 + i * 2 + 1] = hex_chars[byte & 0x0F];
  }
  
  return 36;  // 4-byte header + 32-byte hex key
}

size_t build_status_request_payload(uint8_t protocol_version, uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_POLL;
  payload[2] = CMD_TYPE_40;
  payload[3] = 0x00;
  
  return 4;
}

size_t build_compact_status_request_payload(uint8_t protocol_version, uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_CTRL;
  payload[2] = CMD_TYPE_40;
  payload[3] = 0x00;
  
  return 4;
}

size_t build_set_my_temp_payload(uint8_t protocol_version, uint8_t temp_f, uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  // Clamp to valid range
  if (temp_f < MIN_TEMP_F)
    temp_f = MIN_TEMP_F;
  if (temp_f > MAX_TEMP_F)
    temp_f = MAX_TEMP_F;

  payload[0] = protocol_version;
  payload[1] = CMD_SET_MY_TEMP;
  payload[2] = CMD_TYPE_A3;
  payload[3] = 0x00;
  payload[4] = temp_f;
  
  return 5;
}

size_t build_set_baby_formula_payload(uint8_t protocol_version, bool enabled, uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_SET_BABY_FORMULA;
  payload[2] = CMD_TYPE_A3;
  payload[3] = 0x00;
  payload[4] = static_cast<uint8_t>(enabled ? 0x01 : 0x00);
  
  return 5;
}

size_t build_set_hold_time_payload(uint8_t protocol_version, uint16_t seconds, uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_SET_HOLD_TIME;
  payload[2] = CMD_TYPE_A3;
  payload[3] = 0x00;
  payload[4] = 0x00;
  payload[5] = static_cast<uint8_t>((seconds > 0) ? 0x01 : 0x00);  // enable hold
  payload[6] = static_cast<uint8_t>(seconds & 0xFF);         // Low byte
  payload[7] = static_cast<uint8_t>((seconds >> 8) & 0xFF);  // High byte
  
  return 8;
}

size_t build_set_mode_payload(uint8_t protocol_version, uint8_t mode, uint8_t temp_f,
                              uint16_t hold_time_seconds, uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_SET_MODE;
  payload[2] = CMD_TYPE_A3;
  payload[3] = 0x00;
  payload[4] = mode;
  payload[5] = temp_f;
  payload[6] = static_cast<uint8_t>((hold_time_seconds > 0) ? 0x01 : 0x00);  // enable hold
  payload[7] = static_cast<uint8_t>((hold_time_seconds >> 8) & 0xFF);  // High byte
  payload[8] = static_cast<uint8_t>(hold_time_seconds & 0xFF);          // Low byte
  
  return 9;
}

size_t build_stop_payload(uint8_t protocol_version, uint8_t *payload) {
  if (payload == nullptr) {
    return 0;
  }

  payload[0] = protocol_version;
  payload[1] = CMD_STOP;
  payload[2] = CMD_TYPE_A3;
  payload[3] = 0x00;
  
  return 4;
}

// ============================================================================
// Packet Parsing Functions
// ============================================================================

CompactStatus parse_compact_status(const uint8_t *payload, size_t len) {
  CompactStatus status = {0, 0, 0, 0, 0};
  
  // Compact status: 01 41 40 00 <stage> <mode> <sp> <temp> <status> ...
  if (len < 9 || payload[1] != CMD_CTRL) {
    return status;
  }

  uint8_t temp = payload[7];      // Current temperature

  // Validate temperature range
  if (temp < MIN_VALID_READING_F || temp > MAX_VALID_READING_F) {
    return status;
  }

  status.temp = temp;
  status.mode = payload[5];      // Mode
  status.setpoint = payload[6];        // Setpoint temperature
  status.stage = payload[4];     // Heating stage
  status.valid = true;
  
  return status;
}

ExtendedStatus parse_extended_status(const uint8_t *payload, size_t len) {
  ExtendedStatus status = {0, 0, 0, 0, 0, 0, 0, 0, false, false};
  
  // Extended status: 01 40 40 00 <stage> <mode> <sp> <temp> ... <on_base> ...
  // NOTE: Extended packets (A512 = A5 + 12, len=29) contain on-base detection at payload[14] (byte 20)
  // Compact packets (A522 = A5 + 22, len=12) do NOT contain on-base information
  if (len < 29 || payload[1] != CMD_POLL) {
    return status;
  }

  uint8_t temp = payload[7];

  // Validate temperature range
  if (temp < MIN_VALID_READING_F || temp > MAX_VALID_READING_F) {
    return status;
  }

  status.stage = payload[4];
  status.mode = payload[5];
  status.setpoint = payload[6];
  status.temp = temp;
  status.valid = true;

  uint8_t mytemp = payload[8];
  if (mytemp >= MIN_TEMP_F && mytemp <= MAX_TEMP_F) {
    status.my_temp = mytemp;
  }

  status.on_base = (payload[14] == 0x00);  // 0x00=on-base, 0x01=off-base
  status.configured_hold_time = (static_cast<uint16_t>(payload[11]) << 8) | payload[10];
  status.remaining_hold_time = (static_cast<uint16_t>(payload[13]) << 8) | payload[12];
  status.baby_formula_enabled = (payload[26] == 0x01);
  
  return status;
}

}  // namespace cosori_kettle_ble
}  // namespace esphome
