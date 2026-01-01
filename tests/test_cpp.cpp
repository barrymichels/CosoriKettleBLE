#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

// Include the C++ files we're testing
#include "components/cosori_kettle_ble/envelope.h"
#include "components/cosori_kettle_ble/protocol.h"

using namespace esphome::cosori_kettle_ble;

// Helper function to convert hex string to bytes
std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        while (hex[i] == ' ' || hex[i] == ':') {
          i++;
        }
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Helper function to print bytes as hex
std::string bytes_to_hex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; i++) {
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    // std::transform(oss.str().begin(), oss.str().end(), oss.str().begin(), ::toupper);
    // oss.uppercase
    return oss.str();
}

// Test envelope building and checksum calculation
void test_envelope_build() {
    std::cout << "Testing envelope building...\n";
    
    // Test 1: Build status request (A522 4104 0072 0140 4000)
    {
        uint8_t payload[] = {0x01, 0x40, 0x40, 0x00};
        Envelope env;
        bool result = env.set_message_payload(0x41, payload, 4);
        assert(result);
        assert(env.size() == 10);  // 6 header + 4 payload
        
        // Verify structure
        assert(env.data()[0] == 0xA5);  // magic
        assert(env.data()[1] == 0x22);  // message type
        assert(env.data()[2] == 0x41);  // seq
        assert(env.data()[3] == 0x04);  // len_lo
        assert(env.data()[4] == 0x00);  // len_hi
        assert(env.data()[5] == 0x72);  // checksum (from real packet)
        
        // Verify payload
        assert(memcmp(env.data() + 6, payload, 4) == 0);
        
        std::cout << "  ✓ Status request build\n";
    }
    
    // Test 2: Build V1 start coffee (A522 0309 0095 01F0 A300 0300 0000 00)
    {
        uint8_t payload[] = {0x01, 0xF0, 0xA3, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
        Envelope env;
        bool result = env.set_message_payload(0x03, payload, 9);
        assert(result);
        assert(env.size() == 15);  // 6 header + 9 payload
        
        assert(env.data()[0] == 0xA5);
        assert(env.data()[1] == 0x22);
        assert(env.data()[2] == 0x03);
        assert(env.data()[3] == 0x09);  // len_lo
        assert(env.data()[4] == 0x00);  // len_hi
        assert(env.data()[5] == 0x95);  // checksum
        
        std::cout << "  ✓ V1 start coffee build\n";
    }
    
    // Test 3: Build V1 stop (A522 0404 0098 01F4 A300)
    {
        uint8_t payload[] = {0x01, 0xF4, 0xA3, 0x00};
        Envelope env;
        bool result = env.set_message_payload(0x04, payload, 4);
        assert(result);
        assert(env.size() == 10);
        assert(env.data()[5] == 0x98);  // checksum
        
        std::cout << "  ✓ V1 stop build\n";
    }
    
    // Test 4: Build set mytemp 179 (A522 1C05 00CD 01F3 A300 B3)
    {
        uint8_t payload[] = {0x01, 0xF3, 0xA3, 0x00, 0xB3};
        Envelope env;
        bool result = env.set_message_payload(0x1C, payload, 5);
        assert(result);
        assert(env.size() == 11);
        assert(env.data()[5] == 0xCD);  // checksum
        
        std::cout << "  ✓ Set mytemp build\n";
    }
    
    std::cout << "All envelope build tests passed!\n\n";
}

// Test envelope parsing from real packets
void test_envelope_parse() {
    std::cout << "Testing envelope parsing...\n";
    
    // Test 1: Parse status request
    {
        auto packet = hex_to_bytes("A5224104007201404000");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.frame_type == 0x22);
        assert(frame.seq == 0x41);
        assert(frame.payload_len == 4);
        assert(memcmp(frame.payload, packet.data() + 6, 4) == 0);
        
        std::cout << "  ✓ Parse status request\n";
    }
    
    // Test 2: Parse compact status (A522 B50C 00B3 0141 4000 0000 B38F 0000 0000)
    {
        auto packet = hex_to_bytes("A522B50C00B3014140000000B38F00000000");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.frame_type == 0x22);
        assert(frame.seq == 0xB5);
        assert(frame.payload_len == 12);
        
        std::cout << "  ✓ Parse compact status\n";
    }
    
    // Test 3: Parse extended status ACK (A512 401D 0093 0140 4000 0000 AF69 AF00 0000 0000 0100 00C4 0E00 0000 0000 3408 0000 01)
    {
        auto packet = hex_to_bytes("A512401D0093014040000000AF69AF0000000000010000C40E00000000003408000001");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.frame_type == 0x12);  // ACK
        assert(frame.seq == 0x40);
        assert(frame.payload_len == 29);
        
        std::cout << "  ✓ Parse extended status ACK\n";
    }
    
    // Test 4: Parse completion notification (A522 9805 00E0 01F7 A300 20)
    {
        auto packet = hex_to_bytes("A522980500E001F7A30020");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.frame_type == 0x22);
        assert(frame.seq == 0x98);
        assert(frame.payload_len == 5);
        
        std::cout << "  ✓ Parse completion notification\n";
    }
    
    // Test 5: Parse multiple frames in sequence
    {
        auto packet1 = hex_to_bytes("A5221F0C0073014140000000AF6900000000");
        auto packet2 = hex_to_bytes("A522200C008A014140000000AF5100000000");
        auto packet3 = hex_to_bytes("A522210C0088014140000000AF5100010000");
        
        Envelope env;
        env.append(packet1.data(), packet1.size());
        env.append(packet2.data(), packet2.size());
        env.append(packet3.data(), packet3.size());
        
        Envelope::FrameInfo frame1 = env.process_next_frame(512);
        assert(frame1.valid && frame1.seq == 0x1F);
        
        Envelope::FrameInfo frame2 = env.process_next_frame(512);
        assert(frame2.valid && frame2.seq == 0x20);
        
        Envelope::FrameInfo frame3 = env.process_next_frame(512);
        assert(frame3.valid && frame3.seq == 0x21);
        
        std::cout << "  ✓ Parse multiple frames\n";
    }
    
    // Test 6: Invalid packet (wrong magic)
    {
        auto packet = hex_to_bytes("FF224104007201404000");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(!frame.valid);
        
        std::cout << "  ✓ Reject invalid magic\n";
    }
    
    // Test 7: Incomplete packet
    {
        auto packet = hex_to_bytes("A5224104");  // Incomplete
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(!frame.valid);  // Should not be valid yet
        
        std::cout << "  ✓ Handle incomplete packet\n";
    }
    
    std::cout << "All envelope parse tests passed!\n\n";
}

void assertMessagePayload(const std::string& expected_payload, const uint8_t *payload, size_t len) {
  auto expectedBytes = hex_to_bytes(expected_payload);
  
  uint8_t seq = expectedBytes[2];
  Envelope env;
  bool result = env.set_message_payload(seq, payload, len);
  assert(result);
  
  // std::cout << "expected_payload: " << expected_payload << std::endl;
  // std::cout << "payload: " << bytes_to_hex(env.data(), env.remaining()) << std::endl;

  // std::cout << "expectedBytes.size(): " << expectedBytes.size() << std::endl;
  // std::cout << "env.remaining(): " << env.remaining() << std::endl;

  assert(env.remaining() == expectedBytes.size());
  for (size_t i = 0; i < env.remaining(); i++) {
      assert(env.data()[i] == expectedBytes[i]);
  }
}

// Test protocol payload building
void test_protocol_build() {
    std::cout << "Testing protocol payload building...\n";
    
    uint8_t payload[64];
    
    // Test 1: Build status request payload
    {
        size_t len = build_status_request_payload(PROTOCOL_VERSION_V1, payload);
        assertMessagePayload("A5224104007201404000", payload, len);
        std::cout << "  ✓ Build status request payload\n";
    }
    
    // Test 2: Build compact status request payload
    {
        size_t len = build_compact_status_request_payload(PROTOCOL_VERSION_V1, payload);
        assertMessagePayload("A522B50400FD01414000", payload, len);
        std::cout << "  ✓ Build compact status request payload\n";
    }
    
    // Test 3: Build set mytemp payload (179°F = 0xB3)
    {
        size_t len = build_set_my_temp_payload(PROTOCOL_VERSION_V1, 179, payload);
        assertMessagePayload("A522 1C05 00CD 01F3 A300 B3", payload, len);
        
        std::cout << "  ✓ Build set mytemp payload\n";
    }
    
    // Test 4: Build set baby formula payload (enabled)
    {
        size_t len = build_set_baby_formula_payload(PROTOCOL_VERSION_V1, true, payload);
        assertMessagePayload("A522 2505 0074 01F5 A300 01", payload, len);
        
        std::cout << "  ✓ Build set baby formula payload (enabled)\n";
    }
    
    // Test 5: Build set baby formula payload (disabled)
    {
        size_t len = build_set_baby_formula_payload(PROTOCOL_VERSION_V1, false, payload);
        assertMessagePayload("A522 1D05 007D 01F5 A300 00", payload, len);
        
        std::cout << "  ✓ Build set baby formula payload (disabled)\n";
    }
    
    // Test 6: Build stop payload
    {
        size_t len = build_stop_payload(PROTOCOL_VERSION_V1, payload);
        assertMessagePayload("A5220404009801F4A300", payload, len);
        
        std::cout << "  ✓ Build stop payload\n";
    }
    
    // Test 7: Build set mode payload (coffee mode, no hold)
    {
        size_t len = build_set_mode_payload(PROTOCOL_VERSION_V1, 0x03, 0x00, 0, payload);
        assertMessagePayload("A522 4809 0050 01F0 A300 0300 0000 00", payload, len);
        
        std::cout << "  ✓ Build set mode payload (no hold)\n";
    }
    
    // Test 8: Build set mode payload (coffee mode, 35 min hold = 2100 seconds = 0x0834)
    {
        size_t len = build_set_mode_payload(PROTOCOL_VERSION_V1, 0x03, 0xCD, 2100, payload);
        assertMessagePayload("A522 1C090072 01F0A300 03CD 0108 34", payload, len);
        std::cout << "  ✓ Build set mode payload (with hold)\n";
    }
    
    // Test 9: Build set hold time payload (35 min = 2100 seconds)
    {
        size_t len = build_set_hold_time_payload(PROTOCOL_VERSION_V1, 2100, payload);
        assertMessagePayload("A522 49080014 01F2A300 0001 3408", payload, len);
        std::cout << "  ✓ Build set hold time payload\n";
    }
    
    // Test 10: Build register payload (pairing mode - 0x80)
    {
        std::array<uint8_t, 16> reg_key = {
            0x99, 0x03, 0xe0, 0x1a, 0x3c, 0x3b, 0xaa, 0x8f,
            0x6c, 0x71, 0xcb, 0xb5, 0x16, 0x7e, 0x7d, 0x5f
        };
        
        size_t len = build_register_payload(PROTOCOL_VERSION_V1, reg_key, payload);
        assert(len == 36);  // 4-byte header + 32-byte hex key
        
        // Verify header
        assert(payload[0] == PROTOCOL_VERSION_V1);  // 0x01
        assert(payload[1] == 0x80);  // CMD_REGISTER
        assert(payload[2] == 0xD1);  // CMD_TYPE_D1
        assert(payload[3] == 0x00);  // padding
        
        // Verify hex encoding of key (32 bytes ASCII hex)
        // 9903e01a3c3baa8f6c71cbb5167e7d5f
        const char* expected_hex = "9903e01a3c3baa8f6c71cbb5167e7d5f";
        for (size_t i = 0; i < 32; i++) {
            assert(payload[4 + i] == expected_hex[i]);
        }
        
        std::cout << "  ✓ Build register payload (0x80)\n";
    }
    
    // Test 11: Build hello payload (reconnect - 0x81)
    {
        // Test with the same registration key
        std::array<uint8_t, 16> reg_key = {
            0x99, 0x03, 0xe0, 0x1a, 0x3c, 0x3b, 0xaa, 0x8f,
            0x6c, 0x71, 0xcb, 0xb5, 0x16, 0x7e, 0x7d, 0x5f
        };
        
        size_t len = build_hello_payload(PROTOCOL_VERSION_V1, reg_key, payload);
        assert(len == 36);  // 4-byte header + 32-byte hex key
        
        // Verify header
        assert(payload[0] == PROTOCOL_VERSION_V1);  // 0x01
        assert(payload[1] == 0x81);  // CMD_HELLO
        assert(payload[2] == 0xD1);  // CMD_TYPE_D1
        assert(payload[3] == 0x00);  // padding
        
        // Verify hex encoding of key (32 bytes ASCII hex)
        const char* expected_hex = "9903e01a3c3baa8f6c71cbb5167e7d5f";
        for (size_t i = 0; i < 32; i++) {
            assert(payload[4 + i] == expected_hex[i]);
        }
        
        std::cout << "  ✓ Build hello payload (0x81)\n";
    }
    
    // Test 12: Register vs Hello command ID difference
    {
        std::array<uint8_t, 16> reg_key = {
            0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
        };
        
        uint8_t register_payload[36];
        uint8_t hello_payload[36];
        
        size_t reg_len = build_register_payload(PROTOCOL_VERSION_V1, reg_key, register_payload);
        size_t hello_len = build_hello_payload(PROTOCOL_VERSION_V1, reg_key, hello_payload);
        
        assert(reg_len == 36);
        assert(hello_len == 36);
        
        // Both should have same structure except command ID
        assert(register_payload[0] == hello_payload[0]);  // version
        assert(register_payload[1] == 0x80);  // REGISTER
        assert(hello_payload[1] == 0x81);   // HELLO
        assert(register_payload[2] == hello_payload[2]);  // type
        assert(register_payload[3] == hello_payload[3]);  // padding
        
        // Hex encoding should be identical
        for (size_t i = 4; i < 36; i++) {
            assert(register_payload[i] == hello_payload[i]);
        }
        
        std::cout << "  ✓ Register (0x80) vs Hello (0x81) command ID difference\n";
    }
    
    // Test 13: Test with null payload pointer (error handling)
    {
        std::array<uint8_t, 16> reg_key = {
            0x99, 0x03, 0xe0, 0x1a, 0x3c, 0x3b, 0xaa, 0x8f,
            0x6c, 0x71, 0xcb, 0xb5, 0x16, 0x7e, 0x7d, 0x5f
        };
        
        size_t len = build_register_payload(PROTOCOL_VERSION_V1, reg_key, nullptr);
        assert(len == 0);  // Should return 0 on error
        
        len = build_hello_payload(PROTOCOL_VERSION_V1, reg_key, nullptr);
        assert(len == 0);  // Should return 0 on error
        
        std::cout << "  ✓ Null payload pointer error handling\n";
    }
    
    // Test 14: Test hex encoding with all byte values
    {
        // Test key with all possible nibble values to verify hex encoding
        std::array<uint8_t, 16> reg_key = {
            0x00, 0x01, 0x0F, 0x10, 0xFF, 0xFE, 0xAB, 0xCD,
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xEF
        };
        
        size_t len = build_register_payload(PROTOCOL_VERSION_V1, reg_key, payload);
        assert(len == 36);
        
        // Verify hex encoding: each byte becomes 2 hex chars
        // 0x00->"00", 0x01->"01", 0x0F->"0f", 0x10->"10", 
        // 0xFF->"ff", 0xFE->"fe", 0xAB->"ab", 0xCD->"cd",
        // 0x12->"12", 0x34->"34", 0x56->"56", 0x78->"78",
        // 0x9A->"9a", 0xBC->"bc", 0xDE->"de", 0xEF->"ef"
        const char* expected_hex = "00010f10fffeabcd123456789abcdeef";
        for (size_t i = 0; i < 32; i++) {
            assert(payload[4 + i] == static_cast<uint8_t>(expected_hex[i]));
        }
        
        std::cout << "  ✓ Hex encoding with all byte values\n";
    }
    
    // Test 15: Test with V0 protocol version
    {
        std::array<uint8_t, 16> reg_key = {
            0x99, 0x03, 0xe0, 0x1a, 0x3c, 0x3b, 0xaa, 0x8f,
            0x6c, 0x71, 0xcb, 0xb5, 0x16, 0x7e, 0x7d, 0x5f
        };
        
        size_t len = build_register_payload(PROTOCOL_VERSION_V0, reg_key, payload);
        assert(len == 36);
        assert(payload[0] == PROTOCOL_VERSION_V0);  // 0x00
        
        len = build_hello_payload(PROTOCOL_VERSION_V0, reg_key, payload);
        assert(len == 36);
        assert(payload[0] == PROTOCOL_VERSION_V0);  // 0x00
        
        std::cout << "  ✓ Protocol version V0 support\n";
    }
    
    std::cout << "All protocol build tests passed!\n\n";
}

std::vector<uint8_t> parseEnvelope(const std::string& message) {
    auto packet = hex_to_bytes(message);
    Envelope env;
    env.append(packet.data(), packet.size());
    
    Envelope::FrameInfo frame = env.process_next_frame(512);
    assert(frame.valid);

    std::vector<uint8_t> payload(frame.payload, frame.payload + frame.payload_len);
    return payload;
}

// Test protocol parsing
void test_protocol_parse() {
    std::cout << "Testing protocol parsing...\n";
    
    {
      auto frame = parseEnvelope("a5:22:0b:0c:00:4b 01:41:40:00 01:04:d4:7b:00:00:00:00");
      auto ex_frame = parseEnvelope("a5:12:18:1d:00:a2:01:40:40:00:01:04:d4:7b:8c:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:08:07:00:00:01");
      CompactStatus status = parse_compact_status(frame.data(), frame.size());
      ExtendedStatus ex_status = parse_extended_status(ex_frame.data(), ex_frame.size());
      
      // std::cout << "status.valid: " << (status.valid ? "true" : "false") << std::endl;
      // std::cout << "ex_status.valid: " << (ex_status.valid ? "true" : "false") << std::endl;

      // std::cout << "status.stage: " << static_cast<int>(status.stage) << std::endl;
      // std::cout << "ex_status.stage: " << static_cast<int>(ex_status.stage) << std::endl;

      // std::cout << "status.mode: " << static_cast<int>(status.mode) << std::endl;
      // std::cout << "ex_status.mode: " << static_cast<int>(ex_status.mode) << std::endl;
      
      // std::cout << "status.setpoint: " << static_cast<int>(status.setpoint) << std::endl;
      // std::cout << "ex_status.setpoint: " << static_cast<int>(ex_status.setpoint) << std::endl;
      
      // std::cout << "status.temp: " << static_cast<int>(status.temp) << std::endl;
      // std::cout << "ex_status.temp: " << static_cast<int>(ex_status.temp) << std::endl;
      
      assert(status.valid);
      assert(ex_status.valid);

      assert(status.stage == 0x01);
      assert(ex_status.stage == 0x01);

      assert(status.mode == 0x04);
      assert(ex_status.mode == 0x04);
      
      assert(status.setpoint == 212);
      assert(ex_status.setpoint == 212);

      assert(status.temp == 123);
      assert(ex_status.temp == 123);

      std::cout << "  ✓ Parse compact status\n";
    }
    
    // Parse compact status (A522 B50C 00B3 0141 4000 0000 B38F 0000 0000)
    {
        auto frame = parseEnvelope("A522 B50C00B3 01414000 00 00 B3 8F 00 00 00 00");
        CompactStatus status = parse_compact_status(frame.data(), frame.size());
        assert(status.valid);
        assert(status.stage == 0x00);  // IDLE
        assert(status.mode == 0x00);   // IDLE
        assert(status.setpoint == 0xB3);  // 179°F
        assert(status.temp == 0x8F);   // 143°F
        
        std::cout << "  ✓ Parse compact status\n";
    }
    
    // Parse another compact status (A522 1F0C 0073 0141 4000 0000 AF69 0000 0000)
    {
        auto frame = parseEnvelope("A522 1F0C0073 01414000 0000 AF69 0000 0000");
        CompactStatus status = parse_compact_status(frame.data(), frame.size());

        // std::cout << "status.valid: " << (status.valid ? "true" : "false") << std::endl;
        // std::cout << "status.stage: " << static_cast<int>(status.stage) << std::endl;
        // std::cout << "status.mode: " << static_cast<int>(status.mode) << std::endl;
        // std::cout << "status.setpoint: " << static_cast<int>(status.setpoint) << std::endl;
        // std::cout << "status.temp: " << static_cast<int>(status.temp) << std::endl;
        
        assert(status.valid);
        assert(status.setpoint == 0xAF);  // 175°F
        assert(status.temp == 0x69);      // 105°F
        
        std::cout << "  ✓ Parse compact status (175°F setpoint)\n";
    }
    
    // Parse compact status with heating (A522 1D0C 0068 0141 4000 0101 B46F 0000 0000)
    {
        auto frame = parseEnvelope("A5221D0C0068014140000101B46F00000000");
        CompactStatus status = parse_compact_status(frame.data(), frame.size());
        assert(status.valid);
        assert(status.stage == 0x01);  // HEATING
        assert(status.mode == 0x01);   // GREEN_TEA (180°F)
        assert(status.setpoint == 0xB4);  // 180°F
        assert(status.temp == 0x6F);   // 111°F
        
        std::cout << "  ✓ Parse compact status (heating)\n";
    }

    {
        auto frame = parseEnvelope("A512 831D 00B6 0140 4000 0301 B4B5 AF01 2C01 9F00 0000 0058 0200 0000 0000 2C01 0000 01");
        ExtendedStatus status = parse_extended_status(frame.data(), frame.size());
        assert(status.valid);
        assert(status.stage == 0x03);
        assert(status.mode == 0x01);
        
        assert(status.configured_hold_time == 0x012C);
        assert(status.remaining_hold_time == 0x009F);
        
        std::cout << "  ✓ Parse extended status ACK (hold time)\n";
    }

    {
        auto frame = parseEnvelope("A512 8B1D 0014 0140 4000 0000 68B2 6800 0000 0000 0000 0058 0200 0000 0000 2C01 0100 01");
        ExtendedStatus status = parse_extended_status(frame.data(), frame.size());
        assert(status.valid);
        assert(status.stage == 0x00);
        assert(status.mode == 0x00);

        assert(status.my_temp == 0x68);   // 104°F
        assert(status.on_base == true);  // 0x00 = on base
        assert(status.configured_hold_time == 0x0000);
        assert(status.baby_formula_enabled == true);
        
        std::cout << "  ✓ Parse extended status ACK (off base)\n";
    }
    
    
    // Parse extended status ACK (A512 401D 0093 0140 4000 0000 AF69 AF00 0000 0000 0100 00C4 0E00 0000 0000 3408 0000 01)
    {
        auto frame = parseEnvelope("A512401D0093014040000000AF69AF0000000000010000C40E00000000003408000001");
        ExtendedStatus status = parse_extended_status(frame.data(), frame.size());
        assert(status.valid);
        assert(status.stage == 0x00);
        assert(status.setpoint == 0xAF);  // 175°F
        assert(status.temp == 0x69);      // 105°F
        assert(status.my_temp == 0xAF);   // 175°F
        assert(status.on_base == false);  // 0x01 = off base
        assert(status.configured_hold_time == 0x0000);
        assert(status.remaining_hold_time == 0x0000);
        assert(status.baby_formula_enabled == false);
        
        std::cout << "  ✓ Parse extended status ACK (off base)\n";
    }
    
    // Parse extended status ACK (on base) - A512 871D 0016 0140 4000 0000 68B5 6800 0000 0000 0000 0058 0200 0000 0000 2C01 0000 01
    {
        auto frame = parseEnvelope("A512871D001601404000000068B5680000000000000000580200000000002C01000001");
        ExtendedStatus status = parse_extended_status(frame.data(), frame.size());
        assert(status.valid);
        assert(status.setpoint == 0x68);  // 104°F
        assert(status.temp == 0xB5);      // 181°F
        assert(status.my_temp == 0x68);   // 104°F
        assert(status.on_base == true);   // 0x00 = on base
        assert(status.configured_hold_time == 0x0000);
        assert(status.remaining_hold_time == 0x0000);
        assert(status.baby_formula_enabled == false);
        
        std::cout << "  ✓ Parse extended status ACK (on base)\n";
    }
    
    // Invalid compact status (too short)
    {
        uint8_t payload[] = {0x01, 0x41, 0x40, 0x00, 0x00};  // Only 5 bytes
        CompactStatus status = parse_compact_status(payload, 5);
        assert(!status.valid);
        
        std::cout << "  ✓ Reject invalid compact status (too short)\n";
    }
    
    // Invalid extended status (wrong command)
    {
        uint8_t payload[] = {0x01, 0x99, 0x40, 0x00};  // Wrong command
        ExtendedStatus status = parse_extended_status(payload, 4);
        assert(!status.valid);
        
        std::cout << "  ✓ Reject invalid extended status (wrong command)\n";
    }
    
    std::cout << "All protocol parse tests passed!\n\n";
}

// Test round-trip: build payload, wrap in envelope, parse back
void test_round_trip() {
    std::cout << "Testing round-trip (build -> envelope -> parse)...\n";
    
    // Test 1: Status request round-trip
    {
        uint8_t payload[64];
        size_t payload_len = build_status_request_payload(PROTOCOL_VERSION_V1, payload);
        
        Envelope env;
        bool result = env.set_message_payload(0x42, payload, payload_len);
        assert(result);
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x42);
        assert(frame.payload_len == payload_len);
        assert(memcmp(frame.payload, payload, payload_len) == 0);
        
        std::cout << "  ✓ Status request round-trip\n";
    }
    
    // Test 2: Set mytemp round-trip
    {
        uint8_t payload[64];
        size_t payload_len = build_set_my_temp_payload(PROTOCOL_VERSION_V1, 179, payload);
        
        Envelope env;
        bool result = env.set_message_payload(0x1C, payload, payload_len);
        assert(result);
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x1C);
        assert(frame.payload_len == payload_len);
        assert(frame.payload[4] == 0xB3);  // 179°F
        
        std::cout << "  ✓ Set mytemp round-trip\n";
    }
    
    // Test 3: Compact status round-trip (build envelope from real packet, parse)
    {
        auto packet = hex_to_bytes("A522B50C00B3014140000000B38F00000000");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        
        CompactStatus status = parse_compact_status(frame.payload, frame.payload_len);
        assert(status.valid);
        assert(status.setpoint == 0xB3);
        assert(status.temp == 0x8F);
        
        std::cout << "  ✓ Compact status round-trip\n";
    }
    
    std::cout << "All round-trip tests passed!\n\n";
}

// Test against specific real packets from stuff.md
void test_real_packets() {
    std::cout << "Testing against real device packets from stuff.md...\n";
    
    // Test 1: Start 205, no hold: A522 xxxx xxxx 01F0 A300 0300 0000 00
    // Using seq 0x48 from test: A522 4809 0050 01F0 A300 0300 0000 00
    {
        auto packet = hex_to_bytes("A5224809005001F0A3000300000000");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x48);
        assert(frame.payload_len == 9);
        assert(frame.payload[0] == 0x01);
        assert(frame.payload[1] == 0xF0);
        assert(frame.payload[2] == 0xA3);
        assert(frame.payload[3] == 0x00);
        assert(frame.payload[4] == 0x03);  // mode = coffee
        assert(frame.payload[5] == 0x00);
        assert(frame.payload[6] == 0x00);  // hold disabled
        assert(frame.payload[7] == 0x00);  // hold time
        assert(frame.payload[8] == 0x00);
        
        std::cout << "  ✓ Start coffee, no hold\n";
    }
    
    // Test 2: Start 205, hold 35m: A522 xxxx xxxx 01F0 A300 0300 0134 08 (35 min = 2100s = 0x0834)
    // Build the packet correctly using envelope builder
    {
        uint8_t payload[] = {0x01, 0xF0, 0xA3, 0x00, 0x03, 0x00, 0x01, 0x08, 0x34};
        Envelope env;
        bool result = env.set_message_payload(0x48, payload, 9);
        assert(result);
        
        // Verify the structure
        assert(env.data()[0] == 0xA5);
        assert(env.data()[1] == 0x22);
        assert(env.data()[2] == 0x48);
        assert(env.data()[3] == 0x09);  // payload len
        assert(env.data()[4] == 0x00);
        
        // Verify payload
        assert(env.data()[6] == 0x01);  // version
        assert(env.data()[7] == 0xF0);  // CMD_SET_MODE
        assert(env.data()[8] == 0xA3);
        assert(env.data()[9] == 0x00);
        assert(env.data()[10] == 0x03);  // mode = coffee
        assert(env.data()[11] == 0x00);
        assert(env.data()[12] == 0x01);  // hold enabled
        assert(env.data()[13] == 0x08);  // hold time high (big-endian)
        assert(env.data()[14] == 0x34);  // hold time low (2100 seconds)
        
        std::cout << "  ✓ Start coffee, hold 35m\n";
    }
    
    // Test 3: Stop: A522 xxxx xxxx 01F4 A300
    {
        auto packet = hex_to_bytes("A5220404009801F4A300");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x04);
        assert(frame.payload_len == 4);
        assert(frame.payload[1] == 0xF4);  // STOP command
        
        std::cout << "  ✓ Stop command\n";
    }
    
    // Test 4: Set mytemp to 179: A522 1C05 00CD 01F3 A300 B3
    {
        auto packet = hex_to_bytes("A5221C0500CD01F3A300B3");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x1C);
        assert(frame.payload_len == 5);
        assert(frame.payload[1] == 0xF3);  // SET_MY_TEMP
        assert(frame.payload[4] == 0xB3);  // 179°F
        
        std::cout << "  ✓ Set mytemp to 179°F\n";
    }
    
    // Test 5: Set baby formula mode: A522 2505 0074 01F5 A300 01
    {
        auto packet = hex_to_bytes("A5222505007401F5A30001");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x25);
        assert(frame.payload[1] == 0xF5);  // SET_BABY_FORMULA
        assert(frame.payload[4] == 0x01);  // enabled
        
        std::cout << "  ✓ Set baby formula mode ON\n";
    }
    
    // Test 6: Done notification: A522 9805 00E0 01F7 A300 20
    {
        auto packet = hex_to_bytes("A522980500E001F7A30020");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x98);
        assert(frame.payload[1] == 0xF7);  // Completion notification
        assert(frame.payload[4] == 0x20);  // DONE status
        
        std::cout << "  ✓ Completion notification (done)\n";
    }
    
    // Test 7: Hold timer end: A522 E105 0096 01F7 A300 21
    {
        auto packet = hex_to_bytes("A522E105009601F7A30021");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.payload[4] == 0x21);  // HOLD_COMPLETE status
        
        std::cout << "  ✓ Completion notification (hold complete)\n";
    }
    
    // Test 8: Delay start: A522 290B 0099 01F1 A300 C40E 0400 0000 00
    // 1h 3min = 3780 seconds = 0x0EC4 (big-endian)
    {
        auto packet = hex_to_bytes("A522290B009901F1A300C40E0400000000");
        Envelope env;
        env.append(packet.data(), packet.size());
        
        Envelope::FrameInfo frame = env.process_next_frame(512);
        assert(frame.valid);
        assert(frame.seq == 0x29);
        assert(frame.payload_len == 11);
        assert(frame.payload[1] == 0xF1);  // Delay start command
        // Delay: C40E = 0x0EC4 = 3780 seconds = 1h 3min
        assert(frame.payload[4] == 0xC4);
        assert(frame.payload[5] == 0x0E);
        assert(frame.payload[6] == 0x04);  // mode = boil
        assert(frame.payload[7] == 0x00);
        
        std::cout << "  ✓ Delay start (1h 3min, boil)\n";
    }
    
    std::cout << "All real packet tests passed!\n\n";
}

int main() {
    std::cout << "=== C++ Protocol Tester ===\n\n";
    
    try {
        test_envelope_build();
        test_envelope_parse();
        test_protocol_build();
        test_protocol_parse();
        test_round_trip();
        test_real_packets();
        
        std::cout << "=== All tests passed! ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception\n";
        return 1;
    }
}
