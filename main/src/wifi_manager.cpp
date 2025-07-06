#include "wifi_manager.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_wifi_remote.h"
#include "esp_mac.h"
#include "esp_hosted.h"
#include "nvs_flash.h"
#include <cstring>
#include <algorithm>

namespace wifi_config {

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager() 
    : initialized_(false), connected_(false), retry_count_(0) {
    wifi_event_group_ = xEventGroupCreate();
}

WiFiManager::~WiFiManager() {
    if (wifi_event_group_) {
        vEventGroupDelete(wifi_event_group_);
    }
}

bool WiFiManager::initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "WiFiManager already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing WiFi Manager");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Initialize ESP-Hosted
    ret = esp_hosted_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-Hosted: %s", esp_err_to_name(ret));
        return false;
    }
    
    setup_wifi_stack();
    register_event_handlers();
    
    // Initialize WiFi with default config (will be remapped to remote via esp_wifi_remote)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Set WiFi mode and start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    initialized_ = true;
    ESP_LOGI(TAG, "WiFi Manager initialized successfully");
    return true;
}

void WiFiManager::setup_wifi_stack() {
    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi station interface
    esp_netif_create_default_wifi_sta();
}

void WiFiManager::register_event_handlers() {
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                             &WiFiManager::event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                             &WiFiManager::event_handler, this));
}

void WiFiManager::event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started");
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                manager->connected_ = false;
                manager->connected_ssid_.clear();
                if (manager->retry_count_ < MAX_RETRY_COUNT) {
                    esp_wifi_connect();
                    manager->retry_count_++;
                    ESP_LOGI(TAG, "Retry to connect to the AP (attempt %d/%d)", 
                            manager->retry_count_, MAX_RETRY_COUNT);
                } else {
                    ESP_LOGI(TAG, "Connect to the AP failed");
                    xEventGroupSetBits(manager->wifi_event_group_, WIFI_FAIL_BIT);
                }
                break;
                
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "WiFi scan completed");
                
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count);
                
                if (ap_count == 0) {
                    ESP_LOGI(TAG, "No WiFi networks found");
                    manager->scanned_networks_.clear();
                    xEventGroupSetBits(manager->wifi_event_group_, WIFI_SCAN_DONE_BIT);
                    return;
                }
                
                auto ap_info = std::make_unique<wifi_ap_record_t[]>(ap_count);
                esp_wifi_scan_get_ap_records(&ap_count, ap_info.get());
                
                manager->scanned_networks_.clear();
                manager->scanned_networks_.reserve(std::min(static_cast<int>(ap_count), MAX_NETWORKS));
                
                for (int i = 0; i < std::min(static_cast<int>(ap_count), MAX_NETWORKS); i++) {
                    std::string ssid(reinterpret_cast<const char*>(ap_info[i].ssid));
                    if (!ssid.empty()) {  // Skip empty SSIDs
                        manager->scanned_networks_.emplace_back(
                            ssid, ap_info[i].rssi, ap_info[i].authmode);
                    }
                }
                
                // Sort networks by signal strength (strongest first)
                std::sort(manager->scanned_networks_.begin(), manager->scanned_networks_.end(),
                         [](const NetworkInfo& a, const NetworkInfo& b) {
                             return a.rssi > b.rssi;
                         });
                
                ESP_LOGI(TAG, "Found %d WiFi networks", static_cast<int>(manager->scanned_networks_.size()));
                xEventGroupSetBits(manager->wifi_event_group_, WIFI_SCAN_DONE_BIT);
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        manager->connected_ = true;
        manager->retry_count_ = 0;
        
        // SSID will be set during connection process
        // connected_ssid_ is already set in connect_to_network()
        
        xEventGroupSetBits(manager->wifi_event_group_, WIFI_CONNECTED_BIT);
    }
}

bool WiFiManager::scan_networks() {
    if (!initialized_) {
        ESP_LOGE(TAG, "WiFiManager not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting WiFi scan");
    
    // Clear previous scan results
    scanned_networks_.clear();
    
    // Clear event bits
    xEventGroupClearBits(wifi_event_group_, WIFI_SCAN_DONE_BIT);
    
    esp_err_t ret = esp_wifi_scan_start(nullptr, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Wait for scan to complete
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group_, WIFI_SCAN_DONE_BIT,
                                          pdFALSE, pdFALSE, 
                                          pdMS_TO_TICKS(10000)); // 10 second timeout
    
    if (!(bits & WIFI_SCAN_DONE_BIT)) {
        ESP_LOGE(TAG, "WiFi scan timeout");
        return false;
    }
    
    return true;
}

bool WiFiManager::connect_to_network(const std::string& ssid, const std::string& password) {
    if (!initialized_) {
        ESP_LOGE(TAG, "WiFiManager not initialized");
        return false;
    }
    
    if (ssid.empty()) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return false;
    }
    
    ESP_LOGI(TAG, "Connecting to network: %s", ssid.c_str());
    
    wifi_config_t wifi_config = {};
    
    // Copy SSID
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), ssid.c_str(), 
            sizeof(wifi_config.sta.ssid) - 1);
    
    // Copy password
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password), password.c_str(), 
            sizeof(wifi_config.sta.password) - 1);
    
    // Set security configuration
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    // Reset retry count and store SSID for later use
    retry_count_ = 0;
    connected_ssid_ = ssid;
    
    // Clear event bits
    xEventGroupClearBits(wifi_event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    // Wait for connection result
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group_,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE, pdFALSE, 
                                          pdMS_TO_TICKS(30000)); // 30 second timeout
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected successfully to %s", ssid.c_str());
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to %s", ssid.c_str());
        return false;
    } else {
        ESP_LOGE(TAG, "Connection timeout for %s", ssid.c_str());
        return false;
    }
}

bool WiFiManager::disconnect() {
    if (!initialized_) {
        ESP_LOGE(TAG, "WiFiManager not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Disconnecting from WiFi");
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
        return false;
    }
    
    connected_ = false;
    connected_ssid_.clear();
    return true;
}

const std::vector<NetworkInfo>& WiFiManager::get_scanned_networks() const {
    return scanned_networks_;
}

bool WiFiManager::is_connected() const {
    return connected_;
}

std::string WiFiManager::get_connected_ssid() const {
    return connected_ssid_;
}

std::string WiFiManager::get_ip_address() const {
    if (!connected_) {
        return "";
    }
    
    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        return std::string(ip_str);
    }
    
    return "";
}

int8_t WiFiManager::get_rssi() const {
    if (!connected_) {
        return 0;
    }
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    
    return 0;
}

const char* WiFiManager::auth_mode_to_string(wifi_auth_mode_t auth_mode) const {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
}

} // namespace wifi_config