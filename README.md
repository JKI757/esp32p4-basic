# ESP32-P4 Development & Control Tool

| Supported Targets | ESP32-P4 |
| ----------------- | ----- |

## Overview

This project is a **comprehensive development and control tool** for ESP32-P4 based applications. Built using modern C++ practices and designed for extensibility, it provides WiFi configuration, BLE wireless interface, and relay control capabilities for both development and production use.

**Current Implementation**: Complete WiFi + BLE + Relay control system using ESP32-P4 + ESP32-C6 architecture with dual interface support (USB Serial JTAG and BLE Nordic UART Service).

**Key Capabilities**:
- **WiFi Configuration**: Network scanning, connection management, and status monitoring
- **BLE Wireless Interface**: Nordic UART Service for remote command access
- **Relay Control**: Dual relay board support with individual and simultaneous control
- **Board Variant Detection**: Automatic detection of single board vs dual relay board variants
- **Dual Command Interface**: Both USB Serial JTAG and wireless BLE UART access

**Future Vision**: This foundation will evolve to support:
- **Touchscreen GUI Control**: LVGL-based user interfaces with gesture support
- **Audio Processing**: Real-time audio ingest, processing, and playback via ES8311 codec
- **Peripheral Management**: Comprehensive control of sensors, actuators, and external devices
- **Multimedia Applications**: Camera integration, video processing, and display management
- **Advanced Networking**: Mesh networking, IoT protocols, and cloud connectivity

## Architecture

**Hardware Configuration:**
- **ESP32-P4**: Main processor (no native WiFi/BLE) handling application logic and user interface
- **ESP32-C6**: WiFi/BLE coprocessor connected via SDIO providing wireless functionality  
- **Relay Board**: Optional dual relay board with GPIO32 and GPIO46 control
- **Communication**: ESP-Hosted framework over SDIO transport

**Software Architecture:**
- Modern C++ project structure with proper separation of concerns
- WiFiManager class: Handles ESP32-C6 communication and WiFi operations
- BLEManager class: Provides wireless BLE Nordic UART Service interface  
- RelayManager class: Controls dual relay board with safety features
- CommandInterpreter class: Unified command interface for USB and BLE
- Event-driven architecture with automatic reconnection and board variant detection

## Current Features (v1.0)

### Core Functionality
- **WiFi Management**: Network scanning with RSSI-based sorting, interactive connection management
- **BLE Wireless Interface**: Nordic UART Service with wireless command access and response chunking
- **Relay Control**: Dual relay board support (GPIO32, GPIO46) with individual and simultaneous control
- **Board Variant Detection**: Automatic detection and graceful fallback for single vs dual relay boards

### Interface & Architecture  
- **Dual Command Interface**: USB Serial JTAG and BLE UART with unified command set
- **ESP-Hosted Integration**: Seamless P4↔C6 communication over VHCI/SDIO transport
- **Modern C++ Architecture**: Smart pointers, STL containers, RAII patterns, and safety features
- **Extensible Design**: Modular manager classes ready for feature expansion

### Safety & Reliability
- **Auto-off Safety**: Relays automatically turn off on system destruction
- **Error Handling**: Comprehensive error checking and logging throughout
- **State Tracking**: Operation statistics and relay switch counting
- **Robust BLE**: Response chunking for large data and proper handle management

## Planned Features (Future Releases)

- **Touch GUI Interface**: LVGL-based touchscreen controls with terminal output display
- **Audio System**: Real-time audio capture, processing, and playback
- **Camera Integration**: Video capture and processing capabilities
- **Sensor Management**: Unified interface for multiple sensor types
- **Device Control**: GPIO, I2C, SPI peripheral management
- **Data Logging**: Local storage and cloud synchronization
- **OTA Updates**: Secure firmware update system

## Project Structure

```
p4-base/
├── main/
│   ├── main.cpp                     # Clean entry point with board variant detection
│   ├── include/
│   │   ├── wifi_manager.hpp         # WiFi management class interface
│   │   ├── ble_manager.hpp          # BLE Nordic UART Service interface
│   │   ├── relay_manager.hpp        # Relay control class interface
│   │   └── command_interpreter.hpp  # Unified USB/BLE command interface
│   ├── src/
│   │   ├── wifi_manager.cpp         # WiFi implementation using ESP-Hosted
│   │   ├── ble_manager.cpp          # BLE implementation using ESP-Hosted NimBLE
│   │   ├── relay_manager.cpp        # Relay control with safety features
│   │   └── command_interpreter.cpp  # Unified command processing for both interfaces
│   ├── CMakeLists.txt              # Component registration with all sources
│   └── idf_component.yml           # ESP-Hosted and WiFi Remote dependencies
├── sdkconfig.defaults.esp32p4       # ESP32-P4 specific configuration
└── .gitignore                      # Build artifacts exclusion
```

## Hardware Requirements

### Required
- Waveshare ESP32-P4 development board with ESP32-C6 coprocessor
- USB cable for power and programming

### Optional (Board Variants)
- **Dual Relay Board**: Relay control module with GPIO32 and GPIO46 connections
- **Touchscreen Display**: 7" or 10.1" display for future GUI extension

## Software Dependencies

- **ESP-IDF**: v5.x framework
- **ESP-Hosted**: Communication framework between ESP32-P4 and ESP32-C6  
- **WiFi Remote**: Automatic API remapping for remote WiFi calls
- **LVGL**: (for future GUI development) v9.2.0

## Build Instructions

All commands must be run from the `p4-base/` directory:

```bash
# Set target to ESP32-P4 (required first step)
idf.py set-target esp32p4

# Configure project (optional - defaults should work)
idf.py menuconfig

# Build project
idf.py build

# Flash to device and monitor
idf.py -p PORT flash monitor
```

To exit the serial monitor, type `Ctrl-]`.

## Usage

Once flashed and running, connect via USB Serial JTAG or BLE UART and use these commands:

### WiFi Commands
- `scan` or `s`: Scan for available WiFi networks
- `list` or `l`: List previously scanned networks  
- `connect <ssid> <password>`: Connect to WiFi network
- `status` or `st`: Show current WiFi connection status
- `disconnect` or `d`: Disconnect from current network

### BLE Commands  
- `ble_start` or `bs`: Start BLE advertising (Nordic UART Service)
- `ble_stop` or `bp`: Stop BLE advertising
- `ble_status` or `bt`: Show detailed BLE status
- `ble_scan [duration]` or `bsc`: Scan for nearby BLE devices
- `ble_name <name>` or `bn`: Set BLE device name
- `ble_debug` or `bd`: Show comprehensive BLE debug information

### Relay Commands (Dual Relay Board Only)
- `relay_on [1|2|all]`: Turn on relay(s) - defaults to all if no argument
- `relay_off [1|2|all]`: Turn off relay(s) - defaults to all if no argument  
- `relay_toggle [1|2|all]`: Toggle relay(s) - defaults to all if no argument
- `relay_status`: Show current relay states and board variant
- `relay_debug`: Show comprehensive relay debug information with statistics

### General Commands
- `help` or `h`: Show command help with board-specific available commands

### Interface Access
- **USB Serial JTAG**: Direct connection via USB cable
- **BLE UART**: Wireless access via Nordic UART Service (Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E)

## Example Output

### System Startup
```text
I (2000) WiFiManager: WiFi Manager initialized successfully
I (2010) BLEManager: BLE Manager initialized successfully
I (2020) RelayManager: Relay Manager initialized successfully
I (2030) main: Dual relay board variant detected
I (2040) CommandInterpreter: Command interpreter started

=========================================
  ESP32-P4 Development & Control Tool  
=========================================
WiFi • BLE • Relay Control • Commands
Type 'help' for available commands
```

### WiFi Usage Example
```text
> scan
I (5230) WiFiManager: Starting WiFi scan
I (8340) WiFiManager: Found 12 WiFi networks
Networks found:
  [0] MyHomeWiFi (WPA2, RSSI: -45 dBm)
  [1] OfficeNetwork (WPA2, RSSI: -52 dBm)
  [2] Guest_Network (Open, RSSI: -67 dBm)
> connect MyHomeWiFi password123
I (15670) WiFiManager: Connected successfully to MyHomeWiFi
Connected to MyHomeWiFi
IP Address: 192.168.1.100
```

### Relay Control Example  
```text
> relay_status
=== Relay Status ===
Board Variant: Dual Relay Board
Relay 1 (GPIO32): OFF
Relay 2 (GPIO46): OFF

> relay_on 1
I (25000) RelayManager: Set Relay 1 to ON (GPIO32)
Relay 1 turned ON

> relay_toggle all
I (30000) RelayManager: Set Relay 1 to OFF (GPIO32)
I (30010) RelayManager: Set Relay 2 to ON (GPIO46)
All relays toggled
```

### BLE Interface Example
```text
> ble_start
I (35000) BLEManager: Starting BLE advertising
I (35010) BLEManager: BLE device 'ESP32P4-DEV' is now discoverable
BLE advertising started - Device discoverable as 'ESP32P4-DEV'

# Commands can now be sent wirelessly via BLE UART
# Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
```

## Development Notes

### ESP-Hosted Integration
- Standard `esp_wifi_*` functions are automatically remapped to remote calls
- Communication handled transparently by ESP-Hosted framework
- SDIO transport provides reliable P4↔C6 communication

### GUI Extension Ready
This project is designed for easy extension with a touchscreen GUI:
- Reference examples available in `../waveshare-source/ESP32-P4-Module-DEV-KIT_Demo/`
- LVGL integration examples in `3_Advanced/LVGL/`
- BSP support for 7" and 10.1" displays

### Code Quality
- Modern C++ patterns throughout
- Comprehensive error handling and logging
- Event-driven architecture for responsiveness
- Clean separation of WiFi management and user interface

## Troubleshooting

### WiFi Issues
- If WiFi scan fails, ensure ESP32-C6 is properly connected and ESP-Hosted is initialized
- For connection issues, verify network credentials and signal strength  
- Check SDIO communication if WiFi remote functions fail

### BLE Issues
- If BLE initialization fails, verify ESP-Hosted BLE configuration in sdkconfig
- Ensure VHCI transport is enabled, UART transport is disabled
- Use `ble_debug` command for comprehensive BLE stack status

### Relay Issues  
- If relay commands fail, check board variant detection in startup logs
- Verify GPIO32 and GPIO46 connections on dual relay board
- Single board variant will show "Relay Manager initialization failed" (normal)

### General Debugging
- Use `idf.py monitor` to see detailed logging output
- Check system logs for component initialization status
- Verify USB Serial JTAG driver installation for command interface

For technical queries, refer to the [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/) or [ESP-Hosted documentation](https://github.com/espressif/esp-hosted).

## Development Roadmap

### Phase 1: Foundation ✅ (Completed)
- [x] ESP32-P4 + ESP32-C6 architecture with ESP-Hosted
- [x] WiFi configuration system with network scanning and management
- [x] BLE Nordic UART Service with wireless command interface  
- [x] Relay control system with dual board variant support
- [x] Modern C++ framework with manager class architecture
- [x] Unified command interface for USB Serial JTAG and BLE UART
- [x] Board variant detection and safety features

### Phase 2: GUI Interface 🔄 (In Progress)
- [ ] LVGL integration with touchscreen
- [ ] Terminal output display with action buttons
- [ ] Touch-responsive WiFi configuration
- [ ] System status dashboard

### Phase 3: Audio System 📋 (Planned)
- [ ] ES8311 codec integration
- [ ] Real-time audio capture
- [ ] Audio processing pipeline
- [ ] Playback control system

### Phase 4: Advanced Features 📋 (Future)
- [ ] Camera integration and processing
- [ ] Sensor management framework
- [ ] Device control interfaces
- [ ] Cloud connectivity and OTA updates

## Contributing

This project is designed as a learning platform and foundation for ESP32-P4 development. The modular architecture makes it easy to:
- Add new peripheral drivers
- Implement additional UI components
- Extend networking capabilities
- Integrate audio/video processing

Each major feature is implemented as a separate class with clean interfaces, making the codebase approachable for developers of different skill levels.