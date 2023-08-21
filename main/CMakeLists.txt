idf_component_register(
    SRCS 
        "main.cpp"
        "motordrivers.cpp"
        "motorctl.cpp"
        "config.cpp"
        "joystick.cpp"
        "buzzer.cpp"
        "control.cpp"
        "button.cpp"
        "fan.cpp"
        "wifi.c"
        "http.cpp"
        "auto.cpp"
		"currentsensor.cpp"
    INCLUDE_DIRS 
        "."
    )

spiffs_create_partition_image(spiffs ../react-app/build FLASH_IN_PROJECT)
