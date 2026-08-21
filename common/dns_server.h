#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//===============================
//====== dns redirect server ====
//===============================
// Simple DNS server that answers every type-A query with the IP of the soft-AP.
// Together with the redirect of all foreign hosts in http.cpp this forms a
// captive portal: the phone detects "this network requires sign in" and opens
// the armchair web-app automatically (no need to type the IP manually).
//
// Note: has to be started AFTER the access-point was started (needs netif "WIFI_AP_DEF")

// start the dns server task (does nothing when already running)
void dns_server_start(void);

// stop the dns server task and close its socket (blocks until task exited)
void dns_server_stop(void);

#ifdef __cplusplus
}
#endif
