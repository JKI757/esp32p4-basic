#include <memory>
#include "esp_log.h"
#include "wifi_manager.hpp"
#include "command_interpreter.hpp"

static const char* TAG = "main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-P4 WiFi Configuration Tool");
    
    // Create WiFi manager
    auto wifi_manager = std::make_shared<wifi_config::WiFiManager>();
    if (!wifi_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WiFi Manager");
        return;
    }
    
    // Create command interpreter
    auto command_interpreter = std::make_unique<command_interface::CommandInterpreter>(wifi_manager);
    if (!command_interpreter->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize Command Interpreter");
        return;
    }
    
    ESP_LOGI(TAG, "System initialized successfully");
    
    // Start interactive command mode
    command_interpreter->start_interactive_mode();
}