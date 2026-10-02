/*
 * Title: ESP8266 LiPo Discharge Data Logger
 *
 * Purpose:
 * Measure and record LiPo battery voltage during
 * discharge using LEDs as the approved load.
 *
 * Inputs:
 * Battery voltage through voltage divider connected to A0
 *
 * Outputs:
 * Sample number, elapsed time, ADC reading,
 * and calculated battery voltage
 *
 * Authors:
 * Taine Harrison/Steve Gutierrez
 *
 * Version:
 * V1 - Discharge experiment
 *
 * File Dependencies:
 * Arduino.h
 */


#include <Arduino.h>


// External voltage divider
const float R1 = 10000.0;     // 10 kOhm
const float R2 = 22000.0;     // 22 kOhm


// ADC constants
const float ADC_MAX = 1023.0;
const float ADC_VOLTAGE = 3.3;


// Take one measurement every 60 seconds
const unsigned long SAMPLE_INTERVAL = 60000;


unsigned long sampleNumber = 0;
unsigned long previousTime = 0;
unsigned long startTime = 0;




/*
 * Function: takeMeasurement()
 *
 * Reads the ADC, converts the reading to battery voltage,
 * and prints the results in CSV format.
 */
void takeMeasurement()
{
    // Read analog input
    int adcValue = analogRead(A0);


    // Convert ADC reading to voltage at A0
    float adcVoltage =
        (adcValue / ADC_MAX) * ADC_VOLTAGE;


    // Convert A0 voltage back to actual battery voltage
    float batteryVoltage =
        adcVoltage * ((R1 + R2) / R2);


    // Calculate elapsed time in minutes
    float elapsedMinutes =
        (millis() - startTime) / 60000.0;


    // Print CSV data
    Serial.print(sampleNumber);
    Serial.print(",");


    Serial.print(elapsedMinutes, 2);
    Serial.print(",");


    Serial.print(adcValue);
    Serial.print(",");


    Serial.println(batteryVoltage, 3);
}




void setup()
{
    // Start serial communication
    Serial.begin(115200);


    delay(1000);


    // Start experiment timer
    startTime = millis();


    // CSV column headings
    Serial.println("Sample,Time_min,ADC,Battery_V");


    // Take first measurement immediately at t = 0
    takeMeasurement();


    previousTime = millis();
}




void loop()
{
    unsigned long currentTime = millis();


    // Check whether 60 seconds have passed
    if (currentTime - previousTime >= SAMPLE_INTERVAL)
    {
        previousTime = currentTime;


        sampleNumber++;


        takeMeasurement();
    }
}

