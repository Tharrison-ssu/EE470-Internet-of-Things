/* EE470 MQTT Node_3 | Taine Harrison
 * One program for the potentiometer, button, serial/MQTT LED control and peer ESP.
 * The Pi relay is the only component that submits Node_3 data to Hostinger.
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include <time.h>
#include "secrets.h"
#include "ca_certificates.h"

constexpr char BASE[] = "IoT/potent_35317d28";
constexpr char BROKER[] = "broker.hivemq.com";
constexpr uint8_t LED_PIN = D1;     // GPIO5: external LED, active HIGH.
constexpr uint8_t BUTTON_PIN = D5;  // GPIO14: momentary button to GND.
// Part E: enter your classmate's exact switch topic when known; otherwise leave empty.
constexpr char PEER_SWITCH_TOPIC[] = "";

BearSSL::WiFiClientSecure socketClient;
BearSSL::X509List trust(ROOT_CA_PEM);
PubSubClient mqtt(socketClient);
String potTopic, commandTopic, stateTopic, switchTopic, telemetryTopic, statusTopic;
bool led = false, pulse = false, stateDirty = true, switchDirty = true, telemetryDirty = true;
bool sawPress = false;
int rawButton = HIGH, stableButton = HIGH;
uint32_t changedAt=0, pulseAt=0, lastPot=0, lastSerial=0, lastConnect=0, lastWifi=0;

void setLed(bool on) {
    led = on;
    digitalWrite(LED_PIN, on ? HIGH : LOW);
    stateDirty = true; telemetryDirty = true;
    Serial.printf("LED output: %d\n", led ? 1 : 0);
}
void message(char* topic, byte* payload, unsigned int length) {
    // Payloads are length-delimited bytes, not zero-terminated strings.
    if (length != 1 || (payload[0] != '0' && payload[0] != '1')) return;
    if (commandTopic == topic || (strlen(PEER_SWITCH_TOPIC) && strcmp(topic, PEER_SWITCH_TOPIC) == 0)) {
        setLed(payload[0] == '1');
    }
}
void buttonTick(uint32_t now) {
    int reading = digitalRead(BUTTON_PIN);
    if (reading != rawButton) { rawButton = reading; changedAt = now; }
    if (reading != stableButton && uint32_t(now - changedAt) >= 35) {
        stableButton = reading;
        Serial.printf("Physical button: %d\n", reading == LOW ? 1 : 0);
        if (reading == LOW) sawPress = true;
        if (reading == HIGH && sawPress) {
            sawPress = false;
            pulse = true; pulseAt = now;
            setLed(true);  // Light the local LED for the five-second pulse.
            switchDirty = telemetryDirty = true;
        }
    }
    // No delay(5000): MQTT and LED commands continue during this interval.
    if (pulse && uint32_t(now - pulseAt) >= 5000) {
        pulse = false;
        setLed(false);  // End the local LED pulse.
        switchDirty = telemetryDirty = true;
    }
}
void connectTick(uint32_t now) {
    if (WiFi.status() != WL_CONNECTED) {
        if (uint32_t(now-lastWifi) >= 15000) { lastWifi=now; WiFi.reconnect(); Serial.println("Waiting for Wi-Fi..."); }
        return;
    }
    // Verified TLS needs the clock synchronized by NTP.
    if (time(nullptr) < 1700000000 || mqtt.connected() || uint32_t(now-lastConnect) < 5000) return;
    lastConnect = now;
    String clientId = "node3_" + String(ESP.getChipId(), HEX);
    if (mqtt.connect(clientId.c_str(), statusTopic.c_str(), 0, true, "offline")) {
        mqtt.subscribe(commandTopic.c_str(), 0);
        if (strlen(PEER_SWITCH_TOPIC)) mqtt.subscribe(PEER_SWITCH_TOPIC, 0);
        mqtt.publish(statusTopic.c_str(), "online", true);
        stateDirty = switchDirty = telemetryDirty = true;
        lastPot = millis() - 15000;
        Serial.println("MQTT connected on TLS port 8883.");
    } else Serial.printf("MQTT error %d; retrying in five seconds.\n", mqtt.state());
}
void publishTick(uint32_t now) {
    if (!mqtt.connected()) return;
    if (uint32_t(now-lastPot) >= 15000) {
        lastPot = now;
        telemetryDirty = true;
    }
    if (stateDirty && mqtt.publish(stateTopic.c_str(), led ? "1" : "0", true)) stateDirty = false;
    if (switchDirty && mqtt.publish(switchTopic.c_str(), pulse ? "1" : "0", false)) switchDirty = false;
    if (telemetryDirty) {
        // One ADC sample feeds both topics so live and stored values match.
        int pot = analogRead(A0);
        char value[8]; snprintf(value, sizeof(value), "%d", pot);
        mqtt.publish(potTopic.c_str(), value, false);
        char data[80];
        snprintf(data, sizeof(data), "{\"pot\":%d,\"led\":%d,\"switch\":%d}", pot, led ? 1 : 0, pulse ? 1 : 0);
        if (mqtt.publish(telemetryTopic.c_str(), data, false)) {
            telemetryDirty = false; Serial.printf("Telemetry: %s\n", data);
        }
    }
}
void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT); pinMode(BUTTON_PIN, INPUT_PULLUP); setLed(false);
    rawButton = stableButton = digitalRead(BUTTON_PIN); sawPress = stableButton == LOW;
    potTopic=String(BASE)+"/pot"; commandTopic=String(BASE)+"/led/set";
    stateTopic=String(BASE)+"/led/state"; switchTopic=String(BASE)+"/switch";
    telemetryTopic=String(BASE)+"/telemetry"; statusTopic=String(BASE)+"/status";
    socketClient.setTrustAnchors(&trust); socketClient.setBufferSizes(16384,512);
    mqtt.setServer(BROKER,8883); mqtt.setCallback(message); mqtt.setSocketTimeout(15); mqtt.setBufferSize(384);
    WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID,WIFI_PASSWORD); configTime(0,0,"pool.ntp.org","time.nist.gov");
    Serial.println("Serial: type 1 for LED on, 0 for off. A0 prints once per second.");
    Serial.println("Wi-Fi but no MQTT? Check NTP, certificates and port 8883.");
}
void loop() {
    while (Serial.available()) { char c=Serial.read(); if(c=='0'||c=='1')setLed(c=='1'); }
    uint32_t now=millis(); buttonTick(now); connectTick(now);
    if(mqtt.connected())mqtt.loop();
    now=millis(); publishTick(now);
    if(uint32_t(now-lastSerial)>=1000){lastSerial=now;Serial.printf("A0=%d | button=%d | LED=%d\n",analogRead(A0),stableButton==LOW?1:0,led?1:0);}
    yield();
}
