#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

// Enable the DEBUG define if you want to use the serial output for debugging
// Also if enabling debugging, make sure to connect a serial client, otherwise
// buffers can overflow, so only enable when debugging, when in 'production' mode
// this should be disabled
// 'Serial' will be used for debugging
// 'Serial1' will be used to connect to the WiFi ESP8266
//#define DEBUG

extern void debug(const char* message);
extern void debugln(const char* message);

#endif
