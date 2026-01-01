#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

// Include the C++ files we're testing
#include "components/cosori_kettle_ble/cosori_kettle_state.h"
#include "components/cosori_kettle_ble/protocol.h"

using namespace esphome::cosori_kettle_ble;

// Helper function to convert hex string to bytes
std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        while (i < hex.length() && (hex[i] == ' ' || hex[i] == ':')) {
          i++;
        }
        if (i >= hex.length()) break;
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
    return oss.str();
}

// Test helpers
class TestContext {
public:
    std::vector<std::vector<uint8_t>> sent_packets;

    void clear() {
        sent_packets.clear();
    }
};

// Test basic state initialization
void test_state_initialization() {
    std::cout << "Testing state initialization...\n";

    CosoriKettleState::Config config;
    config.protocol_version = 1;
    config.registration_key_set = false;

    CosoriKettleState state(config);

    const auto& kettle_state = state.get_state();
    assert(kettle_state.current_temp_f == 0.0f);
    assert(kettle_state.target_setpoint_f == 212.0f);  // Default boil temp
    assert(kettle_state.heating == false);
    assert(kettle_state.on_base == false);
    assert(kettle_state.status_received == false);

    std::cout << "  ✓ State initialization\n";
}

// Test processing RX status packets
void test_rx_status_processing() {
    std::cout << "Testing RX status processing...\n";

    TestContext ctx;

    CosoriKettleState::Config config;
    config.protocol_version = 1;
    config.registration_key = {
        0x99, 0x03, 0xe0, 0x1a, 0x3c, 0x3b, 0xaa, 0x8f,
        0x6c, 0x71, 0xcb, 0xb5, 0x16, 0x7e, 0x7d, 0x5f
    };
    config.registration_key_set = true;

    CosoriKettleState state(config);

    // Set up callbacks
    state.set_send_data_callback([&ctx](const uint8_t* data, size_t len) {
        std::vector<uint8_t> packet(data, data + len);
        ctx.sent_packets.push_back(packet);
    });

    // Test 1: Process compact status packet (kettle idle at 143°F, setpoint 179°F)
    {
        auto packet = hex_to_bytes("A522 B50C00B3 01414000 00 00 B3 8F 00 00 00 00");
        state.process_rx_data(packet.data(), packet.size());

        const auto& kettle_state = state.get_state();
        assert(kettle_state.current_temp_f == 143);  // 0x8F
        assert(kettle_state.kettle_setpoint_f == 179);  // 0xB3
        assert(kettle_state.heating == false);  // stage = 0
        assert(kettle_state.status_received == true);

        std::cout << "  ✓ Process compact status (idle)\n";
    }

    // Test 2: Process compact status packet (kettle heating)
    {
        auto packet = hex_to_bytes("A522 1D0C 0068 0141 4000 0101 B46F 0000 0000");
        state.process_rx_data(packet.data(), packet.size());

        const auto& kettle_state = state.get_state();
        assert(kettle_state.current_temp_f == 111);  // 0x6F
        assert(kettle_state.kettle_setpoint_f == 180);  // 0xB4
        assert(kettle_state.heating == true);  // stage = 1

        std::cout << "  ✓ Process compact status (heating)\n";
    }

    // Test 3: Process extended status packet (on base, hold time)
    {
        auto packet = hex_to_bytes("A512 831D 00B6 0140 4000 0301 B4B5 AF01 2C01 9F00 0000 0058 0200 0000 0000 2C01 0000 01");
        state.process_rx_data(packet.data(), packet.size());

        const auto& kettle_state = state.get_state();
        assert(kettle_state.current_temp_f == 181);  // 0xB5
        assert(kettle_state.kettle_setpoint_f == 180);  // 0xB4
        assert(kettle_state.heating == true);  // stage = 3
        assert(kettle_state.on_base == true);  // 0x00
        assert(kettle_state.hold_time_seconds == 0x012C);  // 300 seconds
        assert(kettle_state.remaining_hold_time_seconds == 0x009F);  // 159 seconds

        std::cout << "  ✓ Process extended status (hold time)\n";
    }

    std::cout << "All RX status processing tests passed!\n\n";
}

// Test command generation and TX flow
void test_command_generation() {
    std::cout << "Testing command generation...\n";

    TestContext ctx;

    CosoriKettleState::Config config;
    config.protocol_version = 1;
    config.registration_key = {
        0x99, 0x03, 0xe0, 0x1a, 0x3c, 0x3b, 0xaa, 0x8f,
        0x6c, 0x71, 0xcb, 0xb5, 0x16, 0x7e, 0x7d, 0x5f
    };
    config.registration_key_set = true;

    CosoriKettleState state(config);

    // Set up callbacks
    state.set_send_data_callback([&ctx](const uint8_t* data, size_t len) {
        std::vector<uint8_t> packet(data, data + len);
        ctx.sent_packets.push_back(packet);
    });

    // Test 1: Send hello command
    {
        ctx.clear();
        state.send_hello(false);

        // Process state machine to send the command
        uint32_t now_ms = 0;
        for (int i = 0; i < 5; i++) {
            state.update(now_ms, true, true);
            if (!ctx.sent_packets.empty()) {
                state.on_write_ack(true);
            }
            now_ms += 100;
        }

        // Should send hello command (CMD_HELLO = 0x81)
        assert(ctx.sent_packets.size() >= 1);

        std::cout << "  ✓ Send hello command\n";
    }

    // Test 2: Set my temp
    {
        ctx.clear();
        state.set_my_temp(179);

        // Acknowledge write
        if (!ctx.sent_packets.empty()) {
            state.on_write_ack(true);
        }

        // Should send set my temp command
        assert(ctx.sent_packets.size() >= 1);

        std::cout << "  ✓ Send set my temp\n";
    }

    // Test 3: Set baby formula
    {
        ctx.clear();
        state.set_baby_formula_enabled(true);

        // Acknowledge write
        if (!ctx.sent_packets.empty()) {
            state.on_write_ack(true);
        }

        // Should send set baby formula command
        assert(ctx.sent_packets.size() >= 1);

        std::cout << "  ✓ Send set baby formula\n";
    }

    // Test 4: Set hold time
    {
        ctx.clear();
        state.set_hold_time(2100);  // 35 minutes

        // Acknowledge write
        if (!ctx.sent_packets.empty()) {
            state.on_write_ack(true);
        }

        // Should send set hold time command
        assert(ctx.sent_packets.size() >= 1);

        std::cout << "  ✓ Send set hold time\n";
    }

    std::cout << "All command generation tests passed!\n\n";
}

// Test state machine for heating sequence
void test_heating_sequence() {
    std::cout << "Testing heating sequence...\n";

    TestContext ctx;

    CosoriKettleState::Config config;
    config.protocol_version = 1;
    config.registration_key = {
        0x99, 0x03, 0xe0, 0x1a, 0x3c, 0x3b, 0xaa, 0x8f,
        0x6c, 0x71, 0xcb, 0xb5, 0x16, 0x7e, 0x7d, 0x5f
    };
    config.registration_key_set = true;

    CosoriKettleState state(config);

    // Set up callbacks
    state.set_send_data_callback([&ctx](const uint8_t* data, size_t len) {
        std::vector<uint8_t> packet(data, data + len);
        ctx.sent_packets.push_back(packet);
    });

    // Set target temperature to boil (212°F)
    state.set_target_setpoint(212.0f);

    // Start heating
    ctx.clear();
    state.start_heating();

    // Should transition to heating state
    assert(!state.is_idle());

    // Simulate time passing and call update
    uint32_t now_ms = 0;

    // Process state machine
    for (int i = 0; i < 10; i++) {
        state.update(now_ms, true, true);

        // Acknowledge any writes
        if (!ctx.sent_packets.empty()) {
            state.on_write_ack(true);
            ctx.sent_packets.clear();
        }

        now_ms += 100;
    }

    std::cout << "  ✓ Start heating sequence\n";

    // Test stop heating
    ctx.clear();
    state.stop_heating();

    assert(!state.is_idle());

    // Process state machine
    for (int i = 0; i < 10; i++) {
        state.update(now_ms, true, true);

        // Acknowledge any writes
        if (!ctx.sent_packets.empty()) {
            state.on_write_ack(true);
            ctx.sent_packets.clear();
        }

        now_ms += 100;
    }

    std::cout << "  ✓ Stop heating sequence\n";

    std::cout << "All heating sequence tests passed!\n\n";
}

// Test reset functionality
void test_reset() {
    std::cout << "Testing reset functionality...\n";

    CosoriKettleState::Config config;
    config.protocol_version = 1;
    config.registration_key_set = true;

    CosoriKettleState state(config);

    // Process some data
    auto packet = hex_to_bytes("A522 B50C00B3 01414000 00 00 B3 8F 00 00 00 00");
    state.process_rx_data(packet.data(), packet.size());

    const auto& kettle_state1 = state.get_state();
    assert(kettle_state1.status_received == true);

    // Reset
    state.reset();

    const auto& kettle_state2 = state.get_state();
    assert(kettle_state2.status_received == false);
    assert(kettle_state2.no_response_count == 0);

    std::cout << "  ✓ Reset clears state\n";
    std::cout << "All reset tests passed!\n\n";
}

int main() {
    std::cout << "=== CosoriKettleState Black-Box Tests ===\n\n";

    try {
        test_state_initialization();
        test_rx_status_processing();
        test_command_generation();
        test_heating_sequence();
        test_reset();

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
