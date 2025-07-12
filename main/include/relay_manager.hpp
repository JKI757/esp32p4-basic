#pragma once

// NOTE: ESP-IDF Embedded Development Constraints
// - No exception handling (-fno-exceptions)
// - No RTTI (-fno-rtti) 
// - Use error codes and boolean returns instead of exceptions
// - Memory management through ESP-IDF heap functions

#include <string>
#include <memory>
#include "driver/gpio.h"

namespace relay_control {

/**
 * @brief Relay Manager class for ESP32-P4 dual relay board control
 * 
 * Controls two relays connected to GPIO pins:
 * - Relay 1: GPIO32
 * - Relay 2: GPIO46
 * 
 * Supports individual and simultaneous relay control with safety features.
 */
class RelayManager {
public:
    // GPIO pin definitions for relay board variant
    static constexpr gpio_num_t RELAY_1_GPIO = GPIO_NUM_32;
    static constexpr gpio_num_t RELAY_2_GPIO = GPIO_NUM_46;
    
    // Relay identifiers
    enum class RelayId {
        RELAY_1 = 1,
        RELAY_2 = 2,
        ALL_RELAYS = 0
    };
    
    // Relay states
    enum class RelayState {
        OFF = 0,
        ON = 1
    };

    RelayManager();
    ~RelayManager();

    /**
     * @brief Initialize GPIO pins for relay control
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Set state of a specific relay
     * @param relay_id Which relay to control (RELAY_1, RELAY_2, or ALL_RELAYS)
     * @param state Desired state (ON or OFF)
     * @return true if operation successful
     */
    bool set_relay_state(RelayId relay_id, RelayState state);

    /**
     * @brief Get current state of a specific relay
     * @param relay_id Which relay to query (RELAY_1 or RELAY_2)
     * @return Current relay state
     */
    RelayState get_relay_state(RelayId relay_id) const;

    /**
     * @brief Turn on a specific relay
     * @param relay_id Which relay to turn on (RELAY_1, RELAY_2, or ALL_RELAYS)
     * @return true if operation successful
     */
    bool turn_on(RelayId relay_id);

    /**
     * @brief Turn off a specific relay
     * @param relay_id Which relay to turn off (RELAY_1, RELAY_2, or ALL_RELAYS)
     * @return true if operation successful
     */
    bool turn_off(RelayId relay_id);

    /**
     * @brief Toggle state of a specific relay
     * @param relay_id Which relay to toggle (RELAY_1, RELAY_2, or ALL_RELAYS)
     * @return true if operation successful
     */
    bool toggle(RelayId relay_id);

    /**
     * @brief Turn off all relays (safety function)
     * @return true if operation successful
     */
    bool turn_off_all();

    /**
     * @brief Get status of both relays
     * @return Formatted status string
     */
    std::string get_status() const;

    /**
     * @brief Get detailed debug information
     * @return Debug status string
     */
    std::string get_debug_status() const;

    /**
     * @brief Check if relay manager is initialized
     * @return true if initialized
     */
    bool is_initialized() const;

private:
    /**
     * @brief Configure GPIO pin for relay control
     * @param gpio_pin GPIO pin number to configure
     * @return true if configuration successful
     */
    bool configure_gpio_pin(gpio_num_t gpio_pin);

    /**
     * @brief Set GPIO pin state
     * @param gpio_pin GPIO pin to control
     * @param state Desired state (0=OFF, 1=ON)
     * @return true if operation successful
     */
    bool set_gpio_state(gpio_num_t gpio_pin, int state);

    /**
     * @brief Get GPIO pin state
     * @param gpio_pin GPIO pin to read
     * @return Current pin state (0 or 1)
     */
    int get_gpio_state(gpio_num_t gpio_pin) const;

    /**
     * @brief Convert RelayId to GPIO pin number
     * @param relay_id Relay identifier
     * @return Corresponding GPIO pin number
     */
    gpio_num_t relay_id_to_gpio(RelayId relay_id) const;

    /**
     * @brief Convert RelayId to string
     * @param relay_id Relay identifier
     * @return String representation
     */
    const char* relay_id_to_string(RelayId relay_id) const;

    /**
     * @brief Convert RelayState to string
     * @param state Relay state
     * @return String representation
     */
    const char* relay_state_to_string(RelayState state) const;

    // Member variables
    bool initialized_;
    RelayState relay_1_state_;
    RelayState relay_2_state_;
    
    // Statistics
    uint32_t relay_1_switch_count_;
    uint32_t relay_2_switch_count_;
    uint32_t total_operations_;
};

} // namespace relay_control