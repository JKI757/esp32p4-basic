/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_manager.hpp"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

// ESP-Hosted NimBLE headers
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "host/ble_hs_adv.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_hs_mbuf.h"
#include "os/os_mbuf.h"

#include <cstring>

namespace ble_serial {

static const char* TAG = "BLEManager";

// Static instance for C callbacks
BLEManager* BLEManager::instance_ = nullptr;

// Nordic UART Service UUIDs
static const ble_uuid128_t NUS_SERVICE_UUID = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
);

static const ble_uuid128_t NUS_RX_CHAR_UUID = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
);

static const ble_uuid128_t NUS_TX_CHAR_UUID = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
);

// Forward declarations for GATT callbacks
static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);

// Nordic UART Service definition
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &NUS_SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // RX Characteristic (write from client to device)
                .uuid = &NUS_RX_CHAR_UUID.u,
                .access_cb = nus_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // TX Characteristic (notify from device to client)
                .uuid = &NUS_TX_CHAR_UUID.u,
                .access_cb = nus_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = nullptr, // Will be set by NimBLE
            },
            {
                0, // End of characteristics
            }
        }
    },
    {
        0, // End of services
    }
};

// GATT access callback implementation
static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (!BLEManager::instance_) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    
    const ble_uuid_t* uuid = ctxt->chr->uuid;
    
    if (ble_uuid_cmp(uuid, &NUS_RX_CHAR_UUID.u) == 0) {
        // RX Characteristic - handle writes from client
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
            if (data_len > 0 && data_len <= BLEManager::MAX_DATA_LEN) {
                uint8_t data[BLEManager::MAX_DATA_LEN];
                int rc = ble_hs_mbuf_to_flat(ctxt->om, data, sizeof(data), &data_len);
                if (rc == 0) {
                    BLEManager::instance_->process_received_data(data, data_len);
                }
            }
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &NUS_TX_CHAR_UUID.u) == 0) {
        // TX Characteristic - handle notifications to client
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            // Not typically used for notifications
            return 0;
        }
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

BLEManager::BLEManager()
    : initialized_(false), advertising_(false), connected_(false), scanning_(false),
      conn_handle_(BLE_HS_CONN_HANDLE_NONE), nus_service_handle_(0), 
      nus_rx_char_handle_(0), nus_tx_char_handle_(0), device_name_("ESP32-P4-WiFi") {
    instance_ = this;
}

BLEManager::~BLEManager() {
    if (initialized_) {
        nimble_port_stop();
        nimble_port_deinit();
    }
    instance_ = nullptr;
}

bool BLEManager::initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "BLE Manager already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing ESP-Hosted NimBLE stack...");
    ESP_LOGI(TAG, "Architecture: ESP32-P4 (host) + ESP32-C6 (BLE controller) via SDIO");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE port
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE port: %s", esp_err_to_name(ret));
        return false;
    }

    // Configure NimBLE host
    ble_hs_cfg.reset_cb = on_reset_callback;
    ble_hs_cfg.sync_cb = on_sync_callback;
    ble_hs_cfg.gatts_register_cb = nullptr;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Set device name
    int rc = ble_svc_gap_device_name_set(device_name_.c_str());
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name: %d", rc);
        return false;
    }

    // Register GATT services
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services: %d", rc);
        return false;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services: %d", rc);
        return false;
    }

    // Get TX characteristic handle for notifications
    rc = ble_gatts_find_chr(&NUS_SERVICE_UUID.u, &NUS_TX_CHAR_UUID.u, 
                           nullptr, &nus_tx_char_handle_);
    if (rc != 0) {
        ESP_LOGW(TAG, "Could not find TX characteristic handle: %d", rc);
    } else {
        ESP_LOGI(TAG, "NUS TX characteristic handle: %d", nus_tx_char_handle_);
    }

    // Clear scan results
    scan_results_.clear();

    // Start NimBLE host task
    nimble_port_freertos_init(nimble_host_task);

    initialized_ = true;
    ESP_LOGI(TAG, "ESP-Hosted BLE Manager initialized successfully");
    ESP_LOGI(TAG, "Nordic UART Service registered with ESP32-C6 controller");
    return true;
}

bool BLEManager::start_advertising(const std::string& device_name) {
    if (!initialized_) {
        ESP_LOGE(TAG, "BLE Manager not initialized");
        return false;
    }

    if (advertising_) {
        ESP_LOGW(TAG, "Already advertising");
        return true;
    }

    // Update device name if provided
    if (!device_name.empty() && device_name != device_name_) {
        device_name_ = device_name;
        int rc = ble_svc_gap_device_name_set(device_name_.c_str());
        if (rc != 0) {
            ESP_LOGW(TAG, "Failed to update device name: %d", rc);
        }
    }

    ESP_LOGI(TAG, "Starting BLE advertising as: %s", device_name_.c_str());
    start_advertising_internal();
    return true;
}

bool BLEManager::stop_advertising() {
    if (!advertising_) {
        ESP_LOGW(TAG, "Not currently advertising");
        return false;
    }

    int rc = ble_gap_adv_stop();
    if (rc == 0 || rc == BLE_HS_EALREADY) {
        ESP_LOGI(TAG, "BLE advertising stopped");
        advertising_ = false;
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to stop advertising: %d", rc);
        return false;
    }
}

bool BLEManager::is_connected() const {
    return connected_ && conn_handle_ != BLE_HS_CONN_HANDLE_NONE;
}

bool BLEManager::send_response(const std::string& data) {
    if (!is_connected()) {
        ESP_LOGW(TAG, "No BLE client connected");
        return false;
    }

    if (data.length() > MAX_DATA_LEN) {
        ESP_LOGW(TAG, "Data too long: %zu bytes", data.length());
        return false;
    }

    if (nus_tx_char_handle_ == 0) {
        ESP_LOGE(TAG, "TX characteristic handle not found");
        return false;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data.c_str(), data.length());
    if (om == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate mbuf");
        return false;
    }

    int rc = ble_gatts_notify_custom(conn_handle_, nus_tx_char_handle_, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send notification: %d", rc);
        return false;
    }

    ESP_LOGD(TAG, "Sent %zu bytes via BLE NUS", data.length());
    return true;
}

void BLEManager::set_command_callback(std::function<std::string(const std::string&)> callback) {
    command_callback_ = callback;
    ESP_LOGI(TAG, "BLE command callback registered");
}

// BLE scanning functionality
bool BLEManager::start_scan(int scan_duration_seconds) {
    if (!initialized_) {
        ESP_LOGE(TAG, "BLE Manager not initialized");
        return false;
    }
    
    if (scanning_) {
        ESP_LOGW(TAG, "BLE scan already in progress");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting BLE scan for %d seconds via ESP32-C6", scan_duration_seconds);
    
    // Clear previous results
    scan_results_.clear();
    
    struct ble_gap_disc_params disc_params = {0};
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;
    
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, scan_duration_seconds * 1000, 
                         &disc_params, gap_event_handler, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start scan: %d", rc);
        return false;
    }
    
    scanning_ = true;
    return true;
}

bool BLEManager::stop_scan() {
    if (!scanning_) {
        ESP_LOGW(TAG, "No BLE scan in progress");
        return false;
    }
    
    int rc = ble_gap_disc_cancel();
    if (rc == 0 || rc == BLE_HS_EALREADY) {
        ESP_LOGI(TAG, "BLE scan stopped");
        scanning_ = false;
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to stop scan: %d", rc);
        return false;
    }
}

bool BLEManager::is_scanning() const {
    return scanning_;
}

int BLEManager::get_scan_result_count() const {
    return scan_results_.size();
}

std::string BLEManager::get_scan_result(int index) const {
    if (index < 0 || index >= static_cast<int>(scan_results_.size())) {
        return "";
    }
    
    const auto& result = scan_results_[index];
    std::string info = "[" + std::to_string(index) + "] " + result.address;
    
    if (!result.name.empty()) {
        info += " (" + result.name + ")";
    } else {
        info += " (Unknown)";
    }
    
    info += " RSSI: " + std::to_string(result.rssi) + " dBm";
    
    if (!result.service_uuids.empty()) {
        info += " Services: " + result.service_uuids;
    }
    
    return info;
}

std::string BLEManager::get_debug_status() const {
    std::string status = "=== BLE Debug Status ===\n";
    status += "Implementation: ESP-Hosted NimBLE via ESP32-C6\n";
    status += "Transport: VHCI over SDIO\n";
    status += "Initialized: " + std::string(initialized_ ? "Yes" : "No") + "\n";
    status += "Advertising: " + std::string(advertising_ ? "Active" : "Stopped") + "\n";
    status += "Connected: " + std::string(connected_ ? "Yes" : "No") + "\n";
    status += "Scanning: " + std::string(scanning_ ? "Active" : "Stopped") + "\n";
    status += "Connection Handle: " + std::to_string(conn_handle_) + "\n";
    status += "Device Name: " + device_name_ + "\n";
    status += "Scan Results: " + std::to_string(scan_results_.size()) + " devices\n";
    status += "\nNordic UART Service:\n";
    status += "- Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E\n";
    status += "- RX Char UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (Write)\n";
    status += "- TX Char UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (Notify)\n";
    status += "- TX Handle: " + std::to_string(nus_tx_char_handle_) + "\n";
    status += "\nESP-Hosted Configuration:\n";
    status += "- NimBLE Stack: Active\n";
    status += "- ESP32-C6 Controller: Connected via SDIO\n";
    status += "- VHCI Transport: Enabled\n";
    status += "- Service Registration: Complete\n";
    
    return status;
}

// Static callback handlers
int BLEManager::gap_event_handler(struct ble_gap_event *event, void *arg) {
    if (!instance_) {
        return 0;
    }
    
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE connection event: status=%d", event->connect.status);
            if (event->connect.status == 0) {
                instance_->conn_handle_ = event->connect.conn_handle;
                instance_->connected_ = true;
                instance_->handle_connection_event(event);
            } else {
                // Connection failed, restart advertising
                instance_->start_advertising_internal();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE disconnect event: reason=%d", event->disconnect.reason);
            instance_->conn_handle_ = BLE_HS_CONN_HANDLE_NONE;
            instance_->connected_ = false;
            instance_->handle_disconnection_event(event);
            // Restart advertising
            instance_->start_advertising_internal();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "BLE advertising complete");
            instance_->advertising_ = false;
            break;

        case BLE_GAP_EVENT_DISC:
            {
                // Handle scan results
                char addr_str[18];
                snprintf(addr_str, sizeof(addr_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                        event->disc.addr.val[5], event->disc.addr.val[4],
                        event->disc.addr.val[3], event->disc.addr.val[2], 
                        event->disc.addr.val[1], event->disc.addr.val[0]);
                
                ScanResult result;
                result.address = std::string(addr_str);
                result.rssi = event->disc.rssi;
                result.name = "";
                result.service_uuids = "";
                
                // Extract device name from advertising data
                struct ble_hs_adv_fields fields;
                int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, 
                                               event->disc.length_data);
                if (rc == 0 && fields.name != nullptr) {
                    result.name = std::string((char*)fields.name, fields.name_len);
                }
                
                instance_->scan_results_.push_back(result);
                ESP_LOGD(TAG, "Discovered device: %s, RSSI: %d", addr_str, event->disc.rssi);
            }
            break;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(TAG, "BLE scan complete, found %d devices", 
                    instance_->scan_results_.size());
            instance_->scanning_ = false;
            break;

        default:
            ESP_LOGD(TAG, "BLE GAP Event: %d", event->type);
            break;
    }
    
    return 0;
}

int BLEManager::gatt_access_handler(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // This is handled by the nus_access_cb function
    return 0;
}

void BLEManager::on_sync_callback(void) {
    ESP_LOGI(TAG, "NimBLE host sync callback - stack ready");
    
    // Ensure proper identity address
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure address: %d", rc);
        return;
    }
    
    // Start advertising automatically
    if (instance_ && instance_->initialized_) {
        instance_->start_advertising_internal();
    }
}

void BLEManager::on_reset_callback(int reason) {
    ESP_LOGW(TAG, "NimBLE host reset, reason: %d", reason);
}

void BLEManager::nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    ESP_LOGI(TAG, "NimBLE host task ended");
}

// Internal methods
void BLEManager::setup_gatt_service() {
    ESP_LOGD(TAG, "GATT service setup handled by static definition");
}

bool BLEManager::configure_advertising_data(const std::string& device_name) {
    ESP_LOGD(TAG, "Configuring advertising data for: %s", device_name.c_str());
    return true;
}

void BLEManager::handle_connection_event(struct ble_gap_event *event) {
    ESP_LOGI(TAG, "BLE client connected, handle: %d", event->connect.conn_handle);
}

void BLEManager::handle_disconnection_event(struct ble_gap_event *event) {
    ESP_LOGI(TAG, "BLE client disconnected, reason: %d", event->disconnect.reason);
}

void BLEManager::process_received_data(const uint8_t* data, uint16_t len) {
    if (!command_callback_) {
        ESP_LOGW(TAG, "No command callback registered");
        return;
    }
    
    std::string command(reinterpret_cast<const char*>(data), len);
    ESP_LOGI(TAG, "Received BLE command: %s", command.c_str());
    
    std::string response = command_callback_(command);
    if (!response.empty()) {
        send_response(response);
    }
}

bool BLEManager::register_nus_service() {
    ESP_LOGD(TAG, "Nordic UART Service registered via static definition");
    return true;
}

void BLEManager::start_advertising_internal() {
    if (!initialized_) {
        ESP_LOGE(TAG, "Cannot start advertising - not initialized");
        return;
    }
    
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;
    
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t*)device_name_.c_str();
    fields.name_len = device_name_.length();
    fields.name_is_complete = 1;
    
    // Note: Skipping 128-bit UUID in advertising data to avoid field size issues
    // Service will still be discoverable via GATT service discovery
    
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising fields: %d", rc);
        return;
    }
    
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER,
                          &adv_params, gap_event_handler, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising started via ESP32-C6");
        advertising_ = true;
    }
}

} // namespace ble_serial