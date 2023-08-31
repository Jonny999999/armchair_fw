idf_component_register(
    SRCS 
        "main.cpp"
        "config.cpp"
        "joystick.cpp"
        "control.cpp"
        "button.cpp"
        "http.cpp"
        "auto.cpp"
		"uart.cpp"
		"encoder.cpp"
    INCLUDE_DIRS 
        "."
    )

spiffs_create_partition_image(spiffs ../react-app/build FLASH_IN_PROJECT)
