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
    )

spiffs_create_partition_image(spiffs ../../react-app/build FLASH_IN_PROJECT)
