idf_component_register(
    SRCS 
        "wifi.c"
		"buzzer.cpp"
		"uart_common.cpp"
    INCLUDE_DIRS 
        "."
		PRIV_REQUIRES nvs_flash
    )

