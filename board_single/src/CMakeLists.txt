idf_component_register(
    SRCS 
        "main.cpp"
        "control.cpp"
        "button.cpp"
        "fan.cpp"
        "auto.cpp"
		"display.cpp"
        "menu.cpp"
        "encoder.cpp"
    INCLUDE_DIRS 
        "."
        "../include"
        "../../components"
        "../../common/include"
PRIV_REQUIRES nvs_flash mdns json spiffs esp_http_server encoder gpio buzzer freemodbus
    )


spiffs_create_partition_image(spiffs ../../react-app/build FLASH_IN_PROJECT)
