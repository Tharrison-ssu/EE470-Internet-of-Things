/*
 * Project/Program Name: EE470 Combined ESP8266 IoT
 * File Name: config.h
 * Author: Taine Harrison | Date: 09/11/2026 | Version: 0.3
 * Purpose: Configure the network, receiver URL, hardware and retry interval.
 * Inputs: Locally entered Wi-Fi credentials and agreed receiver URL.
 * Outputs: Constants used by the program.
 * Example Application: Connect the existing sensor circuit to a hotspot.
 * Dependencies: Arduino.h and NodeMCU ESP8266.
 * Usage Notes: Keep credentials private. SERVER_URL stays blank until the
 * PHP receiver implements SERVER_CONTRACT.txt; blank disables sending.
 */
#pragma once
#include <Arduino.h>

constexpr char WIFI_SSID[] = "hamzah’s iPhone";
constexpr char WIFI_PASSWORD[] = "1214161810";
constexpr char SERVER_URL[] = "https://taineengineering.com/insert.php";
constexpr uint32_t RETRY_MS = 30000;

constexpr uint8_t TEMP_PIN = D2;
constexpr uint8_t BUTTON_PIN = D5;
constexpr uint8_t TILT_PIN = D6;
constexpr uint8_t TILT_ACTIVE_LEVEL = LOW;
constexpr uint32_t DEBOUNCE_MS = 40;

// Course demonstration mode: HTTPS encryption is used, but certificates are
// not authenticated. See START_HERE.txt before use outside the demonstration.
constexpr bool DEMO_SKIP_CERTIFICATE_CHECK = true;
