# Guide: Extracting Cosori Kettle BLE Handshake from Packet Capture Logs

This guide explains how to identify and extract the BLE handshake sequence from packet capture logs for Cosori kettles, which is needed to configure the ESPHome component.

## Overview

The Cosori kettle requires a specific handshake sequence during initial connection. This handshake contains a registration key that authenticates your device with the kettle. Without the correct handshake, the kettle will not respond to commands.

## Tools Needed

- BLE packet capture tool (e.g., nRF Connect, Wireshark with BLE support, PacketLogger on macOS)
- Text editor
- The official Cosori mobile app (for capturing the legitimate handshake)

## Step 1: Capture the BLE Traffic

1. Start your BLE packet capture tool
2. Open the Cosori mobile app
3. Connect to your kettle through the app
4. Let the connection establish completely
5. Stop the packet capture
6. Export the log as text

## Step 2: Understand the Log Format

Your log will contain entries like this:
```
Jan 04 08:02:40.876  ATT Send         0x0405  00:00:00:00:00:00  Write Request - Handle:0x000E - Value: A522 0424 002E 0181 D100 3766 3836 3839…  05 04 1B 00 17 00 04 00 12 0E 00 A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64
```

Key components:
- **Timestamp**: When the packet was sent
- **Direction**: "ATT Send" (to kettle) or "ATT Receive" (from kettle)
- **Type**: "Write Request" or "Handle Value Notification"
- **Handle**: BLE characteristic handle (look for 0x000E for writes TO the kettle)
- **Value**: The actual data being sent (shown in hex)

## Step 3: Identify the Handshake Sequence

### 3.1 Find the Connection Start

Look for the very first few "ATT Send" packets with "Write Request - Handle:0x000E" after connection establishment. The handshake typically happens within the first second of connection.

**In the example log:**
- First packet: `Jan 04 08:02:40.876`
- Second packet: `Jan 04 08:02:40.920` (44ms later)
- Third packet: `Jan 04 08:02:40.981` (61ms later)

### 3.2 Distinguish Handshake from Status Updates

**Handshake characteristics:**
- Occurs immediately after connection
- Typically 2-3 packets in rapid succession
- Contains ASCII-encoded hex characters in the payload
- Followed by a response from the kettle

**Status update characteristics:**
- Occur periodically (every few seconds to 30 seconds)
- Start with patterns like `A5 22 6A 0C` or `A5 22 6B 0C` (incrementing packet counter)
- Contain temperature and state information

**In the example:**
```
Handshake packets (08:02:40.876 - 08:02:40.981):
A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64
65 30 35 36 62 36 30 62 35 34 30 33 34 33 33 61 64 34 32 62
64 63

First status update (08:02:51.209 - much later):
A5 22 6A 0C 00 E7 01 41 40 00 00 00 B4 A5 00 00 00 00
```

Notice the handshake packets have `04 24` early in the sequence, while status updates have incrementing counters like `6A 0C`, `6B 0C`, `6C 0C`, etc.

## Step 4: Extract the Handshake Packets

### 4.1 Locate the Exact Payload

For each handshake packet, extract ONLY the data portion after "Value:". Ignore the protocol overhead at the beginning.

**Example from raw log line:**
```
ATT Send - Handle:0x000E - Value: A522 0424 002E 0181 D100 3766 3836 3839…  05 04 1B 00 17 00 04 00 12 0E 00 A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64
```

The full hex data is:
```
05 04 1B 00 17 00 04 00 12 0E 00 A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64
```

The actual payload starts AFTER the ATT header (after `12 0E 00`):
```
A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64
```

### 4.2 Extract All Handshake Packets

From the example log, the three handshake packets are:

**Packet 1:**
```
A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64
```

**Packet 2:**
```
65 30 35 36 62 36 30 62 35 34 30 33 34 33 33 61 64 34 32 62
```

**Packet 3:**
```
64 63
```

## Step 5: Verify the Handshake

### 5.1 Check for Kettle Response

After the handshake packets, you should see a response from the kettle (ATT Receive, Handle Value Notification):

```
Jan 04 08:02:41.040  ATT Receive  Handle Value Notification - Handle:0x0010 - Value: A512 0405 00EC 0181 D100 00
```

This confirms the kettle accepted the handshake.

### 5.2 Decode the Registration Key (Optional)

The handshake packets contain an embedded registration key in ASCII-encoded hex format.

**Convert the relevant bytes to ASCII:**

Packet 1 suffix: `37 66 38 36 38 39 36 32 63 64` → ASCII: `7f868962cd`
Packet 2 all: `65 30 35 36 62 36 30 62 35 34 30 33 34 33 33 61 64 34 32 62` → ASCII: `e0556b60b540343ad42b`
Packet 3 all: `64 63` → ASCII: `dc`

**Combined key:** `7f868962cde0556b60b540343ad42bdc` (32 hex characters = 128-bit key)

## Step 6: Format for ESPHome Configuration

### 6.1 Add Spaces Between Bytes

Format each packet with spaces between each byte pair:

```yaml
handshake:
  - "A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64"
  - "65 30 35 36 62 36 30 62 35 34 30 33 34 33 33 61 64 34 32 62"
  - "64 63"
```

### 6.2 Complete Configuration Example

```yaml
external_components:
  - source: github://barrymichels/CosoriKettleBLE
    components: [cosori_kettle_ble]
    refresh: 0s

ble_client:
  - mac_address: "7C:FE:62:2B:93:A8"  # Your kettle's MAC
    id: cosori_kettle_client
    auto_connect: true

cosori_kettle_ble:
  ble_client_id: cosori_kettle_client
  id: my_kettle
  name: "Kettle"
  update_interval: 10s
  handshake:
    - "A5 22 04 24 00 2E 01 81 D1 00 37 66 38 36 38 39 36 32 63 64"
    - "65 30 35 36 62 36 30 62 35 34 30 33 34 33 33 61 64 34 32 62"
    - "64 63"
```

## Step 7: Troubleshooting

### Problem: Can't Find Handshake Packets

**Solutions:**
- Make sure you captured the INITIAL connection, not a reconnection
- Look for the very first Write Request packets after connection
- Try clearing the kettle from the app and re-pairing it while capturing

### Problem: Multiple Similar Packets

**Solutions:**
- Focus on the first 2-5 seconds after connection
- Look for packets with sequential timing (milliseconds apart)
- Ignore packets that come 10+ seconds after connection

### Problem: Different Packet Structure

**Solutions:**
- Different kettle models may have different handshake structures
- Look for the pattern of rapid sequential writes to the same handle
- The registration key might be in a different position or format

### Problem: Kettle Doesn't Respond

**Solutions:**
- Verify you captured ALL handshake packets (don't miss any)
- Check that byte spacing is correct in your YAML
- Ensure the MAC address matches your kettle
- Try capturing the handshake again - keys might be session-specific

## Tips for Success

1. **Capture on first pairing**: The clearest handshake is during initial pairing in the app
2. **Start capture before opening app**: This ensures you catch the entire sequence
3. **Don't filter too aggressively**: Capture all BLE traffic, filter later
4. **Document your findings**: Save the raw log, extracted packets, and working config
5. **Test quickly**: Some kettles may have time-limited registration keys

## Alternative: If Your Kettle Uses Different Protocol

Some Cosori kettle models use different protocols. Look for these patterns:

### Pattern 1: Single Large Packet
One write containing the entire registration key

### Pattern 2: Multiple Small Packets
Like the example - key split across multiple writes

### Pattern 3: Challenge-Response
Kettle sends a challenge, app responds with computed value

If your log doesn't match the patterns in this guide, document the unique characteristics and try configuring based on what you observe.

## Appendix: Common Handle Numbers

Different kettle models may use different BLE handles:

- **0x000E**: Write handle (most common for commands)
- **0x0010**: Notification handle (most common for kettle responses)
- **0x000C**: Alternate write handle (some models)
- **0x0012**: Alternate notification handle (some models)

Always check your specific log to confirm which handles are used.

## Appendix: Example Log Analysis Workflow

1. Open log in text editor
2. Search for "ATT Send"
3. Filter to only Handle:0x000E
4. Find the first 3-5 occurrences
5. Extract hex values after the handle number
6. Check for corresponding "ATT Receive" responses
7. Format for YAML
8. Test and iterate

---

**Created:** 2026-01-04
**Based on:** Cosori Kettle Model 7C:FE:62:2B:93:A8 packet capture
**Component:** barrymichels/CosoriKettleBLE for ESPHome
