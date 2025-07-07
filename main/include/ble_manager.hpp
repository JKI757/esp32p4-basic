/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// NOTE: ESP-IDF Embedded Development Constraints
// - No exception handling (-fno-exceptions)
// - No RTTI (-fno-rtti) 
// - Use error codes and boolean returns instead of exceptions
// - Memory management through ESP-IDF heap functions

#include <string>
#include <memory>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

#include "nimble/ble.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"

#ifdef __cplusplus
}
#endif

namespace ble_serial {

// Forward declaration
class CommandInterpreter;

/**
 * @brief BLE Manager class for ESP32-P4 + ESP32-C6 architecture
 * 
 * Implements Nordic UART Service (NUS) over ESP-Hosted BLE for wireless
 * command interface access. Uses NimBLE stack with Hosted HCI transport.
 */
class BLEManager {
public:
    static constexpr size_t MAX_DATA_LEN = 512;
    
    // Nordic UART Service UUIDs (will be defined in implementation)
    // Service:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
    // RX Char:  6E400002-B5A3-F393-E0A9-E50E24DCCA9E  
    // TX Char:  6E400003-B5A3-F393-E0A9-E50E24DCCA9E

    BLEManager();
    ~BLEManager();

    /**
     * @brief Initialize BLE stack and Nordic UART Service
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Start BLE advertising with device name
     * @param device_name Name to advertise (default: "ESP32-P4-WiFi")
     * @return true if advertising started successfully
     */
    bool start_advertising(const std::string& device_name = "ESP32-P4-WiFi");

    /**
     * @brief Stop BLE advertising
     * @return true if advertising stopped successfully
     */
    bool stop_advertising();

    /**
     * @brief Check if a BLE client is connected
     * @return true if connected
     */
    bool is_connected() const;

    /**
     * @brief Send response data to connected BLE client
     * @param data Response string to send
     * @return true if data sent successfully
     */
    bool send_response(const std::string& data);

    /**
     * @brief Set callback for processing received commands
     * @param callback Function to call when command received from BLE
     */
    void set_command_callback(std::function<std::string(const std::string&)> callback);

private:
    // BLE event handlers (implementation specific)
    static int gap_event_handler(void *event, void *arg);
    static int gatt_access_handler(uint16_t conn_handle, uint16_t attr_handle,
                                   void *ctxt, void *arg);

    // Internal methods
    void setup_gatt_service();
    bool configure_advertising_data(const std::string& device_name);
    void handle_connection_event(void *event);
    void handle_disconnection_event(void *event);
    void process_received_data(const uint8_t* data, uint16_t len);

    // State variables
    bool initialized_;
    bool advertising_;
    bool connected_;
    uint16_t conn_handle_;
    uint16_t tx_char_handle_;
    
    // Command processing callback
    std::function<std::string(const std::string&)> command_callback_;
    
    // Static instance for C callbacks
    static BLEManager* instance_;
};

} // namespace ble_serial