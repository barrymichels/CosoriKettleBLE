# Cosori Smart Kettle BLE Protocol

This document describes the custom Bluetooth Low Energy protocol used by the Cosori Smart Electric Kettle for communication between the kettle and control devices (smartphone apps, ESP32, etc.).

## Overview

The Cosori kettle uses a proprietary BLE protocol for bidirectional communication. The protocol has evolved through two versions:

### V0 Protocol (Legacy)
- Basic temperature control using setpoint commands
- Requires hello5 handshake before setpoint
- Limited to boil and heat modes

### V1 Protocol (Current)
- Advanced features: delayed start, hold/keep-warm timers, custom temperature
- Registration/pairing support
- Baby formula mode
- Completion notifications
- No hello5 required

Both protocols support:
- Reading kettle status (temperature, heating state, on-base detection)
- Starting and stopping heating
- Real-time status updates via BLE notifications

## BLE Characteristics

The kettle exposes two primary GATT characteristics:

| Characteristic | UUID | Direction | Purpose |
|---|---|---|---|
| **TX** | `0xFFF2` | Write | Send commands to kettle |
| **RX** | `0xFFF1` | Notify | Receive status updates from kettle |

**Service UUID:** `0xFFF0`

### Flow Control & BLE Communication

**20-Byte Chunking for TX (Controller → Kettle):**
- BLE characteristic writes are limited to 20 bytes per write
- Packets larger than 20 bytes MUST be split into 20-byte chunks when sending
- Example: 35-byte status ACK packet = 20 bytes + 15 bytes (if sending to kettle)
- Chunks are sent sequentially without additional framing

**Packet Sizes:**
- Envelope: 6 bytes (fixed)
- Status request: 10 bytes total (6 envelope + 4 payload) - single write
- Compact status: 18 bytes total (6 + 12) - single write  
- Status ACK: 35 bytes total (6 + 29) - requires 2 writes if sending
- V1 commands: varies (9-15 bytes typically) - single write

**RX (Kettle → Controller) - Single Complete Messages:**
- Kettle sends complete messages as single notifications
- No multi-chunk reassembly needed for RX
- Status ACK (35 bytes) arrives as a single notification
- Parser receives complete packets, no buffering required

**Request/Response Pattern:**
1. Controller writes command to TX characteristic (0xFFF2)
   - Split into 20-byte chunks if needed
2. Kettle sends response via RX notification characteristic (0xFFF1)
   - Always complete messages (no chunking)
3. Response types:
   - Command ACK (frame type 0x12, 4-5 bytes payload)
   - Status ACK (frame type 0x12, 29 bytes payload) - response to status request
   - Compact status (frame type 0x22, 12 bytes payload) - unsolicited updates
   - Completion notification (frame type 0x22, 5 bytes payload)

**Example Flow - Set Custom Temperature:**
```
Controller → TX: Set mytemp to 179°F (A522 1C05 00CD 01F3 A300 B3)
Kettle → RX: Command ACK (A512 1C04 0091 01F3 A300) [single notification]
Kettle → RX: Compact status showing new setpoint [single notification]
```

**Example Flow - Status Request:**
```
Controller → TX: Status request (A522 4104 0072 0140 4000) [single write]
Kettle → RX: Status ACK (35 bytes) [single notification - complete packet]
```

**Implementation Notes:**
- **TX (sending):** Split packets >20 bytes into 20-byte chunks
- **RX (receiving):** Process complete packets as received
- Validate magic byte (0xA5) at start of each packet
- Validate checksum before processing

## Packet Structure

All packets follow this format:

```
[Header: 6 bytes] [Payload: variable length]
```

### Header Format

| Offset | Field | Description |
|---|---|---|
| 0 | Magic | Always `0xA5` |
| 1 | Type | Packet type identifier |
| 2 | Sequence | Packet sequence number (0x00-0xFF, wraps) |
| 3 | Length Low | Payload length low byte |
| 4 | Length High | Payload length high byte |
| 5 | Checksum | Simple checksum (see below) |

### Checksum Calculation

<!-- TODO: is this accurate for v0 packets? -->
The v0 checksum is calculated as:
```
checksum = (magic + type + seq + len_lo + len_hi) & 0xFF
```

The v1 checksum is calculated by setting the checksum byte to 0x01, set `checksum = 0` then for each byte, `checksum = (checksum - byte) & 0xFF`

## Protocol Versions

### Detecting Protocol Version

The protocol version can be detected from the device's hardware and software version strings:
- **V0**: Older firmware versions
- **V1**: Hardware 1.0.00, Software R0007V0012 and newer

### Command Headers

All commands use a 4-byte header:
```
[version] [command_id] [direction] [padding]
```

- **V0 commands**: Start with `0x00` (e.g., `00F0A300`, `00F2A300`, `00F4A300`)
- **V1 commands**: Start with `0x01` (e.g., `01F0A300`, `01F1A300`, `0181D100`)

## Packet Types

### Status Packets (FROM Kettle)

#### 1. Compact Status (`0x22`)

**Total Length:** 18 bytes (6 header + 12 payload)

**Format:**
```
A5 22 [seq] [len_lo] [len_hi] [checksum] [payload: 12 bytes]
```

**Payload Structure:**

| Offset | Field | Description | Values |
|---|---|---|---|
| 0 | Header1 | Always `0x01` | - |
| 1 | Header2 | Always `0x41` | - |
| 2 | Reserved | Unknown | `0x40` |
| 3 | Reserved | Unknown | `0x00` |
| 4 | Stage | Heating stage | `0x01`=heating, `0x00`=not heating |
| 5 | Mode | Operating mode | `0x00`=normal, `0x04`=keep warm |
| 6 | Setpoint | Target temperature (°F) | 104-212 |
| 7 | Temperature | Current water temperature (°F) | 40-230 |
| 8 | Status | Heating status | `0x00`=idle, non-zero=heating |
| 9-11 | Unknown | Additional data | - |

**Note:** Compact status packets do NOT contain on-base detection information.

**Example:**
```
a5:22:5e:04:00:2f:01:41:40:00:00:00:d4:64:8c:00:00:00
```

#### 2. Status ACK (`0x12`)

**Important:** This is an ACK frame (type 0x12) with status payload. It's sent in response to status requests.

**Total Length:** 35 bytes (6 header + 29 payload) - FIXED LENGTH

**Format:**
```
A5 12 [seq] [len_lo] [len_hi] [checksum] [payload: 29 bytes]
```

**Payload Structure (29 bytes fixed):**

| Offset | Field | Description | Values |
|---|---|---|---|
| 0-3 | Command | Always `01404000` | - |
| 4 | Stage | Heating stage | See HeatingStage enum |
| 5 | Mode | Operating mode | See OperatingMode enum |
| 6 | Setpoint | Target temperature (°F) | 104-212 |
| 7 | Temperature | Current water temperature (°F) | 40-230 |
| 8 | MyTemp | Custom temperature setting | 104-212 |
| 9-13 | Padding | Unknown/reserved | - |
| **14** | **On-Base** | **Kettle placement** | **`0x00`=on-base, `0x01`=off-base** |
| 15-16 | Hold Time | Remaining hold time (seconds, big-endian) | 0-65535 |
| 17-27 | Padding | Unknown/reserved | - |
| 28 | Baby Mode | Baby formula mode | `0x01`=enabled, `0x00`=disabled |

**Important:** The on-base detection byte is at **offset 14** of the payload.

**Example (on-base, not heating):**
```
a5:12:19:1d:00:10:01:40:40:00:00:00:d4:5c:8c:00:00:00:00:00:00:00:00:3c:69:00:00:00:00:01:10:0e:00:00:01
                                                           ^^
                                                      byte 20 = 0x00 (on-base)
```

**Example (off-base):**
```
a5:12:1c:1d:00:0c:01:40:40:00:00:00:d4:5c:8c:00:00:00:00:00:01:00:00:3c:69:00:00:00:00:01:10:0e:00:00:01
                                                           ^^
                                                      byte 20 = 0x01 (off-base)
```

### Command Packets (TO Kettle)

#### 1. Status Request (`0x22`)

Request a status update from the kettle. Device responds with Status ACK (frame type 0x12).

**Format:**
```
A5 22 [seq] 04 00 [checksum] 01 40 40 00
```

**Example:**
```
A5 22 41 04 00 72 01 40 40 00
```

This is typically sent periodically (every 1-2 seconds) to request status updates from the kettle.

#### 2. Start Heating (`0x20`)

Start heating to the specified target temperature.

**Format:**
```
A5 20 [seq] 0C 00 [checksum] 01 41 40 00 [stage] [mode] [temp] 00 00 00 00 00
```

**Parameters:**
- `stage`: Set to `0x01` to start heating
- `mode`: `0x00` for normal heating, `0x04` for keep-warm mode
- `temp`: Target temperature in °F (104-212)

**Example (heat to 212°F):**
```
a5:20:5f:0c:00:2e:01:41:40:00:01:00:d4:00:00:00:00:00
                                    ^^    ^^
                               stage=0x01  temp=212(0xD4)
```

#### 3. Stop Heating (`0x20`)

Stop the current heating operation.

**Format:**
```
A5 20 [seq] 0C 00 [checksum] 01 41 40 00 00 00 [temp] 00 00 00 00 00
```

**Parameters:**
- `stage`: Set to `0x00` to stop heating
- `mode`: Set to `0x00`
- `temp`: Current setpoint (doesn't change anything)

**Example:**
```
a5:20:60:0c:00:2f:01:41:40:00:00:00:d4:00:00:00:00:00
                                    ^^
                               stage=0x00 (stop)
```

## State Detection

### On-Base Detection

The kettle reports whether it is physically placed on the charging base.

**Location:** Extended status packet, byte 20 (payload[14])

**Values:**
- `0x00`: Kettle is on the charging base
- `0x01`: Kettle has been removed from the base

**Behavior:**
- When the kettle is removed from the base while heating, it immediately stops heating
- The on-base status updates in real-time via extended status packets
- Compact status packets do NOT contain on-base information

**Important:** Initial implementations incorrectly used payload[4] (heating stage) which only worked "by accident" when removing the kettle during heating. The correct byte is payload[14].

### Heating Detection

**Location:** Both packet types, payload[4] (stage) and payload[8] (status)

**Values:**
- `payload[4]`: `0x01` when heating, `0x00` when idle
- `payload[8]`: `0x00` when idle, non-zero when heating

Both fields correlate with the heating state, but they serve different purposes. For reliable heating detection, check `payload[4] != 0`.

### Temperature Monitoring

**Location:** Both packet types, payload[6] (setpoint) and payload[7] (current)

**Temperature Ranges:**
- **Setpoint (commanded):** 104-212°F (40-100°C)
  - This is the range you can set as a target temperature
- **Current (sensor validation):** 40-230°F
  - Readings outside this range indicate sensor errors or corrupted packets
- **Current (typical operation):** ~50-212°F
  - Cold tap water is typically 50-70°F
  - Maximum is boiling point at 212°F

**Units:** All temperature values are in Fahrenheit (°F).

**Validation:** If the current temperature reading is below 40°F or above 230°F, the packet should be discarded as invalid. In normal operation, you'll see temperatures from cold tap water (~50-70°F) up to boiling (212°F).

## Communication Flow

### Typical Session

1. **Connect** to BLE device
2. **Discover** service `0xFFF0` and characteristics `0xFFF1`, `0xFFF2`
3. **Subscribe** to notifications on RX characteristic (`0xFFF1`)
4. **Send poll** command to request initial status
5. **Receive** extended status packet with current state
6. **Monitor** notifications for status updates
7. **Send commands** via TX characteristic (`0xFFF2`) as needed

### Polling Strategy

The kettle does not automatically send status updates without prompting. Implement a polling loop:

```
Every 1-2 seconds:
  1. Send poll command (0x21)
  2. Wait for response
  3. Parse status packet
  4. Update internal state
```

If no response is received after 3-5 poll attempts, consider the kettle offline/disconnected.

## Example Scenarios

### Scenario 1: Start Heating to 212°F

**Send:**
```
TX: a5:20:5f:0c:00:2e:01:41:40:00:01:00:d4:00:00:00:00:00
```

**Receive (heating):**
```
RX: a5:12:21:1d:00:c7:01:40:40:00:01:04:d4:5c:8c:01:10:0e:10:0e:00:00:00:3c:69:00:00:00:00:01:10:0e:00:00:01
    Status: Heating, temp=92°F (0x5C), setpoint=212°F (0xD4), on-base
```

**Receive (target reached):**
```
RX: a5:12:25:1d:00:03:01:40:40:00:00:00:d4:d4:8c:00:00:00:00:00:00:00:00:3c:69:00:00:00:00:01:10:0e:00:00:01
    Status: Idle, temp=212°F (0xD4), setpoint=212°F (0xD4), on-base
```

### Scenario 2: Remove Kettle While Heating

**Before removal (heating):**
```
RX: a5:12:23:1d:00:c4:01:40:40:00:01:04:d4:5c:8c:01:10:0e:10:0e:00:00:00:3c:69:00:00:00:00:01:10:0e:00:00:01
                                                           ^^
                                                      byte 20 = 0x00 (on-base)
```

**After removal:**
```
RX: a5:12:25:1d:00:03:01:40:40:00:00:00:d4:5c:8c:00:00:00:00:00:01:00:00:3c:69:00:00:00:00:01:10:0e:00:00:01
                                      ^^                   ^^
                               stage=0x00 (stopped)   byte 20 = 0x01 (off-base)
```

The kettle immediately stops heating when removed from the base.

### Scenario 3: Stop Heating

**Send:**
```
TX: a5:20:60:0c:00:2f:01:41:40:00:00:00:d4:00:00:00:00:00
                                      ^^
                                 stage=0x00 (stop)
```

**Receive:**
```
RX: a5:12:26:1d:00:02:01:40:40:00:00:00:d4:5c:8c:00:00:00:00:00:00:00:00:3c:69:00:00:00:00:01:10:0e:00:00:01
    Status: Idle, temp=92°F, setpoint=212°F, on-base
```

## Implementation Notes

### Sequence Numbers

- Maintain separate sequence counters for TX and RX
- Increment after each packet sent/received
- Sequence numbers wrap from 0xFF to 0x00
- Can be used to detect missed packets or verify packet order

### Error Handling

**Invalid Temperature:** If `payload[7]` is outside 40-230°F, ignore the packet or retain previous temperature.

**Missing Packets:** If no status received after multiple polls, mark device as offline.

**Checksum Mismatch:** Discard packets with invalid checksums.

### Packet Timing

- Poll interval: 1-2 seconds is sufficient
- Response latency: Expect response within 200-500ms
- Notification rate: Kettle may send unsolicited notifications during heating (temperature changes)

### Connection Management

- BLE connection may drop if kettle is off-base for extended periods
- Implement reconnection logic with exponential backoff
- Maximum connection range: ~10 meters (typical BLE range)

## Discoveries and Corrections

During development of the ESPHome component, several incorrect assumptions were made and corrected:

### Incorrect: Using Compact Packet for On-Base Detection

**Initial Attempt:** Used `payload[4]` (stage byte) from compact packets for on-base detection.

**Problem:** This byte tracks **heating state**, not on-base state. It appeared to work when removing the kettle during heating (because removal stops heating), but failed when the kettle was idle.

**Correct:** On-base detection must use extended packet `payload[14]` (byte 20).

### Incorrect: Byte Position for On-Base Status

**Initial Attempt:** Used `payload[18]` based on incorrect packet analysis.

**Problem:** Wrong byte offset.

**Correct:** The on-base indicator is at byte 20 of the full packet, which is `payload[14]` after stripping the 6-byte header.

### Temperature Conversion Issues

**Initial Attempt:** Treated temperature values as Celsius and converted to Fahrenheit.

**Problem:** Values are already in Fahrenheit. Converting them resulted in readings like 414°F.

**Correct:** Temperature values in packets are already in Fahrenheit (°F). No conversion needed.

## Protocol Reverse Engineering

This protocol was reverse-engineered through:
1. BLE packet capture using smartphone app communication
2. Systematic testing of on-base/off-base scenarios
3. Heating and cooling cycle monitoring
4. Trial-and-error with byte field analysis
5. Validation against multiple real-world test scenarios

Special thanks to the implementation efforts that identified the correct byte positions through careful packet analysis.

## V1 Protocol Commands

### Registration Commands

#### Register (Pairing)
**Command:** `0180D100` + 32-byte registration key

Device must be in pairing mode. Generate a 16-byte random key and encode as ASCII hex (32 bytes).

**Example:**
```
A5 22 00 24 00 [cs] 01 80 D1 00 [32 bytes of ASCII hex key]
```

**Response:** ACK with payload `00` on success.

#### Hello (Reconnect)
**Command:** `0181D100` + 32-byte registration key

Use previously registered key to reconnect.

### Heating Commands

#### Start Heating
**Command:** `01F0A300` + mode (2B BE) + hold_enable (1B) + hold_time (2B BE)

**Modes:**
- `0100`: Green Tea (180°F)
- `0200`: Oolong (195°F)
- `0300`: Coffee (205°F)
- `0400`: Boil (212°F)
- `0500`: My Temp (custom)

**Examples:**
```
Start coffee, no hold:     A5 22 xx 09 00 [cs] 01 F0 A3 00 03 00 00 00 00
Start boil, hold 35 min:   A5 22 xx 09 00 [cs] 01 F0 A3 00 04 00 01 08 34
```

#### Delayed Start
**Command:** `01F1A300` + delay (2B BE) + mode (2B BE) + hold_enable (1B) + hold_time (2B BE)

**Example:**
```
Delay 1 hour, boil, no hold: A5 22 xx 0B 00 [cs] 01 F1 A3 00 0E 10 04 00 00 00 00
```

#### Stop Heating
**Command:** `01F4A300`

```
A5 22 xx 04 00 [cs] 01 F4 A3 00
```

### Configuration Commands

#### Set My Temp
**Command:** `01F3A300` + temperature (1B)

Set custom temperature for MY_TEMP mode (104-212°F).

**Example:**
```
Set 179°F: A5 22 1C 05 00 [cs] 01 F3 A3 00 B3
```

#### Set Baby Formula Mode
**Command:** `01F5A300` + enabled (1B)

Enable special baby formula temperature mode.

**Example:**
```
Enable:  A5 22 25 05 00 [cs] 01 F5 A3 00 01
Disable: A5 22 1D 05 00 [cs] 01 F5 A3 00 00
```

### Completion Notifications

**Command:** `01F7A300` + status (1B)

Sent by device when heating completes.

**Status values:**
- `0x20`: Heating complete (may enter hold mode)
- `0x21`: Hold timer complete

**Examples:**
```
Done:          A5 22 98 05 00 [cs] 01 F7 A3 00 20
Hold complete: A5 22 E1 05 00 [cs] 01 F7 A3 00 21
```

## Extended Status Fields

Extended status packets (frame type `0x12`) contain additional fields:

| Offset | Field | Description |
|---|---|---|
| 8 | mytemp_f | Custom temperature setting |
| 10-11 | hold_time_remaining | Seconds remaining in hold (big-endian) |
| 14 | on_base | `0x00`=on-base, `0x01`=off-base |
| 20 | error_code | Error indicator (0=no error) |
| 22 | baby_mode | `0x01`=enabled, `0x00`=disabled |

## Error States

Error states are indicated by:
- Suspicious temperature values (e.g., `0xB004` = 45060°F)
- Non-zero error_code in extended status
- Duplicate error values in multiple temperature fields

## Future Research

Areas not yet fully understood:

- **Error code meanings:** Specific error codes not yet documented
- **Firmware updates:** OTA update mechanism (if any) not documented
- **Pairing mode activation:** How to put device into pairing mode

## References

- **Implementation:** ESPHome custom component in `components/cosori_kettle_ble/`
- **Test captures:** `offbase.json` and various log files
- **Discussion:** Protocol analysis throughout development conversation

## ACK-Based Communication

V1 protocol supports ACK (acknowledgment) packets:

**Frame Type:** `0x12` (same as extended status)

**ACK Packet Structure:**
```
A5 12 [seq] [len] [cs] [command_header] [payload]
```

The ACK mirrors the sequence number and command header from the original message. For registration/hello commands, the payload indicates success:
- `0x00`: Success
- Other values: Failure

**Recommended Flow:**
1. Send command with sequence number
2. Wait for ACK with matching sequence number (timeout: 1-2 seconds)
3. Check ACK payload for success indicator
4. Retry or handle failure as needed

This replaces timing-based delays with proper protocol acknowledgments.

## Operating Modes

| Mode | Value | Temperature | Description |
|---|---|---|---|
| Green Tea | 0x01 | 180°F | Optimal for green tea |
| Oolong | 0x02 | 195°F | Optimal for oolong tea |
| Coffee | 0x03 | 205°F | Optimal for coffee |
| Boil | 0x04 | 212°F | Full boil |
| My Temp | 0x05 | Custom | User-defined temperature |
| Heat (V0) | 0x06 | Variable | Generic heating mode |

## Heating Stages

| Stage | Value | Description |
|---|---|---|
| Idle | 0x00 | Not heating |
| Heating | 0x01 | Actively heating |
| Almost Done | 0x02 | Near target temperature |
| Keep Warm | 0x03 | Holding temperature |

---

**Document Version:** 2.0
**Last Updated:** 2024-12-29
**Kettle Model:** Cosori Smart Electric Kettle
**Authors:** Reverse-engineered through BLE packet analysis
**Protocol Versions:** V0 (legacy), V1 (current)
