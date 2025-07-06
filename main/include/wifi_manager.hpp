#pragma once

#include <string>
#include <vector>
#include <memory>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_wifi_remote.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace wifi_config {

struct NetworkInfo {
    std::string ssid;
    int8_t rssi;
    wifi_auth_mode_t auth_mode;
    
    NetworkInfo(const std::string& ssid, int8_t rssi, wifi_auth_mode_t auth_mode)
        : ssid(ssid), rssi(rssi), auth_mode(auth_mode) {}
};

class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager();
    
    // Core initialization
    bool initialize();
    
    // WiFi operations
    bool scan_networks();
    bool connect_to_network(const std::string& ssid, const std::string& password);
    bool disconnect();
    
    // Network information
    const std::vector<NetworkInfo>& get_scanned_networks() const;
    bool is_connected() const;
    std::string get_connected_ssid() const;
    std::string get_ip_address() const;
    int8_t get_rssi() const;
    
    // Static callback for ESP-IDF event system
    static void event_handler(void* arg, esp_event_base_t event_base, 
                             int32_t event_id, void* event_data);
    
private:
    void setup_wifi_stack();
    void register_event_handlers();
    const char* auth_mode_to_string(wifi_auth_mode_t auth_mode) const;
    
    // Member variables
    std::vector<NetworkInfo> scanned_networks_;
    EventGroupHandle_t wifi_event_group_;
    bool initialized_;
    bool connected_;
    std::string connected_ssid_;
    int retry_count_;
    
    // Event bits
    static const int WIFI_CONNECTED_BIT = BIT0;
    static const int WIFI_FAIL_BIT = BIT1;
    static const int WIFI_SCAN_DONE_BIT = BIT2;
    
    // Constants
    static const int MAX_RETRY_COUNT = 5;
    static const int MAX_NETWORKS = 20;
};

} // namespace wifi_config