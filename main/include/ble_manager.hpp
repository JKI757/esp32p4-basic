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
#include <vector>

// Forward declarations for NimBLE types (headers included in implementation)
struct ble_gap_event;
struct ble_gatt_access_ctxt;

// Include necessary types for class definition
#define BLE_HS_CONN_HANDLE_NONE (0xffff)

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
    
    // Nordic UART Service UUIDs (actual UUIDs defined in implementation)
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

    /**
     * @brief Scan for nearby BLE devices
     * @param scan_duration_seconds Duration to scan in seconds (default: 5)
     * @return true if scan started successfully
     */
    bool start_scan(int scan_duration_seconds = 5);

    /**
     * @brief Stop ongoing BLE scan
     * @return true if scan stopped successfully
     */
    bool stop_scan();

    /**
     * @brief Check if BLE scan is currently active
     * @return true if scanning
     */
    bool is_scanning() const;

    /**
     * @brief Get number of devices found in last scan
     * @return Number of devices found
     */
    int get_scan_result_count() const;

    /**
     * @brief Get scan result by index
     * @param index Index of device (0 to get_scan_result_count()-1)
     * @return Device info string or empty if invalid index
     */
    std::string get_scan_result(int index) const;

    /**
     * @brief Get detailed BLE stack status for debugging
     * @return Status information string
     */
    std::string get_debug_status() const;

private:
    // NimBLE event handlers
    static int gap_event_handler(struct ble_gap_event *event, void *arg);
    static int gatt_access_handler(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
    static void on_sync_callback(void);
    static void on_reset_callback(int reason);
    static void nimble_host_task(void *param);

public:
    // Internal methods (public for C callbacks)
    void process_received_data(const uint8_t* data, uint16_t len);
    void handle_connection_event(struct ble_gap_event *event);
    void handle_disconnection_event(struct ble_gap_event *event);

private:
    // Internal methods
    void setup_gatt_service();
    bool configure_advertising_data(const std::string& device_name);
    
    // NimBLE service setup
    bool register_nus_service();
    void start_advertising_internal();
    
    // BLE data transmission helpers
    bool send_single_packet(const std::string& data);

    // State variables
    bool initialized_;
    bool advertising_;
    bool connected_;
    bool scanning_;
    uint16_t conn_handle_;
    uint16_t nus_service_handle_;
    uint16_t nus_rx_char_handle_;
    uint16_t nus_tx_char_handle_;
    std::string device_name_;
    
    // Scan results storage
    struct ScanResult {
        std::string address;
        std::string name;
        int rssi;
        std::string service_uuids;
    };
    std::vector<ScanResult> scan_results_;
    
    // Command processing callback
    std::function<std::string(const std::string&)> command_callback_;
    
public:
    // Static instance for C callbacks (public for callback access)
    static BLEManager* instance_;
};

} // namespace ble_serial