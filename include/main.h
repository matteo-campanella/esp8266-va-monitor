#include <Arduino.h>
#include <sntp.h>
#include <TZ.h>
#include <Wire.h>
#include <coredecls.h>
#include <Adafruit_INA219.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "ota.h"
#include "logging.h"
#include "wifi_credentials.h"
#include "influx_credentials.h"

void setup();
