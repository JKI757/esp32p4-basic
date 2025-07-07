/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <memory>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

#include "host/ble_hs.h"
#include "host/ble_uuid.h"

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
    
    // Nordic UART Service UUIDs
    static constexpr ble_uuid128_t UART_SERVICE_UUID = 
        BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                         0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
    
    static constexpr ble_uuid128_t UART_RX_UUID = 
        BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                         0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
    
    static constexpr ble_uuid128_t UART_TX_UUID = 
        BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                         0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

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
    // BLE event handlers
    static int gap_event_handler(struct ble_gap_event *event, void *arg);
    static int gatt_access_handler(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);

    // Internal methods
    void setup_gatt_service();
    bool configure_advertising_data(const std::string& device_name);
    void handle_connection_event(struct ble_gap_event *event);
    void handle_disconnection_event(struct ble_gap_event *event);
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