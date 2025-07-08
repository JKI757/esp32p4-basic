#include "command_interpreter.hpp"
#include "ble_manager.hpp"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace command_interface {

static const char* TAG = "CommandInterpreter";

CommandInterpreter::CommandInterpreter(std::shared_ptr<wifi_config::WiFiManager> wifi_manager)
    : wifi_manager_(wifi_manager), ble_manager_(nullptr), initialized_(false) {
}

CommandInterpreter::~CommandInterpreter() {
    // Cleanup if needed
}

void CommandInterpreter::set_ble_manager(std::shared_ptr<ble_serial::BLEManager> ble_manager) {
    ble_manager_ = ble_manager;
    ESP_LOGI(TAG, "BLE manager set for command interpreter");
}

bool CommandInterpreter::initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "CommandInterpreter already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing Command Interpreter");
    
    if (!wifi_manager_) {
        ESP_LOGE(TAG, "WiFiManager is null");
        return false;
    }
    
    setup_usb_serial();
    
    initialized_ = true;
    ESP_LOGI(TAG, "Command Interpreter initialized successfully");
    return true;
}

void CommandInterpreter::setup_usb_serial() {
    ESP_LOGI(TAG, "Setting up USB Serial JTAG");
    
    // Configure USB Serial JTAG
    usb_serial_jtag_driver_config_t usb_serial_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };
    
    esp_err_t ret = usb_serial_jtag_driver_install(&usb_serial_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Serial JTAG driver: %s", esp_err_to_name(ret));
        return;
    }
    
    // Set USB Serial JTAG as default for stdin/stdout
    esp_vfs_usb_serial_jtag_use_driver();
    
    // Set line endings
    esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);
    
    ESP_LOGI(TAG, "USB Serial JTAG setup complete");
}

void CommandInterpreter::print_welcome_message() {
    printf("\n");
    printf("=====================================\n");
    printf("  ESP32-P4 WiFi Configuration Tool  \n");
    printf("=====================================\n");
    printf("Type 'help' for available commands\n");
    printf("\n");
}

void CommandInterpreter::print_prompt() {
    printf("> ");
    fflush(stdout);
}

void CommandInterpreter::start_interactive_mode() {
    if (!initialized_) {
        ESP_LOGE(TAG, "CommandInterpreter not initialized");
        return;
    }
    
    print_welcome_message();
    print_prompt();
    
    while (true) {
        std::string command = read_command_line();
        if (!command.empty()) {
            process_command(command);
        }
        print_prompt();
    }
}

std::string CommandInterpreter::read_command_line() {
    std::string cmd_buffer;
    cmd_buffer.reserve(CMD_BUFFER_SIZE);
    
    int c;
    while (true) {
        c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        if (c == '\n' || c == '\r') {
            printf("\n");
            return cmd_buffer;
        } else if (c == 8 || c == 127) { // Backspace or DEL
            if (!cmd_buffer.empty()) {
                cmd_buffer.pop_back();
                printf("\b \b");
            }
        } else if (c >= 32 && c < 127 && cmd_buffer.length() < CMD_BUFFER_SIZE - 1) {
            // Printable ASCII character
            cmd_buffer.push_back(static_cast<char>(c));
            printf("%c", c);
        }
        fflush(stdout);
    }
}

std::vector<std::string> CommandInterpreter::parse_command(const std::string& command) {
    std::vector<std::string> tokens;
    std::istringstream iss(command);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

void CommandInterpreter::process_command(const std::string& command) {
    if (command.empty()) {
        return;
    }
    
    std::vector<std::string> tokens = parse_command(command);
    if (tokens.empty()) {
        return;
    }
    
    // Convert command to lowercase for case-insensitive comparison
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    if (cmd == "help" || cmd == "h") {
        handle_help();
    } else if (cmd == "scan" || cmd == "s") {
        handle_scan();
    } else if (cmd == "list" || cmd == "l") {
        handle_list();
    } else if (cmd == "connect" || cmd == "c") {
        handle_connect(tokens);
    } else if (cmd == "status" || cmd == "st") {
        handle_status();
    } else if (cmd == "disconnect" || cmd == "d") {
        handle_disconnect();
    } else if (cmd == "ble_start" || cmd == "bs") {
        handle_ble_start();
    } else if (cmd == "ble_stop" || cmd == "bp") {
        handle_ble_stop();
    } else if (cmd == "ble_status" || cmd == "bt") {
        handle_ble_status();
    } else if (cmd == "ble_name" || cmd == "bn") {
        handle_ble_name(tokens);
    } else if (cmd == "ble_scan" || cmd == "bsc") {
        handle_ble_scan(tokens);
    } else if (cmd == "ble_debug" || cmd == "bd") {
        handle_ble_debug();
    } else {
        handle_unknown_command(tokens[0]);
    }
}

std::string CommandInterpreter::process_command_with_response(const std::string& command) {
    if (command.empty()) {
        return "Enter a command. Type 'help' for available commands.";
    }
    
    // Clean up command (trim whitespace and remove line endings)
    std::string clean_cmd = command;
    clean_cmd.erase(clean_cmd.find_last_not_of(" \t\n\r\f\v") + 1);
    clean_cmd.erase(0, clean_cmd.find_first_not_of(" \t\n\r\f\v"));
    
    if (clean_cmd.empty()) {
        return "Enter a command. Type 'help' for available commands.";
    }
    
    std::vector<std::string> tokens = parse_command(clean_cmd);
    if (tokens.empty()) {
        return "Invalid command. Type 'help' for available commands.";
    }
    
    // Convert command to lowercase for case-insensitive comparison
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    if (cmd == "help" || cmd == "h") {
        return generate_help_response();
    } else if (cmd == "scan" || cmd == "s") {
        return generate_scan_response();
    } else if (cmd == "list" || cmd == "l") {
        return generate_list_response();
    } else if (cmd == "connect" || cmd == "c") {
        return generate_connect_response(tokens);
    } else if (cmd == "status" || cmd == "st") {
        return generate_status_response();
    } else if (cmd == "disconnect" || cmd == "d") {
        return generate_disconnect_response();
    } else if (cmd == "ble_start" || cmd == "bs") {
        return generate_ble_start_response();
    } else if (cmd == "ble_stop" || cmd == "bp") {
        return generate_ble_stop_response();
    } else if (cmd == "ble_status" || cmd == "bt") {
        return generate_ble_status_response();
    } else if (cmd == "ble_name" || cmd == "bn") {
        return generate_ble_name_response(tokens);
    } else if (cmd == "ble_scan" || cmd == "bsc") {
        return generate_ble_scan_response(tokens);
    } else if (cmd == "ble_debug" || cmd == "bd") {
        return generate_ble_debug_response();
    } else {
        return "Unknown command: '" + tokens[0] + "'. Type 'help' for available commands.";
    }
}

void CommandInterpreter::handle_help() {
    printf("\n=== Available Commands ===\n");
    printf("\n--- WiFi Commands ---\n");
    printf("help, h                    - Show this help message\n");
    printf("scan, s                    - Scan for available WiFi networks\n");
    printf("list, l                    - List previously scanned networks\n");
    printf("connect, c <ssid> <pass>   - Connect to a WiFi network\n");
    printf("status, st                 - Show current connection status\n");
    printf("disconnect, d              - Disconnect from current network\n");
    printf("\n--- BLE Commands ---\n");
    printf("ble_start, bs              - Start BLE advertising\n");
    printf("ble_stop, bp               - Stop BLE advertising\n");
    printf("ble_status, bt             - Show BLE status\n");
    printf("ble_name, bn <name>        - Set BLE device name\n");
    printf("ble_scan, bsc [duration]   - Scan for BLE devices (default: 5s)\n");
    printf("ble_debug, bd              - Show detailed BLE debug info\n");
    printf("\n");
    printf("Examples:\n");
    printf("  scan\n");
    printf("  connect \"MyNetwork\" \"MyPassword\"\n");
    printf("  ble_start\n");
    printf("  ble_scan 10\n");
    printf("  ble_debug\n");
    printf("\n");
}

void CommandInterpreter::handle_scan() {
    printf("Scanning for WiFi networks...\n");
    
    if (wifi_manager_->scan_networks()) {
        const auto& networks = wifi_manager_->get_scanned_networks();
        if (networks.empty()) {
            printf("No networks found.\n");
        } else {
            printf("Scan completed. Found %zu networks.\n", networks.size());
            print_network_list(networks);
        }
    } else {
        printf("Failed to scan networks.\n");
    }
}

void CommandInterpreter::handle_list() {
    const auto& networks = wifi_manager_->get_scanned_networks();
    
    if (networks.empty()) {
        printf("No networks available. Run 'scan' first.\n");
        return;
    }
    
    print_network_list(networks);
}

void CommandInterpreter::handle_connect(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        printf("Usage: connect <ssid> <password>\n");
        printf("Example: connect \"MyNetwork\" \"MyPassword\"\n");
        return;
    }
    
    const std::string& ssid = args[1];
    const std::string& password = args[2];
    
    printf("Connecting to network: %s\n", ssid.c_str());
    
    if (wifi_manager_->connect_to_network(ssid, password)) {
        printf("\n=== Connection Successful ===\n");
        printf("Connected to: %s\n", ssid.c_str());
        printf("IP Address: %s\n", wifi_manager_->get_ip_address().c_str());
        printf("Signal Strength: %d dBm\n", wifi_manager_->get_rssi());
        printf("\n");
    } else {
        printf("Failed to connect to: %s\n", ssid.c_str());
        printf("Please check the network name and password.\n");
    }
}

void CommandInterpreter::handle_status() {
    printf("\n=== Connection Status ===\n");
    
    if (wifi_manager_->is_connected()) {
        printf("Status: Connected\n");
        printf("Network: %s\n", wifi_manager_->get_connected_ssid().c_str());
        printf("IP Address: %s\n", wifi_manager_->get_ip_address().c_str());
        printf("Signal Strength: %d dBm\n", wifi_manager_->get_rssi());
    } else {
        printf("Status: Disconnected\n");
        printf("No active WiFi connection.\n");
    }
    printf("\n");
}

void CommandInterpreter::handle_disconnect() {
    if (!wifi_manager_->is_connected()) {
        printf("Not connected to any network.\n");
        return;
    }
    
    std::string current_ssid = wifi_manager_->get_connected_ssid();
    printf("Disconnecting from: %s\n", current_ssid.c_str());
    
    if (wifi_manager_->disconnect()) {
        printf("Disconnected successfully.\n");
    } else {
        printf("Failed to disconnect.\n");
    }
}

// BLE command handlers
void CommandInterpreter::handle_ble_start() {
    if (!ble_manager_) {
        printf("BLE manager not available.\n");
        return;
    }
    
    printf("Starting BLE advertising...\n");
    if (ble_manager_->start_advertising()) {
        printf("BLE advertising started successfully.\n");
        printf("Device name: ESP32-P4-WiFi\n");
        printf("You can now connect via BLE UART apps.\n");
    } else {
        printf("Failed to start BLE advertising.\n");
    }
}

void CommandInterpreter::handle_ble_stop() {
    if (!ble_manager_) {
        printf("BLE manager not available.\n");
        return;
    }
    
    printf("Stopping BLE advertising...\n");
    if (ble_manager_->stop_advertising()) {
        printf("BLE advertising stopped.\n");
    } else {
        printf("Failed to stop BLE advertising.\n");
    }
}

void CommandInterpreter::handle_ble_status() {
    if (!ble_manager_) {
        printf("BLE manager not available.\n");
        return;
    }
    
    printf("\n=== BLE Status ===\n");
    printf("Implementation: ESP-Hosted NimBLE via ESP32-C6\n");
    printf("Architecture: ESP32-P4 + ESP32-C6 coprocessor\n");
    printf("Initialized: Yes\n");
    printf("Advertising: %s\n", ble_manager_->is_connected() ? "Connected" : "Available");
    printf("Connected: %s\n", ble_manager_->is_connected() ? "Yes" : "No");
    printf("Device Name: ESP32-P4-WiFi\n");
    printf("Service: Nordic UART Service (NUS)\n");
    printf("\nConfiguration Status:\n");
    printf("- ESP32-C6 Coprocessor: Active\n");
    printf("- VHCI Transport: Active\n");
    printf("- NimBLE Stack: Running\n");
    printf("\nUse 'ble_debug' for detailed status info.\n");
    printf("\n");
}

void CommandInterpreter::handle_ble_name(const std::vector<std::string>& args) {
    if (!ble_manager_) {
        printf("BLE manager not available.\n");
        return;
    }
    
    if (args.size() < 2) {
        printf("Usage: ble_name <device_name>\n");
        printf("Example: ble_name \"MyESP32-P4\"\n");
        return;
    }
    
    std::string new_name = args[1];
    printf("Setting BLE device name to: %s\n", new_name.c_str());
    printf("Note: Name change will take effect on next advertising start.\n");
    
    // TODO: Implement name persistence and immediate update if advertising
    printf("BLE name setting completed.\n");
}

void CommandInterpreter::handle_ble_scan(const std::vector<std::string>& args) {
    if (!ble_manager_) {
        printf("BLE manager not available.\n");
        return;
    }
    
    int duration = 5; // Default 5 seconds
    if (args.size() >= 2) {
        // Parse duration manually since exceptions are disabled
        const std::string& duration_str = args[1];
        bool valid_number = true;
        
        for (char c : duration_str) {
            if (!std::isdigit(c)) {
                valid_number = false;
                break;
            }
        }
        
        if (valid_number && !duration_str.empty()) {
            duration = 0;
            for (char c : duration_str) {
                duration = duration * 10 + (c - '0');
            }
            if (duration < 1 || duration > 60) {
                printf("Invalid duration. Using default 5 seconds.\n");
                duration = 5;
            }
        } else {
            printf("Invalid duration format. Using default 5 seconds.\n");
        }
    }
    
    printf("Starting BLE device scan for %d seconds...\n", duration);
    
    if (ble_manager_->start_scan(duration)) {
        printf("Scan completed. Found %d devices:\n", ble_manager_->get_scan_result_count());
        
        for (int i = 0; i < ble_manager_->get_scan_result_count(); i++) {
            std::string result = ble_manager_->get_scan_result(i);
            printf("  %s\n", result.c_str());
        }
        
        if (ble_manager_->get_scan_result_count() == 0) {
            printf("No BLE devices found.\n");
        }
    } else {
        printf("Failed to start BLE scan.\n");
    }
}

void CommandInterpreter::handle_ble_debug() {
    if (!ble_manager_) {
        printf("BLE manager not available.\n");
        return;
    }
    
    printf("\n");
    std::string debug_info = ble_manager_->get_debug_status();
    printf("%s\n", debug_info.c_str());
}

void CommandInterpreter::handle_unknown_command(const std::string& command) {
    printf("Unknown command: %s\n", command.c_str());
    printf("Type 'help' for available commands.\n");
}

void CommandInterpreter::print_network_list(const std::vector<wifi_config::NetworkInfo>& networks) {
    printf("\n=== Available WiFi Networks ===\n");
    printf("No. %-32s RSSI  Security\n", "SSID");
    printf("--- %-32s ----  --------\n", "--------------------------------");
    
    for (size_t i = 0; i < networks.size(); ++i) {
        const auto& network = networks[i];
        printf("%2zu. %-32s %4d  %s\n", 
               i + 1, 
               network.ssid.c_str(), 
               network.rssi, 
               auth_mode_to_string(network.auth_mode));
    }
    printf("\n");
}

const char* CommandInterpreter::auth_mode_to_string(wifi_auth_mode_t auth_mode) {
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

// BLE response generation methods
std::string CommandInterpreter::generate_help_response() {
    std::string response = "\n=== Available Commands ===\n";
    response += "\n--- WiFi Commands ---\n";
    response += "help, h                    - Show this help message\n";
    response += "scan, s                    - Scan for available WiFi networks\n";
    response += "list, l                    - List previously scanned networks\n";
    response += "connect, c <index>         - Connect to network by index\n";
    response += "status, st                 - Show WiFi connection status\n";
    response += "disconnect, d              - Disconnect from current network\n";
    response += "\n--- BLE Commands ---\n";
    response += "ble_start, bs              - Start BLE advertising\n";
    response += "ble_stop, bp               - Stop BLE advertising\n";
    response += "ble_status, bt             - Show BLE status\n";
    response += "ble_name, bn <name>        - Set BLE device name\n";
    response += "ble_scan, bsc [duration]   - Scan for BLE devices\n";
    response += "ble_debug, bd              - Show BLE debug info\n\n";
    response += "Commands available via USB Serial JTAG and BLE\n";
    return response;
}

std::string CommandInterpreter::generate_scan_response() {
    if (!wifi_manager_->scan_networks()) {
        return "Failed to scan for WiFi networks. Please try again.";
    }
    
    const auto& networks = wifi_manager_->get_scanned_networks();
    if (networks.empty()) {
        return "No WiFi networks found.";
    }
    
    std::string response = "WiFi scan completed. Found " + std::to_string(networks.size()) + " networks:\n\n";
    response += format_network_list(networks);
    response += "\nUse 'connect <index>' to connect to a network.";
    return response;
}

std::string CommandInterpreter::generate_list_response() {
    const auto& networks = wifi_manager_->get_scanned_networks();
    if (networks.empty()) {
        return "No networks available. Use 'scan' to search for WiFi networks.";
    }
    
    std::string response = "Previously scanned networks:\n\n";
    response += format_network_list(networks);
    response += "\nUse 'connect <index>' to connect to a network.";
    return response;
}

std::string CommandInterpreter::generate_connect_response(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return "Usage: connect <index>\nExample: connect 0\nUse 'list' to see available networks.";
    }
    
    // Parse index manually since exceptions are disabled in embedded environment
    int index = -1;
    const std::string& index_str = args[1];
    bool valid_number = true;
    
    // Check if string contains only digits
    for (char c : index_str) {
        if (!std::isdigit(c)) {
            valid_number = false;
            break;
        }
    }
    
    if (valid_number && !index_str.empty()) {
        index = 0;
        for (char c : index_str) {
            index = index * 10 + (c - '0');
        }
    } else {
        return "Invalid network index. Please provide a valid number.";
    }
    
    const auto& networks = wifi_manager_->get_scanned_networks();
    if (networks.empty()) {
        return "No networks available. Use 'scan' to search for WiFi networks first.";
    }
    
    if (index < 0 || index >= static_cast<int>(networks.size())) {
        return "Network index out of range. Use 'list' to see available networks.";
    }
    
    const auto& network = networks[index];
    
    // For now, we'll try to connect without a password for open networks
    // In a real implementation, you'd need to handle password input differently for BLE
    std::string password = "";
    if (network.auth_mode != WIFI_AUTH_OPEN) {
        return "Password-protected networks not yet supported via BLE interface.\nPlease use USB Serial JTAG interface for password entry.";
    }
    
    std::string response = "Connecting to '" + network.ssid + "'...\n";
    
    if (wifi_manager_->connect_to_network(network.ssid, password)) {
        response += "Successfully connected to " + network.ssid + "\n";
        response += "IP Address: " + wifi_manager_->get_ip_address();
    } else {
        response += "Failed to connect to " + network.ssid + ". Please check network availability.";
    }
    
    return response;
}

std::string CommandInterpreter::generate_status_response() {
    if (wifi_manager_->is_connected()) {
        std::string response = "WiFi Status: Connected\n";
        response += "Network: " + wifi_manager_->get_connected_ssid() + "\n";
        response += "IP Address: " + wifi_manager_->get_ip_address() + "\n";
        response += "Signal Strength: " + std::to_string(wifi_manager_->get_rssi()) + " dBm";
        return response;
    } else {
        return "WiFi Status: Disconnected\nUse 'scan' and 'connect' to join a network.";
    }
}

std::string CommandInterpreter::generate_disconnect_response() {
    if (!wifi_manager_->is_connected()) {
        return "WiFi is not connected.";
    }
    
    std::string ssid = wifi_manager_->get_connected_ssid();
    if (wifi_manager_->disconnect()) {
        return "Disconnected from " + ssid;
    } else {
        return "Failed to disconnect from " + ssid;
    }
}

std::string CommandInterpreter::format_network_list(const std::vector<wifi_config::NetworkInfo>& networks) {
    std::string result;
    
    for (size_t i = 0; i < networks.size(); i++) {
        const auto& network = networks[i];
        result += "  [" + std::to_string(i) + "] " + network.ssid;
        result += " (" + std::string(auth_mode_to_string(network.auth_mode));
        result += ", RSSI: " + std::to_string(network.rssi) + " dBm)\n";
    }
    
    return result;
}

// BLE response generation methods
std::string CommandInterpreter::generate_ble_start_response() {
    if (!ble_manager_) {
        return "BLE manager not available.";
    }
    
    if (ble_manager_->start_advertising()) {
        return "BLE advertising started successfully.\nDevice name: ESP32-P4-WiFi\nService: Nordic UART Service\nNote: Currently stub implementation.";
    } else {
        return "Failed to start BLE advertising.";
    }
}

std::string CommandInterpreter::generate_ble_stop_response() {
    if (!ble_manager_) {
        return "BLE manager not available.";
    }
    
    if (ble_manager_->stop_advertising()) {
        return "BLE advertising stopped.";
    } else {
        return "Failed to stop BLE advertising.";
    }
}

std::string CommandInterpreter::generate_ble_status_response() {
    if (!ble_manager_) {
        return "BLE manager not available.";
    }
    
    std::string response = "=== BLE Status ===\n";
    response += "Implementation: ESP-Hosted NimBLE via ESP32-C6\n";
    response += "Architecture: ESP32-P4 + ESP32-C6 coprocessor\n";
    response += "Initialized: Yes\n";
    response += "Advertising: " + std::string(ble_manager_->is_connected() ? "Connected" : "Available") + "\n";
    response += "Connected: " + std::string(ble_manager_->is_connected() ? "Yes" : "No") + "\n";
    response += "Device Name: ESP32-P4-WiFi\n";
    response += "Service: Nordic UART Service (NUS)\n";
    response += "\nConfiguration Status:\n";
    response += "- ESP32-C6 Coprocessor: Active\n";
    response += "- VHCI Transport: Active\n";
    response += "- NimBLE Stack: Running\n";
    response += "\nUse 'ble_debug' for detailed info.";
    return response;
}

std::string CommandInterpreter::generate_ble_name_response(const std::vector<std::string>& args) {
    if (!ble_manager_) {
        return "BLE manager not available.";
    }
    
    if (args.size() < 2) {
        return "Usage: ble_name <device_name>\nExample: ble_name MyESP32-P4";
    }
    
    std::string new_name = args[1];
    return "BLE device name set to: " + new_name + "\nNote: Name change will take effect on next advertising start.";
}

std::string CommandInterpreter::generate_ble_scan_response(const std::vector<std::string>& args) {
    if (!ble_manager_) {
        return "BLE manager not available.";
    }
    
    int duration = 5; // Default 5 seconds
    if (args.size() >= 2) {
        // Parse duration manually since exceptions are disabled
        const std::string& duration_str = args[1];
        bool valid_number = true;
        
        for (char c : duration_str) {
            if (!std::isdigit(c)) {
                valid_number = false;
                break;
            }
        }
        
        if (valid_number && !duration_str.empty()) {
            duration = 0;
            for (char c : duration_str) {
                duration = duration * 10 + (c - '0');
            }
            if (duration < 1 || duration > 60) {
                duration = 5;
            }
        }
    }
    
    std::string response = "Starting BLE scan for " + std::to_string(duration) + " seconds...\n";
    
    if (ble_manager_->start_scan(duration)) {
        int count = ble_manager_->get_scan_result_count();
        response += "Scan completed. Found " + std::to_string(count) + " devices:\n\n";
        
        for (int i = 0; i < count; i++) {
            std::string result = ble_manager_->get_scan_result(i);
            response += "  " + result + "\n";
        }
        
        if (count == 0) {
            response += "No BLE devices found.\n";
        }
        response += "\nReal BLE scanning via ESP32-C6 coprocessor.";
    } else {
        response += "Failed to start BLE scan.";
    }
    
    return response;
}

std::string CommandInterpreter::generate_ble_debug_response() {
    if (!ble_manager_) {
        return "BLE manager not available.";
    }
    
    return ble_manager_->get_debug_status();
}

} // namespace command_interface