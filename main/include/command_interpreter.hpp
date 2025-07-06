#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "wifi_manager.hpp"

namespace command_interface {

class CommandInterpreter {
public:
    explicit CommandInterpreter(std::shared_ptr<wifi_config::WiFiManager> wifi_manager);
    ~CommandInterpreter();
    
    // Core functionality
    bool initialize();
    void start_interactive_mode();
    void process_command(const std::string& command);
    
    // Command handlers
    void handle_help();
    void handle_scan();
    void handle_list();
    void handle_connect(const std::vector<std::string>& args);
    void handle_status();
    void handle_disconnect();
    void handle_unknown_command(const std::string& command);
    
private:
    void setup_usb_serial();
    void print_welcome_message();
    void print_prompt();
    std::string read_command_line();
    std::vector<std::string> parse_command(const std::string& command);
    void print_network_list(const std::vector<wifi_config::NetworkInfo>& networks);
    const char* auth_mode_to_string(wifi_auth_mode_t auth_mode);
    
    // Member variables
    std::shared_ptr<wifi_config::WiFiManager> wifi_manager_;
    bool initialized_;
    
    // Constants
    static const size_t CMD_BUFFER_SIZE = 256;
};

} // namespace command_interface