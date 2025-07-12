#pragma once

// NOTE: This is an embedded project using ESP-IDF framework
// - Exception handling is disabled (-fno-exceptions)
// - RTTI is disabled (-fno-rtti)
// - Use manual error checking instead of try/catch blocks
// - Prefer C-style error codes or boolean returns for error handling

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "wifi_manager.hpp"

// Forward declarations
namespace ble_serial {
    class BLEManager;
}

namespace relay_control {
    class RelayManager;
}

namespace command_interface {

class CommandInterpreter {
public:
    explicit CommandInterpreter(std::shared_ptr<wifi_config::WiFiManager> wifi_manager);
    ~CommandInterpreter();
    
    // Set BLE manager for BLE commands
    void set_ble_manager(std::shared_ptr<ble_serial::BLEManager> ble_manager);
    
    // Set relay manager for relay commands
    void set_relay_manager(std::shared_ptr<relay_control::RelayManager> relay_manager);
    
    // Core functionality
    bool initialize();
    void start_interactive_mode();
    void process_command(const std::string& command);
    
    // BLE interface - process command and return response
    std::string process_command_with_response(const std::string& command);
    
    // WiFi command handlers
    void handle_help();
    void handle_scan();
    void handle_list();
    void handle_connect(const std::vector<std::string>& args);
    void handle_status();
    void handle_disconnect();
    
    // BLE command handlers
    void handle_ble_start();
    void handle_ble_stop(); 
    void handle_ble_status();
    void handle_ble_name(const std::vector<std::string>& args);
    void handle_ble_scan(const std::vector<std::string>& args);
    void handle_ble_debug();
    
    // Relay command handlers
    void handle_relay_on(const std::vector<std::string>& args);
    void handle_relay_off(const std::vector<std::string>& args);
    void handle_relay_toggle(const std::vector<std::string>& args);
    void handle_relay_status();
    void handle_relay_debug();
    
    void handle_unknown_command(const std::string& command);
    
private:
    void setup_usb_serial();
    void print_welcome_message();
    void print_prompt();
    std::string read_command_line();
    std::vector<std::string> parse_command(const std::string& command);
    void print_network_list(const std::vector<wifi_config::NetworkInfo>& networks);
    const char* auth_mode_to_string(wifi_auth_mode_t auth_mode);
    
    // BLE response generation methods
    std::string generate_help_response();
    std::string generate_scan_response();
    std::string generate_list_response();
    std::string generate_connect_response(const std::vector<std::string>& args);
    std::string generate_status_response();
    std::string generate_disconnect_response();
    std::string generate_ble_start_response();
    std::string generate_ble_stop_response();
    std::string generate_ble_status_response();
    std::string generate_ble_name_response(const std::vector<std::string>& args);
    std::string generate_ble_scan_response(const std::vector<std::string>& args);
    std::string generate_ble_debug_response();
    std::string generate_relay_on_response(const std::vector<std::string>& args);
    std::string generate_relay_off_response(const std::vector<std::string>& args);
    std::string generate_relay_toggle_response(const std::vector<std::string>& args);
    std::string generate_relay_status_response();
    std::string generate_relay_debug_response();
    std::string format_network_list(const std::vector<wifi_config::NetworkInfo>& networks);
    
    // Member variables
    std::shared_ptr<wifi_config::WiFiManager> wifi_manager_;
    std::shared_ptr<ble_serial::BLEManager> ble_manager_;
    std::shared_ptr<relay_control::RelayManager> relay_manager_;
    bool initialized_;
    
    // Constants
    static const size_t CMD_BUFFER_SIZE = 256;
};

} // namespace command_interface