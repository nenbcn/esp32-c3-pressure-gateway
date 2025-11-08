// includes.h
#ifndef INCLUDES_H
#define INCLUDES_H

// Arduino Libraries
#include <Arduino.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <stdarg.h>

// FreeRTOS Libraries
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Over-the-Air Update Library
#include <HTTPUpdate.h>

// NeoPixel Library
#include <Adafruit_NeoPixel.h>

// System State
#include "system_state.h"

// NTP Configuration
#include <time.h>

#endif