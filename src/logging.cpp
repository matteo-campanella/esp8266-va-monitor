#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define UDP_BROADCAST_PORT 8888

WiFiUDP udp;

void udpBroadcast(const char *message) {
    if (!WiFi.isConnected()) return;
    if (udp.beginPacket(WiFi.broadcastIP(),UDP_BROADCAST_PORT)) {
        udp.write(message);
        udp.endPacket();
    }
}

void log_print(const char *message) {
    Serial.print(message);
    udpBroadcast(message);
}

void log_printf(const char *message, ...) {
    va_list argp;
    va_start(argp, message);

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), message, argp);
    log_print(buffer);
}

void log_vprintf(const char *message, va_list argp) {
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), message, argp);
    log_print(buffer);
}

void log_printfln(const char *message, ...) {
    va_list argp;
    va_start(argp, message);
    log_vprintf(message, argp);
    log_print("\n");
}

void log_println(String message) {
    log_print(message.c_str());
    log_print("\n");
}
