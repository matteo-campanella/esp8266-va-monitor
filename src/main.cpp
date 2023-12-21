#include "main.h"
#include "ota.h"

#define led_on() digitalWrite(LED_BUILTIN, LOW);
#define led_off() digitalWrite(LED_BUILTIN, HIGH);

#define SITE_NAME "gmssiziano"
#define DEVICE_NAME "camera1"
#define SENSOR_NAME DEVICE_NAME"_battery"
#define OFF_VOLTAGE 14.4
#define ON_VOLTAGE 16.4
#define SLEEP_TIME 300e6
#define PIC_PIN 12

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
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
DynamicJsonDocument doc(1024);



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

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    char buffer[length+1];
    strncpy(buffer,(char *)payload,length);
    buffer[length]=0;
    doc.clear();
    DeserializationError error = deserializeJson(doc,buffer);
    if (error) return;
    JsonArray hoursOFF = doc["hoursOFF"].as<JsonArray>();
    for (int i = 0; i < hoursOFF.size() ; i++ ) {
        log_println(hoursOFF[i][0]);
        log_println(hoursOFF[i][1]);
    }
}

bool mqtt_connect() {
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    secureClient.setInsecure();
    mqttClient.setServer(MQTT_SERVER,MQTT_PORT);   
    mqttClient.setCallback(mqtt_callback); 
    if (mqttClient.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD))
        if (mqttClient.subscribe(MQTT_TOPIC"/cmd")) return true;
    return false;
}

void mqtt_send(char *topic, const char *key, const char *value) {
    if (!mqttClient.connected()) return;
    char mqtt_message[128];
    doc.clear();
    doc["deviceId"]=SENSOR_NAME;
    doc["siteId"]=SITE_NAME;
    doc[key]=value;
    serializeJson(doc, mqtt_message);
    mqttClient.publish(topic, mqtt_message, true);
}

void mqtt_off() {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
        mqttClient.unsubscribe(MQTT_TOPIC"/cmd");
    }
}

bool mqtt_setup() {
    log_print("MQT");
    if (mqtt_connect()) {
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
        tone(PIC_PIN,500);
        log_println("Switching Load ON");
        mqtt_send(MQTT_TOPIC"/sta","status","ON");
    }
    else {
        tone(PIC_PIN,1000);
        log_println("Switching Load OFF");
        mqtt_send(MQTT_TOPIC"/sta","status","OFF");
    }
}

void manage_safety_switch() {
    if (voltage<OFF_VOLTAGE) switchOnOff(false);
    if (voltage>ON_VOLTAGE) switchOnOff(true);
}

void deep_sleep() {
    delay(1000);
    mqtt_off();
    ESP.deepSleep(SLEEP_TIME,RF_NO_CAL);    
}

void setup() {
    ota_setup();
    led_off();
    connect_wifi();
    ina219_setup();
    influxdb_setup();
    mqtt_setup();
    log_println("UP!");
    send_sensor_data();
    manage_safety_switch();
}

unsigned int i=10;

void loop() {
    mqttClient.loop();
    delay(500);
    if (i--==0) deep_sleep();
}