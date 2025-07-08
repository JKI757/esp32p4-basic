idf_component_register(SRCS "main.cpp"
                            "src/wifi_manager.cpp"
                            "src/command_interpreter.cpp"
                            "src/ble_manager.cpp"
                       INCLUDE_DIRS "."
                                   "include"
                       REQUIRES esp_wifi
                               esp_netif
                               esp_event
                               nvs_flash
                               esp_hosted
                               esp_wifi_remote
                               driver
                               bt
                               vfs)
