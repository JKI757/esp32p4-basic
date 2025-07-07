#include <memory>
#include "esp_log.h"
#include "wifi_manager.hpp"
#include "command_interpreter.hpp"
#include "ble_manager.hpp"

static const char* TAG = "main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-P4 Foundational Firmware");
    
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
    
    // Create BLE manager for wireless command access
    auto ble_manager = std::make_unique<ble_serial::BLEManager>();
    if (!ble_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize BLE Manager");
        return;
    }
    
    // Connect BLE to command interpreter
    ble_manager->set_command_callback([&command_interpreter](const std::string& command) -> std::string {
        return command_interpreter->process_command_with_response(command);
    });
    
    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "WiFi commands available via USB Serial JTAG and BLE");
    ESP_LOGI(TAG, "BLE Device Name: ESP32-P4-WiFi");
    
    // Start interactive command mode (USB Serial JTAG)
    command_interpreter->start_interactive_mode();
}