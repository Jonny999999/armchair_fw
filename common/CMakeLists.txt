idf_component_register(
    SRCS 
        "wifi.c"
		"buzzer.cpp"
		"uart_common.cpp"
		"types.cpp"
		"motordrivers.cpp"
		"motorctl.cpp"
		"currentsensor.cpp"
		"joystick.cpp"
		"http.cpp"
		"speedsensor.cpp"
        "chairAdjust.cpp"
    INCLUDE_DIRS 
        "."
		PRIV_REQUIRES nvs_flash mdns json spiffs esp_http_server
    )

