/*
 * Project: EE470 Combined ESP8266 IoT
 * File: transport.h
 * Author: Taine Harrison
 * Date: 11/11/2026 | Version: 0.4
 * Purpose: Define measurement records and network interfaces.
 * Inputs: Node, timestamp, time zone and assigned sensor value.
 * Outputs: HTTP response and delivery confirmation.
 * Dependencies: Arduino.h, transport.cpp.
 * Usage: Preserve the original record when retrying delivery.
 */
#pragma once
#include <Arduino.h>

struct Measurement {
    char node[8];
    char time[24];
    char zone[32];
    char temperature[12]; // Node_2 only.
    char joystick[8];    // Node_1 only.
};

String urlEncode(const String& value);
int httpGet(const String& url, String& response);
bool transmitMeasurement(const Measurement& measurement);