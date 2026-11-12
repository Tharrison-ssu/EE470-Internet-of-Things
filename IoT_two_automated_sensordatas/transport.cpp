/*
 * Project: EE470 Combined ESP8266 IoT
 * File: transport.cpp
 * Author: Taine Harrison
 * Date: 11/11/2026 | Version: 0.4
 * Purpose: Send measurements and verify server confirmations.
 * Inputs: URL or saved Measurement.
 * Outputs: HTTP response and matching delivery confirmation.
 * Dependencies: ESP8266HTTPClient, ESP8266WiFi, BearSSL, ArduinoJson 6.
 * Usage: HTTP success alone does not confirm database storage.
 */
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include "config.h"
#include "transport.h"

String urlEncode(const String& value) {
    String encoded;
    encoded.reserve(value.length() * 3);
    const char* hex = "0123456789ABCDEF";

    for (size_t i = 0; i < value.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(value[i]);

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 15];
        }
    }

    return encoded;
}

static int performGet(WiFiClient& client,
                      const String& url,
                      String& response) {
    HTTPClient http;
    http.setTimeout(5000);

    if (!http.begin(client, url)) return -1000;

    http.addHeader("Accept", "application/json");
    http.addHeader("Cache-Control", "no-cache");

    const int status = http.GET();

    if (status > 0) {
        if (http.getSize() > 2048) {
            http.end();
            return -1001;
        }

        response = http.getString();

        if (response.length() > 2048) {
            response = "";
            http.end();
            return -1001;
        }
    }

    http.end();
    return status;
}

int httpGet(const String& url, String& response) {
    response = "";

    if (url.startsWith("https://")) {
        if (!DEMO_SKIP_CERTIFICATE_CHECK) {
            Serial.println(
                "HTTPS refused: configure trust anchors before disabling demo mode."
            );
            return -1002;
        }

        BearSSL::WiFiClientSecure client;
        client.setInsecure(); // Course demonstration configuration.
        client.setTimeout(5000);

        return performGet(client, url, response);
    }

    if (url.startsWith("http://")) {
        WiFiClient client;
        return performGet(client, url, response);
    }

    return -1003;
}

bool transmitMeasurement(const Measurement& measurement) {
    String url = SERVER_URL;
    url += url.indexOf('?') >= 0 ? '&' : '?';

    url += "node_number=" + urlEncode(measurement.node);
    url += "&time=" + urlEncode(measurement.time);
    url += "&timezone=" + urlEncode(measurement.zone);

    const bool joystickNode =
        strcmp(measurement.node, "Node_1") == 0;

    const bool temperatureNode =
        strcmp(measurement.node, "Node_2") == 0;

    // Send only the sensor assigned to the activated node.
    if (joystickNode) {
        url += "&sensor2=" + urlEncode(measurement.joystick);
    } else if (temperatureNode) {
        url += "&sensor1=" + urlEncode(measurement.temperature);
    } else {
        return false;
    }

    String response;
    const int code = httpGet(url, response);

    Serial.print("Receiver HTTP result: ");
    Serial.println(code);

    if (code != 200 && code != 201 && code != 409) return false;

    DynamicJsonDocument document(1536);

    if (deserializeJson(document, response)) {
        Serial.println(
            "No delivery confirmation: response is not valid JSON."
        );
        return false;
    }

    const char* status = document["status"] | "";

    if (strcmp(status, "stored") != 0 &&
        strcmp(status, "duplicate") != 0) {
        return false;
    }

    JsonObjectConst record =
        document["record"].as<JsonObjectConst>();

    if (record.isNull()) return false;

    // A confirmation must describe the exact queued measurement.
    const bool commonMatches =
        strcmp(record["node_number"] | "", measurement.node) == 0 &&
        strcmp(record["time"] | "", measurement.time) == 0 &&
        strcmp(record["timezone"] | "", measurement.zone) == 0;

    if (!commonMatches) return false;

    if (joystickNode) {
        return !record.containsKey("sensor1") &&
               strcmp(
                   record["sensor2"] | "",
                   measurement.joystick
               ) == 0;
    }

    return !record.containsKey("sensor2") &&
           strcmp(
               record["sensor1"] | "",
               measurement.temperature
           ) == 0;
}