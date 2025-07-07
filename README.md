# ESP32-P4 Foundational Firmware

| Supported Targets | ESP32-P4 |
| ----------------- | ----- |

## Overview

This project serves as a **foundational firmware framework** for ESP32-P4 based applications, starting with a robust WiFi configuration system. Built using modern C++ practices and designed for extensibility, this codebase provides the groundwork for complex multimedia and IoT applications.

**Current Implementation**: WiFi configuration system using ESP32-P4 + ESP32-C6 architecture with interactive USB interface.

**Future Vision**: This foundation will evolve to support:
- **Touchscreen GUI Control**: LVGL-based user interfaces with gesture support
- **Audio Processing**: Real-time audio ingest, processing, and playback via ES8311 codec
- **Peripheral Management**: Comprehensive control of sensors, actuators, and external devices
- **Multimedia Applications**: Camera integration, video processing, and display management
- **Advanced Networking**: Mesh networking, IoT protocols, and cloud connectivity

## Architecture

**Hardware Configuration:**
- **ESP32-P4**: Main processor (no native WiFi) handling application logic and user interface
- **ESP32-C6**: WiFi coprocessor connected via SDIO providing WiFi functionality  
- **Communication**: ESP-Hosted framework over SDIO transport

**Software Architecture:**
- Modern C++ project structure with proper separation of concerns
- WiFiManager class: Handles ESP32-C6 communication and WiFi operations
- CommandInterpreter class: Provides interactive USB Serial JTAG interface
- Event-driven architecture with automatic reconnection capabilities

## Current Features (v1.0)

- **WiFi Management**: Network scanning with RSSI-based sorting, interactive connection
- **USB Interface**: Real-time command processing with interactive commands
- **ESP-Hosted Integration**: Seamless P4↔C6 communication over SDIO
- **Modern C++ Architecture**: Smart pointers, STL containers, RAII patterns
- **Extensible Design**: Modular architecture ready for feature expansion

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
│   ├── main.cpp                     # Clean entry point (30 lines)
│   ├── include/
│   │   ├── wifi_manager.hpp         # WiFi management class interface
│   │   └── command_interpreter.hpp  # USB command interface
│   ├── src/
│   │   ├── wifi_manager.cpp         # WiFi implementation using ESP-Hosted
│   │   └── command_interpreter.cpp  # Interactive command processing
│   ├── CMakeLists.txt              # Component registration
│   └── idf_component.yml           # ESP-Hosted dependencies
├── sdkconfig.defaults.esp32p4       # ESP32-P4 specific configuration
└── .gitignore                      # Build artifacts exclusion
```

## Hardware Requirements

- Waveshare ESP32-P4 development board with ESP32-C6 coprocessor
- USB cable for power and programming
- Optional: 7" or 10.1" touchscreen display for GUI extension

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

Once flashed and running, connect via USB Serial JTAG and use these commands:

- `scan` or `s`: Scan for available WiFi networks
- `list` or `l`: List previously scanned networks  
- `connect <index>` or `c <index>`: Connect to network by index
- `status` or `st`: Show current WiFi connection status
- `disconnect` or `d`: Disconnect from current network
- `help` or `h`: Show command help

## Example Output

```text
I (2000) WiFiManager: WiFi Manager initialized successfully
I (2010) CommandInterpreter: Command interpreter started
WiFi Configuration Tool - Commands: scan, list, connect <index>, status, disconnect, help
> scan
I (5230) WiFiManager: Starting WiFi scan
I (8340) WiFiManager: Found 12 WiFi networks
Networks found:
  [0] MyHomeWiFi (WPA2, RSSI: -45 dBm)
  [1] OfficeNetwork (WPA2, RSSI: -52 dBm)
  [2] Guest_Network (Open, RSSI: -67 dBm)
> connect 0
Enter password for 'MyHomeWiFi': ********
I (15670) WiFiManager: Connected successfully to MyHomeWiFi
Connected to MyHomeWiFi
IP Address: 192.168.1.100
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

- If WiFi scan fails, ensure ESP32-C6 is properly connected and ESP-Hosted is initialized
- For connection issues, verify network credentials and signal strength
- Use `idf.py monitor` to see detailed logging output
- Check SDIO communication if WiFi remote functions fail

For technical queries, refer to the [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/) or [ESP-Hosted documentation](https://github.com/espressif/esp-hosted).

## Development Roadmap

### Phase 1: Foundation ✅ (Current)
- [x] ESP32-P4 + ESP32-C6 architecture
- [x] WiFi configuration system
- [x] Modern C++ framework
- [x] Interactive USB interface

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