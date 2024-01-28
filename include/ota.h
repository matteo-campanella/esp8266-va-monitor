#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>

void ota_setup();

#define hostString "esp8266"
#define FALLBACK_UPDATE_SERVER_IP "192.168.1.8"
#define FALLBACK_UPDATE_SERVER_PORT 4321