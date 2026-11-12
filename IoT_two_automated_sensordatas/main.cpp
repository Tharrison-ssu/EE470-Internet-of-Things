/*
 * Project: EE470 Combined ESP8266 IoT
 * File: main.cpp
 * Author: Taine Harrison
 * Date: 11/11/2026 | Version: 0.4
 *
 * Purpose:
 * Read sensors on switch activation, timestamp measurements,
 * and retry delivery until confirmed.
 *
 * Inputs:
 * Temperature D2, joystick A0/SW D5, tilt D6,
 * USB serial commands and HTTP JSON responses.
 *
 * Outputs:
 * Serial diagnostics and timestamped HTTP GET requests.
 *
 * Example:
 * Joystick click sends joystick X as Node_1.
 * Tilt activation sends temperature as Node_2.
 *
 * Dependencies:
 * Arduino, ESP8266WiFi, OneWire, DallasTemperature,
 * ArduinoJson 6, config.h and transport.h.
 *
 * Usage:
 * Existing D2/D5/D6/A0 wiring is unchanged.
 * Pending records are stored in RAM.
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "config.h"
#include "transport.h"

// Local time zones and daylight-saving rules.
const char* const ZONES[] = {
    "America/New_York",
    "America/Chicago",
    "America/Denver",
    "America/Los_Angeles",
    "America/Anchorage",
    "Pacific/Honolulu",
    "America/Puerto_Rico"
};

const char* const TZ_RULES[] = {
    "EST5EDT,M3.2.0,M11.1.0",
    "CST6CDT,M3.2.0,M11.1.0",
    "MST7MDT,M3.2.0,M11.1.0",
    "PST8PDT,M3.2.0,M11.1.0",
    "AKST9AKDT,M3.2.0,M11.1.0",
    "HST10",
    "AST4"
};

uint8_t selectedZone = 3; // Pacific default.
bool apiConfirmed = false;
bool apiAttempted = false;
uint32_t apiAttemptAt = 0;
bool wasConnected = false;
bool announcedClock = false;

struct SwitchState {
    uint8_t pin;
    uint8_t active;
    int raw;
    int stable;
    uint32_t changedAt;
};

SwitchState button = {
    BUTTON_PIN, LOW, HIGH, HIGH, 0
};

SwitchState tilt = {
    TILT_PIN, TILT_ACTIVE_LEVEL, HIGH, HIGH, 0
};

// Activation requests and completed records use separate queues.
constexpr uint8_t CAPACITY = 16;

struct Activation {
    uint8_t node;
    uint8_t zone;
};

Activation events[CAPACITY];
uint8_t eventHead = 0;
uint8_t eventCount = 0;

struct Delivery {
    Measurement record;
    uint32_t lastAttempt;
    bool attempted;
};

Delivery outbox[CAPACITY];
uint8_t outHead = 0;
uint8_t outCount = 0;

uint32_t deliveredCount[2] = {0, 0};

OneWire temperatureBus(TEMP_PIN);
DallasTemperature temperatureSensor(&temperatureBus);

bool sensorPresent = false;
bool conversionBusy = false;
Measurement acquiring = {};
uint32_t conversionStartedAt = 0;

void setZoneEnvironment(uint8_t zone) {
    setenv("TZ", TZ_RULES[zone], 1);
    tzset();
}

bool clockReady() {
    return time(nullptr) >= 1704067200;
}

bool formatTimestamp(uint8_t zone,
                     char* output,
                     size_t capacity) {
    if (!clockReady()) return false;

    struct timeval now;
    gettimeofday(&now, nullptr);

    struct tm local;
    setZoneEnvironment(zone);
    localtime_r(&now.tv_sec, &local);

    char calendar[20];

    strftime(
        calendar,
        sizeof(calendar),
        "%Y-%m-%d %H:%M:%S",
        &local
    );

    snprintf(
        output,
        capacity,
        "%s.%03ld",
        calendar,
        long(now.tv_usec / 1000)
    );

    setZoneEnvironment(selectedZone);
    return true;
}

void showTime() {
    char timestamp[24];

    if (!formatTimestamp(
            selectedZone,
            timestamp,
            sizeof(timestamp))) {
        Serial.println(
            "Clock not ready; waiting for NTP or TimeAPI."
        );
        return;
    }

    Serial.print("Current time: ");
    Serial.print(timestamp);
    Serial.print(" | ");
    Serial.println(ZONES[selectedZone]);
}

void showMenu() {
    Serial.println(
        "\nSelect Your Time Zone (The default time is PT):"
    );
    Serial.println(
        "1 Eastern / New York | 2 Central / Chicago"
    );
    Serial.println(
        "3 Mountain / Denver | 4 Pacific / Los Angeles"
    );
    Serial.println(
        "5 Alaska / Anchorage | 6 Hawaii / Honolulu"
    );
    Serial.println(
        "7 Atlantic / San Juan, Puerto Rico"
    );
    Serial.println(
        "Enter 1-7. R=IP/MAC/RSSI, T=time, M=menu, Q=queue status."
    );

    Serial.print("Selected: ");
    Serial.println(ZONES[selectedZone]);
}

void showNetwork() {
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi disconnected.");
        return;
    }

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

void showQueues() {
    Serial.print("Awaiting measurement: ");
    Serial.println(eventCount);

    Serial.print("Awaiting server confirmation: ");
    Serial.println(outCount);

    Serial.print("Confirmed this boot: Node_1=");
    Serial.print(deliveredCount[0]);

    Serial.print(" Node_2=");
    Serial.println(deliveredCount[1]);

    if (SERVER_URL[0] == '\0') {
        Serial.println(
            "SERVER_URL blank: records stay local; database sending disabled."
        );
    }
}

void handleSerial() {
    while (Serial.available() > 0) {
        const char command =
            static_cast<char>(Serial.read());

        if (command >= '1' && command <= '7') {
            selectedZone = command - '1';
            setZoneEnvironment(selectedZone);

            apiConfirmed = false;
            apiAttempted = false;

            Serial.print("Selected: ");
            Serial.println(ZONES[selectedZone]);
            showTime();
        } else if (command == 'r' || command == 'R') {
            showNetwork();
        } else if (command == 't' || command == 'T') {
            showTime();
        } else if (command == 'm' || command == 'M') {
            showMenu();
        } else if (command == 'q' || command == 'Q') {
            showQueues();
        } else if (command != '\r' &&
                   command != '\n' &&
                   command != ' ') {
            Serial.println("Use 1-7, R, T, M or Q.");
        }
    }
}

void startWiFi() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Connecting to Wi-Fi...");
}

void checkWiFi() {
    const bool connected =
        WiFi.status() == WL_CONNECTED;

    if (connected && !wasConnected) {
        showNetwork();

        configTime(
            TZ_RULES[selectedZone],
            "pool.ntp.org",
            "time.nist.gov"
        );

        apiAttempted = false;
    } else if (!connected && wasConnected) {
        Serial.println(
            "Wi-Fi lost. Completed records remain queued in RAM."
        );
    }

    wasConnected = connected;
}

bool fetchTimeAPI() {
    const String url =
        String(
            "https://timeapi.io/api/Time/current/zone?timeZone="
        ) + urlEncode(ZONES[selectedZone]);

    String response;
    const int status = httpGet(url, response);

    if (status != 200) {
        Serial.print("TimeAPI unavailable, result=");
        Serial.println(status);
        return false;
    }

    DynamicJsonDocument document(1536);

    if (deserializeJson(document, response)) {
        Serial.println(
            "TimeAPI response is not valid JSON."
        );
        return false;
    }

    if (strcmp(
            document["timeZone"] | "",
            ZONES[selectedZone]) != 0 ||
        !document["dateTime"].is<const char*>()) {
        Serial.println(
            "TimeAPI did not return the selected zone and time."
        );
        return false;
    }

    Serial.print("TimeAPI: ");
    Serial.print(
        document["dateTime"].as<const char*>()
    );
    Serial.print(" | ");
    Serial.println(ZONES[selectedZone]);

    // Use the API to initialize the clock if NTP has not.
    if (!clockReady()) {
        const int year = document["year"] | 0;
        const int month = document["month"] | 0;
        const int day = document["day"] | 0;
        const int hour = document["hour"] | -1;
        const int minute = document["minute"] | -1;
        const int second = document["seconds"] | -1;
        const int millisecond =
            document["milliSeconds"] | -1;

        if (year < 2024 || year > 2099 ||
            month < 1 || month > 12 ||
            day < 1 || day > 31 ||
            hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 ||
            second < 0 || second > 59 ||
            millisecond < 0 || millisecond > 999) {
            return false;
        }

        struct tm local = {};
        local.tm_year = year - 1900;
        local.tm_mon = month - 1;
        local.tm_mday = day;
        local.tm_hour = hour;
        local.tm_min = minute;
        local.tm_sec = second;

        local.tm_isdst =
            document["dstActive"].is<bool>()
                ? (document["dstActive"].as<bool>() ? 1 : 0)
                : -1;

        setZoneEnvironment(selectedZone);

        const time_t epoch = mktime(&local);

        if (epoch < 1704067200) return false;

        struct timeval apiTime = {
            epoch,
            millisecond * 1000L
        };

        settimeofday(&apiTime, nullptr);
    }

    return true;
}

void serviceClock() {
    if (clockReady() && !announcedClock) {
        announcedClock = true;

        Serial.println(
            "Clock ready. Physical activations can now capture records."
        );
        showTime();
    }

    if (WiFi.status() != WL_CONNECTED ||
        conversionBusy ||
        eventCount > 0) {
        return;
    }

    const uint32_t interval =
        apiConfirmed ? 600000UL : 30000UL;

    if (apiAttempted &&
        uint32_t(millis() - apiAttemptAt) < interval) {
        return;
    }

    apiAttempted = true;
    apiAttemptAt = millis();
    apiConfirmed = fetchTimeAPI();
}

void initializeSwitch(SwitchState& input) {
    pinMode(input.pin, INPUT_PULLUP);

    input.raw = digitalRead(input.pin);
    input.stable = input.raw;
    input.changedAt = millis();
}

bool activated(SwitchState& input) {
    const uint32_t now = millis();
    const int raw = digitalRead(input.pin);

    if (raw != input.raw) {
        input.raw = raw;
        input.changedAt = now;
    }

    if (input.stable == input.raw ||
        uint32_t(now - input.changedAt) < DEBOUNCE_MS) {
        return false;
    }

    input.stable = input.raw;

    // Only a transition into the active state creates an event.
    return input.stable == input.active;
}

void queueActivation(uint8_t node) {
    if (!clockReady()) {
        Serial.println(
            "Activation rejected: real date/time is not ready yet."
        );
        return;
    }

    if (eventCount == CAPACITY) {
        Serial.println(
            "ACTIVATION QUEUE FULL: event not captured; no pending record overwritten."
        );
        return;
    }

    events[(eventHead + eventCount) % CAPACITY] = {
        node,
        selectedZone
    };

    ++eventCount;

    Serial.print("Activation queued: Node_");
    Serial.println(node);
}

void checkSwitches() {
    if (activated(button)) queueActivation(1);
    if (activated(tilt)) queueActivation(2);
}

void initializeSensors() {
    initializeSwitch(button);
    initializeSwitch(tilt);

    temperatureSensor.begin();
    sensorPresent =
        temperatureSensor.getDeviceCount() > 0;

    temperatureSensor.setResolution(12);
    temperatureSensor.setWaitForConversion(false);

    Serial.print("Temperature devices detected: ");
    Serial.println(
        temperatureSensor.getDeviceCount()
    );
}

void printRecord(const Measurement& record) {
    Serial.print("Captured ");
    Serial.print(record.node);

    Serial.print(" | ");
    Serial.print(record.time);

    Serial.print(" | ");
    Serial.print(record.zone);

    if (strcmp(record.node, "Node_1") == 0) {
        Serial.print(" | Joystick_X=");
        Serial.println(record.joystick);
    } else {
        Serial.print(" | Temp_C=");
        Serial.println(record.temperature);
    }
}

void saveForDelivery(const Measurement& record) {
    Delivery& delivery =
        outbox[(outHead + outCount) % CAPACITY];

    delivery.record = record;
    delivery.attempted = false;
    delivery.lastAttempt = 0;

    ++outCount;
    printRecord(record);

    if (SERVER_URL[0] == '\0') {
        Serial.println(
            "Local capture only: SERVER_URL is blank."
        );
    }
}

void serviceMeasurements() {
    if (conversionBusy) {
        if (uint32_t(
                millis() - conversionStartedAt) < 800UL) {
            return;
        }

        const float temperature =
            temperatureSensor.getTempCByIndex(0);

        conversionBusy = false;

        if (!isfinite(temperature) ||
            temperature == DEVICE_DISCONNECTED_C ||
            temperature < -55.0f ||
            temperature > 125.0f) {
            Serial.println(
                "Node_2 measurement rejected: invalid temperature."
            );
            return;
        }

        snprintf(
            acquiring.temperature,
            sizeof(acquiring.temperature),
            "%.2f",
            temperature
        );

        saveForDelivery(acquiring);
        return;
    }

    if (eventCount == 0 || outCount == CAPACITY) {
        return;
    }

    const Activation event = events[eventHead];

    eventHead = (eventHead + 1) % CAPACITY;
    --eventCount;

    acquiring = {};

    if (!formatTimestamp(
            event.zone,
            acquiring.time,
            sizeof(acquiring.time))) {
        return;
    }

    strcpy(
        acquiring.node,
        event.node == 1 ? "Node_1" : "Node_2"
    );

    snprintf(
        acquiring.zone,
        sizeof(acquiring.zone),
        "%s",
        ZONES[event.zone]
    );

    if (event.node == 1) {
        // Node_1 samples only joystick X.
        snprintf(
            acquiring.joystick,
            sizeof(acquiring.joystick),
            "%d",
            analogRead(A0)
        );

        saveForDelivery(acquiring);
        return;
    }

    if (!sensorPresent) {
        Serial.println(
            "Node_2 measurement rejected: temperature sensor absent at boot."
        );
        return;
    }

    // Node_2 starts only a temperature measurement.
    temperatureSensor.requestTemperatures();
    conversionStartedAt = millis();
    conversionBusy = true;
}

void serviceTransmissions() {
    if (SERVER_URL[0] == '\0' ||
        outCount == 0 ||
        conversionBusy) {
        return;
    }

    // Permit delivery to continue when the outbox is full.
    if (eventCount > 0 && outCount < CAPACITY) {
        return;
    }

    Delivery& delivery = outbox[outHead];
    const uint32_t now = millis();

    if (delivery.attempted &&
        uint32_t(now - delivery.lastAttempt) < RETRY_MS) {
        return;
    }

    delivery.attempted = true;
    delivery.lastAttempt = now;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(
            "Delivery pending: Wi-Fi offline; retry in 30 seconds."
        );
        return;
    }

    Serial.print("Sending saved record: ");
    Serial.print(delivery.record.node);

    Serial.print(" | ");
    Serial.println(delivery.record.time);

    if (!transmitMeasurement(delivery.record)) {
        Serial.println(
            "Unconfirmed: same timestamp and readings will retry in 30 seconds."
        );
        return;
    }

    const uint8_t node =
        strcmp(delivery.record.node, "Node_1") == 0
            ? 0
            : 1;

    ++deliveredCount[node];

    outHead = (outHead + 1) % CAPACITY;
    --outCount;

    Serial.println(
        "Delivery confirmed by matching stored/duplicate response."
    );
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nEE470 combined ESP program v0.4");

    setZoneEnvironment(selectedZone);
    showMenu();
    initializeSensors();
    showQueues();
    startWiFi();
}

void loop() {
    handleSerial();
    checkWiFi();
    checkSwitches();
    serviceMeasurements();
    serviceTransmissions();
    serviceClock();
    yield();
}