# ESPHome Cosori Kettle BLE Component

Control your Cosori smart kettle from Home Assistant using an ESP32 and BLE.
Amazon Link: https://www.amazon.com/COSORI-Electric-Gooseneck-Variable-Stainless/dp/B07T1CH2HH

## Features

- **Real-time monitoring**: Temperature, setpoint, on-base status, heating state
- **Remote control**: Start/stop heating, adjust target temperature (104-212°F)
- **Persistent connection**: Auto-reconnect on disconnect
- **BLE connection toggle**: Disable BLE to use official mobile app (kettle only supports 1 connection)
- **Home Assistant integration**: Native entities (sensors, switches, numbers)
- **Automation-ready**: Use in HA automations, scripts, and scenes

## Hardware Requirements

- **ESP32** board with Bluetooth LE support (e.g., ESP32-DevKitC, ESP32-WROOM-32)
- **Cosori smart kettle** with BLE (tested with model C4:A9:B8:73:AB:29)
- Stable power supply for ESP32

## Quick Start

**Copy this complete configuration, change the MAC address, and flash to your ESP32:**

```yaml
esphome:
  name: cosori-kettle
  platform: ESP32
  board: esp32dev

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "Cosori Kettle Fallback"
    password: !secret fallback_password

api:
  encryption:
    key: !secret api_encryption_key

ota:
  password: !secret ota_password

logger:

# External component from GitHub
external_components:
  - source: github://barrymichels/CosoriKettleBLE
    components: [cosori_kettle_ble]
    refresh: 0s

# BLE tracker
esp32_ble_tracker:
  scan_parameters:
    active: false

# BLE client - CHANGE THIS MAC ADDRESS TO YOUR KETTLE'S ADDRESS
ble_client:
  - mac_address: "C4:A9:B8:73:AB:29"
    id: cosori_kettle_client
    auto_connect: true

# Cosori kettle component
cosori_kettle_ble:
  ble_client_id: cosori_kettle_client
  id: my_kettle
  name: "Kettle"
  update_interval: 1s

# Sensors
sensor:
  - platform: cosori_kettle_ble
    cosori_kettle_ble_id: my_kettle
    temperature:
      name: "Kettle Temperature"
    kettle_setpoint:
      name: "Kettle Setpoint"

# Binary sensors
binary_sensor:
  - platform: cosori_kettle_ble
    cosori_kettle_ble_id: my_kettle
    on_base:
      name: "Kettle On Base"
    heating:
      name: "Kettle Heating"

# Number (target temperature control)
number:
  - platform: cosori_kettle_ble
    cosori_kettle_ble_id: my_kettle
    target_setpoint:
      name: "Kettle Target Temperature"

# Switches
switch:
  - platform: cosori_kettle_ble
    cosori_kettle_ble_id: my_kettle
    heating_switch:
      name: "Kettle Heating"
    ble_connection_switch:
      name: "Kettle BLE Connection"
```

**That's it!** Just find your kettle's MAC address (see below) and you're ready to go.

## Climate Entity & Thermostat Card

The kettle **automatically appears as a climate entity** in Home Assistant when you add the component! This means you can use the beautiful native **thermostat card** with its semi-circle temperature slider right away.

### Customizing the Climate Entity Name

In your ESPHome config, you can optionally set a name for the climate entity:

```yaml
cosori_kettle_ble:
  ble_client_id: cosori_kettle_client
  id: my_kettle
  name: "Kettle"  # This sets the climate entity name
```

### Using the Thermostat Card

Add this to your Lovelace dashboard:

```yaml
type: thermostat
entity: climate.kettle  # or whatever name you chose
```

The thermostat card provides:
- **Semi-circle temperature slider** (40-100°C / 104-212°F)
- **Current temperature** display
- **Mode control** (OFF / HEAT)
- **Action indicator** (IDLE / HEATING)

### How It Works

- **OFF mode**: Kettle is idle, not heating
- **HEAT mode**: Kettle will heat to target temperature
- **Temperature slider**: Adjust target temperature (40-100°C / 104-212°F)
- **Current temperature**: Shows actual water temperature
- **Action**: Shows HEATING when actively warming, IDLE otherwise

**Note:** Home Assistant will display temperatures in your preferred unit (Celsius or Fahrenheit). The kettle natively uses Fahrenheit, and all conversions are handled automatically.

### Alternative: Individual Entities

You can also use the individual entities (sensors, switches, numbers) for more granular control or custom card designs. Both approaches work simultaneously!

## Finding Your Kettle's MAC Address

### Method 1: Using `bluetoothctl` (Linux)

```bash
sudo bluetoothctl
scan on
# Look for your kettle in the list (usually shows as "Cosori" or similar)
# Note the MAC address (e.g., C4:A9:B8:73:AB:29)
scan off
exit
```

### Method 2: Using BLE Scanner App

- **iOS**: Download "BLE Scanner" or "LightBlue"
- **Android**: Download "nRF Connect" or "BLE Scanner"
- Scan for devices and look for your kettle
- Note the MAC address

### Method 3: Using Python Script

```python
#!/usr/bin/env python3
import asyncio
from bleak import BleakScanner

async def main():
    devices = await BleakScanner.discover()
    for d in devices:
        if "cosori" in d.name.lower() or "kettle" in d.name.lower():
            print(f"Found: {d.name} - {d.address}")

asyncio.run(main())
```

## Advanced Configuration

See [cosori-kettle-example.yaml](cosori-kettle-example.yaml) for a complete configuration example.

## Entities

### Sensors

| Entity | Type | Description | Unit |
|--------|------|-------------|------|
| `temperature` | Sensor | Current water temperature | °F |
| `kettle_setpoint` | Sensor | Actual setpoint on kettle | °F |

### Binary Sensors

| Entity | Type | Description |
|--------|------|-------------|
| `on_base` | Binary Sensor | True if kettle is on charging base |
| `heating` | Binary Sensor | True if kettle is actively heating |

### Numbers

| Entity | Type | Description | Range |
|--------|------|-------------|-------|
| `target_setpoint` | Number | User-adjustable target temperature | 104-212°F |

### Switches

| Entity | Type | Description |
|--------|------|-------------|
| `heating_switch` | Switch | Start/stop heating (uses target_setpoint) |
| `ble_connection_switch` | Switch | Enable/disable BLE connection |

## Usage

### Starting the Kettle

1. **Set target temperature**:
   ```yaml
   service: number.set_value
   target:
     entity_id: number.kettle_target_temperature
   data:
     value: 180
   ```

2. **Turn on heating switch**:
   ```yaml
   service: switch.turn_on
   target:
     entity_id: switch.kettle_heating
   ```

### Stopping the Kettle

```yaml
service: switch.turn_off
target:
  entity_id: switch.kettle_heating
```

### Using Official Mobile App

To use the official Cosori mobile app (which requires exclusive BLE access):

1. Turn off BLE connection switch in Home Assistant
2. Use the mobile app as normal
3. Turn on BLE connection switch when done

## Home Assistant Automations

### Example 1: Morning Kettle Automation

```yaml
automation:
  - alias: "Morning Kettle"
    trigger:
      - platform: time
        at: "07:00:00"
    condition:
      - condition: state
        entity_id: binary_sensor.kettle_on_base
        state: "on"
    action:
      - service: number.set_value
        target:
          entity_id: number.kettle_target_temperature
        data:
          value: 212
      - service: switch.turn_on
        target:
          entity_id: switch.kettle_heating
```

### Example 2: Kettle Ready Notification

**Note:** The kettle holds temperature when it reaches setpoint, so heating doesn't turn off. Use temperature threshold instead:

```yaml
automation:
  - alias: "Kettle Ready"
    trigger:
      - platform: numeric_state
        entity_id: sensor.kettle_temperature
        above: 205  # Trigger a few degrees before setpoint
    condition:
      - condition: state
        entity_id: binary_sensor.kettle_heating
        state: "on"
    action:
      - service: notify.mobile_app
        data:
          title: "Kettle Ready"
          message: "Your water is hot! ☕"
```

### Example 3: Using Climate Entity

```yaml
automation:
  - alias: "Morning Tea"
    trigger:
      - platform: time
        at: "07:00:00"
    condition:
      - condition: state
        entity_id: binary_sensor.kettle_on_base
        state: "on"
    action:
      # Set temperature using climate entity
      - service: climate.set_temperature
        target:
          entity_id: climate.kettle
        data:
          temperature: 100  # 100°C = 212°F
      # Turn on heating
      - service: climate.set_hvac_mode
        target:
          entity_id: climate.kettle
        data:
          hvac_mode: heat
      # Wait 5 minutes then notify
      - delay: "00:05:00"
      - service: notify.mobile_app
        data:
          message: "Your tea water should be ready! ☕"
```

## Protocol Information

This component implements the Cosori kettle BLE protocol reverse-engineered from packet captures:

- **Service UUID**: `0000fff0-0000-1000-8000-00805f9b34fb`
- **RX Characteristic**: `0000fff1-...` (notifications from kettle)
- **TX Characteristic**: `0000fff2-...` (commands to kettle)
- **Sequence**: Registration handshake → Continuous polling (1s) → Commands

See the [protocol documentation](protocol.md) for details (if available).

## Troubleshooting

### Kettle Not Connecting

1. **Check MAC address**: Ensure it matches your kettle
2. **Check distance**: ESP32 should be within ~10m of kettle
3. **Check kettle**: Ensure it's on the base and powered
4. **Check logs**: Enable `logger` in ESPHome config
5. **Restart ESP32**: Sometimes BLE stack needs reset

### Connection Drops

- **Power supply**: Use stable 5V power supply (not USB from computer)
- **WiFi interference**: BLE and WiFi share radio on ESP32, reduce WiFi activity
- **Distance**: Move ESP32 closer to kettle

### Kettle Shows "Unavailable"

- Check ESPHome device status in Home Assistant
- Verify ESP32 is online
- Check BLE connection switch is ON
- Restart ESPHome device

### Commands Not Working

- Ensure kettle is on base (`binary_sensor.kettle_on_base` should be ON)
- Check connection status
- Enable debug logging to see protocol packets

### Temperature Doesn't Reach Exact Setpoint

The kettle may report temperatures 1-3°F below the setpoint when holding temperature. This is normal behavior:

- The kettle cycles heating to maintain temperature
- Temperature readings may fluctuate between 209-212°F when set to 212°F
- Use a threshold automation (e.g., `above: 205`) instead of exact temperature matching
- The `sensor.kettle_temperature` entity shows the actual kettle reading (always updated)
- The climate entity temperature may lag slightly due to Fahrenheit→Celsius→Fahrenheit conversion

**For accurate temperature monitoring**, use the dedicated `sensor.kettle_temperature` entity in your automations rather than the climate entity's current temperature.

## Debug Logging

Enable detailed logging in your YAML:

```yaml
logger:
  level: DEBUG
  logs:
    cosori_kettle_ble: VERBOSE
    ble_client: DEBUG
    esp32_ble_tracker: DEBUG
```

## Credits

- Inspired by [esphome-jk-bms](https://github.com/syssi/esphome-jk-bms)
- Protocol reverse-engineered using Wireshark and Python/Bleak

## License

MIT License - see LICENSE file for details

## Contributing

Contributions welcome! Please open an issue or PR on GitHub.

## Disclaimer

This is an unofficial third-party component. Use at your own risk. The author is not affiliated with Cosori.
