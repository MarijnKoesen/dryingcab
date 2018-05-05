#ifndef DRYINGCAB_H
#define DRYINGCAB_H

// Arduino includes
#include <Arduino.h>
#include <avr/wdt.h> // Watchdog

// DryincCab includes
#include "Settings.h"
#include "WiFiConfig.h"
#include "Status.h"
#include "Output/LcdOutput.h"
#include "Output/WiFiOutput.h"

// ========================
// RELAY
// ========================

#define PIN_RELAY_FRIDGE        13
#define PIN_RELAY_HUMIDIFIER    12
#define PIN_RELAY_FAN           11
#define PIN_RELAY_HEATER        10

// ========================
// THERMOMETER / HYDROMETER
// ========================

#define PIN_DHT 5       // what pin we're connected to
#define DHTTYPE DHT22   // DHT11 or DHT22

// ========================
// DEBUG
// ========================

// To enable/disable debugging go to Debug.h
#include "Debug.h"


#endif
