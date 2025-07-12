#include "relay_manager.hpp"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sstream>

namespace relay_control {

static const char* TAG = "RelayManager";

RelayManager::RelayManager()
    : initialized_(false), relay_1_state_(RelayState::OFF), relay_2_state_(RelayState::OFF),
      relay_1_switch_count_(0), relay_2_switch_count_(0), total_operations_(0) {
}

RelayManager::~RelayManager() {
    if (initialized_) {
        // Safety: Turn off all relays before destruction
        turn_off_all();
    }
}

bool RelayManager::initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "Relay Manager already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing Relay Manager for dual relay board variant");
    ESP_LOGI(TAG, "Relay 1: GPIO%d, Relay 2: GPIO%d", RELAY_1_GPIO, RELAY_2_GPIO);

    // Configure GPIO pins for relay control
    if (!configure_gpio_pin(RELAY_1_GPIO)) {
        ESP_LOGE(TAG, "Failed to configure Relay 1 GPIO%d", RELAY_1_GPIO);
        return false;
    }

    if (!configure_gpio_pin(RELAY_2_GPIO)) {
        ESP_LOGE(TAG, "Failed to configure Relay 2 GPIO%d", RELAY_2_GPIO);
        return false;
    }

    // Initialize relays to OFF state for safety
    if (!set_gpio_state(RELAY_1_GPIO, 0) || !set_gpio_state(RELAY_2_GPIO, 0)) {
        ESP_LOGE(TAG, "Failed to initialize relays to OFF state");
        return false;
    }

    // Initialize state tracking
    relay_1_state_ = RelayState::OFF;
    relay_2_state_ = RelayState::OFF;
    relay_1_switch_count_ = 0;
    relay_2_switch_count_ = 0;
    total_operations_ = 0;

    initialized_ = true;
    ESP_LOGI(TAG, "Relay Manager initialized successfully");
    ESP_LOGI(TAG, "Both relays initialized to OFF state for safety");
    
    return true;
}

bool RelayManager::configure_gpio_pin(gpio_num_t gpio_pin) {
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", gpio_pin, esp_err_to_name(ret));
        return false;
    }

    ESP_LOGD(TAG, "Configured GPIO%d for relay control", gpio_pin);
    return true;
}

bool RelayManager::set_gpio_state(gpio_num_t gpio_pin, int state) {
    esp_err_t ret = gpio_set_level(gpio_pin, state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO%d to %d: %s", gpio_pin, state, esp_err_to_name(ret));
        return false;
    }

    ESP_LOGD(TAG, "Set GPIO%d to %s", gpio_pin, state ? "HIGH" : "LOW");
    return true;
}

int RelayManager::get_gpio_state(gpio_num_t gpio_pin) const {
    return gpio_get_level(gpio_pin);
}

gpio_num_t RelayManager::relay_id_to_gpio(RelayId relay_id) const {
    switch (relay_id) {
        case RelayId::RELAY_1:
            return RELAY_1_GPIO;
        case RelayId::RELAY_2:
            return RELAY_2_GPIO;
        default:
            return GPIO_NUM_NC; // Invalid
    }
}

const char* RelayManager::relay_id_to_string(RelayId relay_id) const {
    switch (relay_id) {
        case RelayId::RELAY_1: return "Relay 1";
        case RelayId::RELAY_2: return "Relay 2";
        case RelayId::ALL_RELAYS: return "All Relays";
        default: return "Unknown";
    }
}

const char* RelayManager::relay_state_to_string(RelayState state) const {
    return (state == RelayState::ON) ? "ON" : "OFF";
}

bool RelayManager::set_relay_state(RelayId relay_id, RelayState state) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Relay Manager not initialized");
        return false;
    }

    total_operations_++;

    if (relay_id == RelayId::ALL_RELAYS) {
        // Control both relays
        bool success = true;
        success &= set_relay_state(RelayId::RELAY_1, state);
        success &= set_relay_state(RelayId::RELAY_2, state);
        
        if (success) {
            ESP_LOGI(TAG, "Set all relays to %s", relay_state_to_string(state));
        }
        return success;
    }

    gpio_num_t gpio_pin = relay_id_to_gpio(relay_id);
    if (gpio_pin == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "Invalid relay ID");
        return false;
    }

    int gpio_level = (state == RelayState::ON) ? 1 : 0;
    if (!set_gpio_state(gpio_pin, gpio_level)) {
        return false;
    }

    // Update state tracking
    if (relay_id == RelayId::RELAY_1) {
        if (relay_1_state_ != state) {
            relay_1_switch_count_++;
        }
        relay_1_state_ = state;
    } else if (relay_id == RelayId::RELAY_2) {
        if (relay_2_state_ != state) {
            relay_2_switch_count_++;
        }
        relay_2_state_ = state;
    }

    ESP_LOGI(TAG, "Set %s to %s (GPIO%d)", 
             relay_id_to_string(relay_id), relay_state_to_string(state), gpio_pin);
    
    return true;
}

RelayManager::RelayState RelayManager::get_relay_state(RelayId relay_id) const {
    if (!initialized_) {
        return RelayState::OFF;
    }

    switch (relay_id) {
        case RelayId::RELAY_1:
            return relay_1_state_;
        case RelayId::RELAY_2:
            return relay_2_state_;
        default:
            return RelayState::OFF;
    }
}

bool RelayManager::turn_on(RelayId relay_id) {
    return set_relay_state(relay_id, RelayState::ON);
}

bool RelayManager::turn_off(RelayId relay_id) {
    return set_relay_state(relay_id, RelayState::OFF);
}

bool RelayManager::toggle(RelayId relay_id) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Relay Manager not initialized");
        return false;
    }

    if (relay_id == RelayId::ALL_RELAYS) {
        // Toggle both relays
        bool success = true;
        success &= toggle(RelayId::RELAY_1);
        success &= toggle(RelayId::RELAY_2);
        return success;
    }

    RelayState current_state = get_relay_state(relay_id);
    RelayState new_state = (current_state == RelayState::ON) ? RelayState::OFF : RelayState::ON;
    
    return set_relay_state(relay_id, new_state);
}

bool RelayManager::turn_off_all() {
    return set_relay_state(RelayId::ALL_RELAYS, RelayState::OFF);
}

std::string RelayManager::get_status() const {
    if (!initialized_) {
        return "Relay Manager: Not initialized";
    }

    std::string status = "=== Relay Status ===\n";
    status += "Board Variant: Dual Relay Board\n";
    status += "Relay 1 (GPIO32): " + std::string(relay_state_to_string(relay_1_state_)) + "\n";
    status += "Relay 2 (GPIO46): " + std::string(relay_state_to_string(relay_2_state_)) + "\n";
    
    return status;
}

std::string RelayManager::get_debug_status() const {
    std::string status = "=== Relay Debug Status ===\n";
    status += "Board Variant: ESP32-P4 Dual Relay Board\n";
    status += "Initialized: " + std::string(initialized_ ? "Yes" : "No") + "\n";
    
    if (initialized_) {
        status += "\nRelay Configuration:\n";
        status += "- Relay 1: GPIO" + std::to_string(RELAY_1_GPIO) + " = " + 
                  std::string(relay_state_to_string(relay_1_state_)) + "\n";
        status += "- Relay 2: GPIO" + std::to_string(RELAY_2_GPIO) + " = " + 
                  std::string(relay_state_to_string(relay_2_state_)) + "\n";
        
        status += "\nGPIO Pin States:\n";
        status += "- GPIO" + std::to_string(RELAY_1_GPIO) + " Level: " + 
                  std::to_string(get_gpio_state(RELAY_1_GPIO)) + "\n";
        status += "- GPIO" + std::to_string(RELAY_2_GPIO) + " Level: " + 
                  std::to_string(get_gpio_state(RELAY_2_GPIO)) + "\n";
        
        status += "\nOperation Statistics:\n";
        status += "- Relay 1 Switches: " + std::to_string(relay_1_switch_count_) + "\n";
        status += "- Relay 2 Switches: " + std::to_string(relay_2_switch_count_) + "\n";
        status += "- Total Operations: " + std::to_string(total_operations_) + "\n";
        
        status += "\nSafety Features:\n";
        status += "- Auto-off on destruction: Enabled\n";
        status += "- Initialization to OFF: Enabled\n";
        status += "- Error logging: Enabled\n";
    }
    
    return status;
}

bool RelayManager::is_initialized() const {
    return initialized_;
}

} // namespace relay_control