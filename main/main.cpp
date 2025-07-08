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
    
    // Create BLE manager for wireless command access
    auto ble_manager = std::make_shared<ble_serial::BLEManager>();
    if (!ble_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize BLE Manager");
        ESP_LOGE(TAG, "BLE functionality will not be available");
        return;
    }
    
    // Create command interpreter
    auto command_interpreter = std::make_unique<command_interface::CommandInterpreter>(wifi_manager);
    if (!command_interpreter->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize Command Interpreter");
        return;
    }
    
    // Connect BLE manager to command interpreter
    command_interpreter->set_ble_manager(ble_manager);
    
    // Connect BLE to command interpreter for wireless access
    ble_manager->set_command_callback([&command_interpreter](const std::string& command) -> std::string {
        return command_interpreter->process_command_with_response(command);
    });
    
    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "WiFi + BLE commands available via USB Serial JTAG");
    ESP_LOGI(TAG, "BLE commands: ble_start, ble_stop, ble_status, ble_name, ble_scan, ble_debug");
    ESP_LOGI(TAG, "BLE implementation: ESP-Hosted NimBLE with Nordic UART Service");
    ESP_LOGI(TAG, "Architecture: ESP32-P4 (host) + ESP32-C6 (controller) via VHCI/SDIO");
    
    // Start interactive command mode (USB Serial JTAG)
    command_interpreter->start_interactive_mode();
}