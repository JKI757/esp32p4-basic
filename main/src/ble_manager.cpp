/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_manager.hpp"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <cstring>

namespace ble_serial {

static const char* TAG = "BLEManager";

// Static instance for C callbacks
BLEManager* BLEManager::instance_ = nullptr;

// GATT service definition
static struct ble_gatt_svc_def gatt_service_def[];

BLEManager::BLEManager()
    : initialized_(false), advertising_(false), connected_(false),
      conn_handle_(BLE_HS_CONN_HANDLE_NONE), tx_char_handle_(0) {
    instance_ = this;
}

BLEManager::~BLEManager() {
    if (advertising_) {
        stop_advertising();
    }
    instance_ = nullptr;
}

bool BLEManager::initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "BLE Manager already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing BLE Manager with ESP-Hosted");

    // Initialize NimBLE port
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE port: %s", esp_err_to_name(ret));
        return false;
    }

    // Initialize BLE host stack
    ble_hs_cfg.reset_cb = [](int reason) {
        ESP_LOGI(TAG, "BLE host reset, reason: %d", reason);
    };

    ble_hs_cfg.sync_cb = []() {
        ESP_LOGI(TAG, "BLE host synced");
        // Start advertising when host is ready
        if (instance_) {
            instance_->start_advertising();
        }
    };

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Set device name
    ret = ble_svc_gap_device_name_set("ESP32-P4-WiFi");
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to set device name: %d", ret);
        return false;
    }

    // Set up GATT service
    setup_gatt_service();

    // Start NimBLE task
    nimble_port_freertos_init([](void *param) {
        ESP_LOGI(TAG, "BLE Host Task Started");
        nimble_port_run();
        ESP_LOGI(TAG, "BLE Host Task Ended");
        nimble_port_freertos_deinit();
    });

    initialized_ = true;
    ESP_LOGI(TAG, "BLE Manager initialized successfully");
    return true;
}

void BLEManager::setup_gatt_service() {
    // GATT service definition with Nordic UART Service
    static struct ble_gatt_chr_def characteristics[] = {
        {
            // RX Characteristic (write from client)
            .uuid = reinterpret_cast<const ble_uuid_t*>(&UART_RX_UUID),
            .access_cb = gatt_access_handler,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        },
        {
            // TX Characteristic (notify to client)
            .uuid = reinterpret_cast<const ble_uuid_t*>(&UART_TX_UUID),
            .access_cb = gatt_access_handler,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &tx_char_handle_,
        },
        {
            0, // End of characteristics
        }
    };

    static struct ble_gatt_svc_def service_def[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = reinterpret_cast<const ble_uuid_t*>(&UART_SERVICE_UUID),
            .characteristics = characteristics,
        },
        {
            0, // End of services
        }
    };

    int rc = ble_gatts_count_cfg(service_def);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT configuration: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(service_def);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT service: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Nordic UART Service configured");
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

    // Set up advertising data
    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Configure advertising data
    if (!configure_advertising_data(device_name)) {
        return false;
    }

    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                               &adv_params, gap_event_handler, this);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
        return false;
    }

    advertising_ = true;
    ESP_LOGI(TAG, "BLE advertising started as '%s'", device_name.c_str());
    return true;
}

bool BLEManager::configure_advertising_data(const std::string& device_name) {
    struct ble_hs_adv_fields fields = {};

    // Set flags
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Set device name
    fields.name = reinterpret_cast<const uint8_t*>(device_name.c_str());
    fields.name_len = device_name.length();
    fields.name_is_complete = 1;

    // Set service UUID
    fields.uuids128 = &UART_SERVICE_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising data: %d", rc);
        return false;
    }

    return true;
}

bool BLEManager::stop_advertising() {
    if (!advertising_) {
        return true;
    }

    int rc = ble_gap_adv_stop();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to stop advertising: %d", rc);
        return false;
    }

    advertising_ = false;
    ESP_LOGI(TAG, "BLE advertising stopped");
    return true;
}

bool BLEManager::is_connected() const {
    return connected_;
}

bool BLEManager::send_response(const std::string& data) {
    if (!connected_) {
        ESP_LOGW(TAG, "No BLE client connected");
        return false;
    }

    if (data.empty()) {
        return true;
    }

    // Split data into chunks if needed (BLE MTU limitations)
    size_t offset = 0;
    while (offset < data.length()) {
        size_t chunk_size = std::min(data.length() - offset, static_cast<size_t>(244)); // Conservative MTU
        
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data.c_str() + offset, chunk_size);
        if (om == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate mbuf for notification");
            return false;
        }

        int rc = ble_gatts_notify_custom(conn_handle_, tx_char_handle_, om);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to send notification: %d", rc);
            return false;
        }

        offset += chunk_size;
    }

    return true;
}

void BLEManager::set_command_callback(std::function<std::string(const std::string&)> callback) {
    command_callback_ = callback;
}

void BLEManager::process_received_data(const uint8_t* data, uint16_t len) {
    if (!command_callback_) {
        ESP_LOGW(TAG, "No command callback set");
        return;
    }

    std::string command(reinterpret_cast<const char*>(data), len);
    ESP_LOGI(TAG, "Received BLE command: %s", command.c_str());

    // Process command and get response
    std::string response = command_callback_(command);
    
    // Send response back to client
    if (!response.empty()) {
        send_response(response);
    }
}

// Static callback handlers
int BLEManager::gap_event_handler(struct ble_gap_event *event, void *arg) {
    BLEManager* manager = static_cast<BLEManager*>(arg);
    if (!manager) {
        return 0;
    }

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        manager->handle_connection_event(event);
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        manager->handle_disconnection_event(event);
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete, restarting");
        manager->advertising_ = false;
        manager->start_advertising();
        break;

    default:
        break;
    }

    return 0;
}

int BLEManager::gatt_access_handler(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (!instance_) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    const ble_uuid_t *uuid = ctxt->chr->uuid;

    // Check if this is the RX characteristic (write from client)
    if (ble_uuid_cmp(uuid, reinterpret_cast<const ble_uuid_t*>(&UART_RX_UUID)) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            struct os_mbuf *om = ctxt->om;
            if (om) {
                uint16_t len = OS_MBUF_PKTLEN(om);
                if (len > 0 && len <= MAX_DATA_LEN) {
                    uint8_t data[MAX_DATA_LEN];
                    int rc = ble_hs_mbuf_to_flat(om, data, len, nullptr);
                    if (rc == 0) {
                        instance_->process_received_data(data, len);
                    }
                }
            }
            return 0;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

void BLEManager::handle_connection_event(struct ble_gap_event *event) {
    if (event->connect.status == 0) {
        connected_ = true;
        conn_handle_ = event->connect.conn_handle;
        ESP_LOGI(TAG, "BLE client connected, handle: %d", conn_handle_);
        
        // Stop advertising when connected
        stop_advertising();
    } else {
        ESP_LOGE(TAG, "BLE connection failed, status: %d", event->connect.status);
        start_advertising(); // Restart advertising on failed connection
    }
}

void BLEManager::handle_disconnection_event(struct ble_gap_event *event) {
    connected_ = false;
    conn_handle_ = BLE_HS_CONN_HANDLE_NONE;
    ESP_LOGI(TAG, "BLE client disconnected, reason: %d", event->disconnect.reason);
    
    // Restart advertising after disconnection
    start_advertising();
}

} // namespace ble_serial