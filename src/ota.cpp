#include "ota.h"
#include "ota_credentials.h"

WiFiClient ota_client;

bool initNetwork() {
    const char *found_ssid = NULL;
    int n = 0;

    for (int i = 0; i < 3; i++) {
        n = WiFi.scanNetworks();
        if (n > 0) break;
        delay(250);
    }

    for (int i = 0; i < n; ++i) {
        int j = 0;
        while (OTA_CREDENTIALS[j][0] != NULL) {
            if (WiFi.SSID(i) == OTA_CREDENTIALS[j][0]) {
                found_ssid = OTA_CREDENTIALS[j][0];
                const char *passphrase = OTA_CREDENTIALS[j][1];
                WiFi.begin(found_ssid, passphrase);
                break;
            }
            j++;
        }
    }

    if (found_ssid == NULL) {
        Serial.println("No known WiFi found.");
        return false;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(found_ssid);

    int tries = 50;
    while (WiFi.status() != WL_CONNECTED && tries > 0) {
        delay(250);
        tries--;
    }
    if (tries == 0) {
        Serial.println("Failed to connect to WiFi!");
        return false;
    }
    WiFi.mode(WIFI_STA);
    return true;
}

// bool initNetwork() {
//     WiFi.mode(WIFI_STA);
//     WiFi.begin(OTA_SSID,OTA_PASS);
//     int tries = 50;
//     while (WiFi.status() != WL_CONNECTED && tries > 0) {
//         delay(250);
//         tries--;
//     }
//     if (tries == 0) {
//         Serial.println("Failed to connect to WiFi!");
//         return false;
//     }
//     return true;
// }

void initMDns() {
    if (!MDNS.begin(hostString)) {
      Serial.println("Error setting up MDNS responder!");
    }
    Serial.println("mDNS responder started");
}

void checkUpdates() {  
    String host = "";
    int port = 0;

    Serial.println("Sending mDNS query");
    int n = MDNS.queryService("espupdate", "tcp");
    Serial.println("mDNS query done");
    if (n == 0) {
      Serial.println("no update services found, using fallback");
      host = FALLBACK_UPDATE_SERVER_IP;
      port = FALLBACK_UPDATE_SERVER_PORT;
    }
    else {
      Serial.print(n);
      Serial.println(" service(s) found");
      for (int i = 0; i < n; ++i) {
        // Print details for each service found
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(MDNS.hostname(i));
        Serial.print(" (");
        Serial.print(MDNS.IP(i).toString());
        Serial.print(":");
        Serial.print(MDNS.port(i));
        Serial.println(")");
      }
      host = MDNS.IP(0).toString();
      port = MDNS.port(0);
    }
    Serial.println();
    String url = "http://"+host+":"+port+"/espupdate?n=" SW_NAME "&v=" SW_VERSION;
    Serial.println("Checking firmware update from "+url);
    t_httpUpdate_return ret = ESPhttpUpdate.update(ota_client,url);
    switch (ret) {
      case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str()); break;

      case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES"); break;

      case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK"); break;
    }
}

void ota_setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.flush();
  if (initNetwork()) {
    initMDns();
    checkUpdates();
  };
}
