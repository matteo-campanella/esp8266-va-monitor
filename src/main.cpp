#include "main.h"
#include "ota.h"

#define led_on() digitalWrite(LED_BUILTIN, LOW);
#define led_off() digitalWrite(LED_BUILTIN, HIGH);

#define SENSOR_NAME "camera1_battery"
#define OFF_VOLTAGE 14.4
#define ON_VOLTAGE 15.2
#define SWITCH_PIN 14
#define SLEEP_TIME 300e6

bool isTimeValid = false;
Adafruit_INA219 ina219;
float voltage,current;

// InfluxDB v2 server url (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"

// Set timezone string according to <https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html>
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor(SENSOR_NAME);


bool connect_wifi() {
    const char *found_ssid = NULL;
    int n = 0;

    timeSync(TZ_Etc_UTC, "pool.ntp.org", "time.nist.gov", "ntp1.inrim.it");

    for (int i = 0; i < 3; i++) {
        n = WiFi.scanNetworks();
        if (n > 0) break;
        delay(250);
    }

    for (int i = 0; i < n; ++i) {
        int j = 0;
        while (WIFI_CREDENTIALS[j][0] != NULL) {
            if (WiFi.SSID(i) == WIFI_CREDENTIALS[j][0]) {
                found_ssid = WIFI_CREDENTIALS[j][0];
                const char *passphrase = WIFI_CREDENTIALS[j][1];
                WiFi.begin(found_ssid, passphrase);
                break;
            }
            j++;
        }
    }

    if (found_ssid == NULL) {
        log_println("No known WiFi found.");
        return false;
    }

    log_printfln("Connecting to WiFi: %s ...", found_ssid);

    int tries = 50;
    while (WiFi.status() != WL_CONNECTED && tries > 0) {
        delay(250);
        tries--;
    }
    if (tries == 0) {
        log_println("Failed to connect to WiFi!");
        return false;
    }
    WiFi.mode(WIFI_STA);
    return true;
}

bool ina219_setup() {
    log_print("INA");
    if (! ina219.begin()) {
        log_print("-");
        return false;
    }
    else {
        ina219.setCalibration_32V_1A();
        log_print("+");
        return true;
    }
}

bool influxdb_setup() {
    log_print("INF");
    if (client.validateConnection()) {
        log_print("+");
        return true;
    }
    else {
        log_print("-");
        return false;
    }
}

void send_sensor_data() {
    voltage = ina219.getBusVoltage_V();
    current = ina219.getCurrent_mA();

    sensor.clearFields();
    sensor.addField("V",voltage);
    sensor.addField("mA",current);
    bool rc = client.writePoint(sensor);
    log_print(sensor.toLineProtocol().c_str());
    log_println(rc?" OK":" KO");
}

void switchOnOff(bool isSwitchOn) {
    if (isSwitchOn) {
        digitalWrite(SWITCH_PIN,HIGH);
        log_println("Swtiching Load ON");
    }
    else {
        digitalWrite(SWITCH_PIN,LOW);
        log_println("Switching Load OFF");
    }
}

void manage_safety_switch() {
    if (voltage<OFF_VOLTAGE) switchOnOff(false);
    if (voltage>ON_VOLTAGE) switchOnOff(true);
}

void pin_setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    led_off();
    pinMode(SWITCH_PIN,OUTPUT);
    digitalWrite(SWITCH_PIN,LOW);
}

void setup() {
    ota_setup();
    pin_setup();
    led_off();
    connect_wifi();
    ina219_setup();
    influxdb_setup();
    log_println("UP!");
    send_sensor_data();
    manage_safety_switch();
    delay(1000);
    ESP.deepSleep(SLEEP_TIME,RF_NO_CAL);
}

void loop() {

}