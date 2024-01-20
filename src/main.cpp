#include "main.h"
#include "ota.h"

#define led_on() digitalWrite(LED_BUILTIN, LOW);
#define led_off() digitalWrite(LED_BUILTIN, HIGH);

#define SITE_NAME "lab"
#define DEVICE_NAME "camera1"
#define SENSOR_NAME DEVICE_NAME"_battery"
#define OFF_VOLTAGE 14.4
#define ON_VOLTAGE 16.4
#define SLEEP_TIME 300e6
#define PIC_PIN 12
#define LOOP_DELAY 500

Adafruit_INA219 ina219;
float voltage,current;

#define MQTT_SERVER "81981a2bf6a64f8e8c093fe623897e5c.s2.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_TOPIC SITE_NAME
#define MQTT_COMMAND_TOPIC MQTT_TOPIC"/"DEVICE_NAME"/cmd"
#define MQTT_STATUS_TOPIC MQTT_TOPIC"/"DEVICE_NAME"/sta"

// InfluxDB v2 server url (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"

// Set timezone string according to <https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html>
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#define EEPROM_SIZE 2

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor(SENSOR_NAME);
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
DynamicJsonDocument doc(1024);
uint8_t hoursOFF[24];
uint8_t daysOFF[7];
bool stayON=false;
uint8_t stayONSeconds=0;
unsigned int loopCounter=10;
bool isSwitchedOn = false;
bool wasOn = false;
bool wasStayON = false;

bool connect_wifi() {
    const char *found_ssid = NULL;
    int n = 0;

    timeSync(TZ_Europe_Rome, "pool.ntp.org", "time.nist.gov", "ntp1.inrim.it");

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
    if (doc.containsKey("daysOFF")) {
        JsonArray jsonDaysOFF = doc["daysOFF"].as<JsonArray>();
        for(uint8_t i=0;i<7;i++) daysOFF[i]=false;
        for(JsonVariant v : jsonDaysOFF) daysOFF[v.as<uint8_t>()]=true;
    }
    if (doc.containsKey("hoursOFF")) {
        JsonArray jsonHoursOFF = doc["hoursOFF"].as<JsonArray>();
        for(uint8_t i=0;i<24;i++) hoursOFF[i]=false;
        for(JsonVariant v : jsonHoursOFF) hoursOFF[v.as<uint8_t>()]=true;
    }
    if (doc.containsKey("stayON")) {
        stayON = true;
        stayONSeconds = doc["stayON"].as<uint16_t>();
    }
}

bool mqtt_connect() {
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    secureClient.setInsecure();
    mqttClient.setServer(MQTT_SERVER,MQTT_PORT);   
    mqttClient.setCallback(mqtt_callback); 
    if (mqttClient.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD))
        if (mqttClient.subscribe(MQTT_COMMAND_TOPIC)) return true;
    return false;
}

void mqtt_send(char *topic, const char *key, const char *value, bool isRetained) {
    if (!mqttClient.connected()) return;
    char mqtt_message[128];
    doc.clear();
    doc["deviceId"]=SENSOR_NAME;
    doc["siteId"]=SITE_NAME;
    doc[key]=value;
    serializeJson(doc, mqtt_message);
    mqttClient.publish(topic, mqtt_message, isRetained);
}

void mqtt_off() {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
        mqttClient.unsubscribe(MQTT_COMMAND_TOPIC);
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
        isSwitchedOn = true;
    }
    else {
        tone(PIC_PIN,1000);
        log_println("Switching Load OFF");
        isSwitchedOn = false;
    }
    mqtt_send(MQTT_STATUS_TOPIC,"status",isSwitchOn?"ON":"OFF",true);    
}

uint8_t getHourOfDay() {
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);
    return tm_info->tm_hour;
}

uint8_t getDayOfWeek() {
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);
    return tm_info->tm_wday;    
}

bool manage_power_switch() {
    log_printfln("Status is %s",isSwitchedOn?"ON":"OFF");
    uint8_t h = getHourOfDay();
    uint8_t d = getDayOfWeek();
    log_printfln("Hour Of Day: %d -> %s",h,hoursOFF[h]?"OFF":"ON"); //remove
    log_printfln("Day Of Week: %d -> %s",d,daysOFF[d]?"OFF":"ON"); //remove
    if (stayON) {
        log_printfln("Forcing ON for %u seconds",stayONSeconds);
        wasOn = isSwitchedOn;
        switchOnOff(true);
        loopCounter = stayONSeconds * (1000 / LOOP_DELAY);
        stayON = false;
        wasStayON = true;
        return false;
    }
    if (!wasStayON) {
        if ((hoursOFF[h] || daysOFF[d]) && isSwitchedOn) {
            switchOnOff(false);
            wasOn = true;
            return true;
        }
        if (!(hoursOFF[h] || daysOFF[d]) && wasOn) {
            switchOnOff(true);
            wasOn = false;
        }
    }
    else {
        switchOnOff(wasOn);
        wasStayON = false;
        return true;
    }
    if (voltage<OFF_VOLTAGE) switchOnOff(false);
    if (voltage>ON_VOLTAGE) switchOnOff(true);
    return true;
}

void eeprom_setup() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0,wasOn);
    EEPROM.get(1,isSwitchedOn);
}

void eeprom_off() {
    EEPROM.put(0,wasOn);
    EEPROM.put(1,isSwitchedOn);
    EEPROM.commit();
    EEPROM.end();
}

void deep_sleep() {
    log_println("DeepSleep");
    delay(1000);
    mqtt_off();
    eeprom_off();
    ESP.deepSleep(SLEEP_TIME,RF_NO_CAL);
    //ESP.deepSleep(60e6,RF_NO_CAL);
}

void setup() {
    ota_setup();
    eeprom_setup();
    led_off();
    connect_wifi();
    ina219_setup();
    influxdb_setup();
    mqtt_setup();
    log_println("UP!");
    send_sensor_data();
}

void loop() {
    mqttClient.loop();
    delay(LOOP_DELAY);
    if (loopCounter--==0) {
        if (manage_power_switch()) deep_sleep();
    }
}