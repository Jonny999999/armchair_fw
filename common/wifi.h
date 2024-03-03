#pragma once

//TODO: currently wifi names and passwords are configured in wifi.c -> move this to config?

//initialize nvs-flash and netif (needed for both AP and CLIENT)
//both functions have to be run once at startup 
void wifi_initNvs();
void wifi_initNetif();


//function to start an access point (config in wifi.c)
void wifi_start_ap(void);
//function to disable/stop access point
void wifi_stop_ap(void);

//function to connect to existing wifi network (config in wifi.c)
void wifi_start_client(void);
//function to disable/deinit client
void wifi_stop_client(void);


    
