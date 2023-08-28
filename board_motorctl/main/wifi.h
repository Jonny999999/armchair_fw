#pragma once

//TODO: currently wifi names and passwords are configured in wifi.c -> move this to config?

//initialize nvs-flash and netif (needed for both AP and CLIENT)
//has to be run once at startup 
//Note: this cant be put in wifi_init functions because this may not be in deinit functions
void wifi_initNvs_initNetif();


//function to start an access point
void wifi_init_ap(void);
//function to disable/deinit access point
void wifi_deinit_ap(void);

//function to connect to existing wifi network
void wifi_init_client(void);
//function to disable/deinit client
void wifi_deinit_client(void);


    
