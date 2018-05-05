#ifndef WIFI_OUTPUT_H
#define WIFI_OUTPUT_H

#include "WiFiEsp.h"
#include <Output/Output.h>
#include <Status.h>
#include <Settings.h>
#include <Debug.h>
#include <avr/wdt.h>

class WiFiOutput: public Output
{
  public:
    WiFiOutput(const char *ssid, const char *pass);
    void connect();
    virtual void output(Status status, Settings settings);

  private:
    WiFiEspClient client;
    unsigned long lastConnectTime;
    const char *ssid;
    const char *pass;
    bool isInitialized;

    bool isConnected();
};

#endif
