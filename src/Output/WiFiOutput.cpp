#include "WiFiOutput.h"

WiFiOutput::WiFiOutput(const char *ssid, const char *pass)
{
  this->ssid = ssid;
  this->pass = pass;
  this->lastConnectTime = 0;
  this->isInitialized = false;
}

void WiFiOutput::connect()
{
  unsigned long now = millis();
  if (now - lastConnectTime < 10000 && lastConnectTime != 0) {
    // Only try to connect once every 10 seconds
    debugln("Not connecting... threshold not reached");
    return;
  }
  lastConnectTime = now;

  debugln("Trying to connect to WiFi...");
  if (!this->isInitialized) {
    wdt_reset();
    debugln("Starting Serial...");
    Serial1.begin(115200);
    WiFi.init(&Serial1);
    this->isInitialized = true;
  }

  int status = WiFi.status();
  if (status == WL_CONNECTED) {
    debugln("Already connected to WiFi");
    return;
  }

  // check for the presence of the shield:
  if (status == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    return;
  }

  // Attempt to connect to Wifi network:
  debug("Attempting to connect to SSID: ");
  debug(ssid);
  debugln(", this can take some time...");
  wdt_reset();

  status = WiFi.begin(ssid, pass);
}

bool WiFiOutput::isConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

void WiFiOutput::output(Status status, Settings settings)
{
  debugln("is connected??");
  debug(WiFi.status());
  if (!isConnected()) {
    debugln("nop");
    connect();
    return;
  }
  debugln("yes, setnding data");

  if (client.connect("192.168.1.3", 80)) {
    String line =
      String("GET /push-status.php?")
        + String("temperature=")
        + String(status.temperature)
        + String("&setMinTemperature=")
        + String(settings.minTemp)
        + String("&setMaxTemperature=")
        + String(settings.maxTemp)

        + String("&humidity=")
        + String(status.humidity)
        + String("&setMinHumidity=")
        + String(settings.minHumidity)
        + String("&setMaxHumidity=")
        + String(settings.maxHumidity)

        + String("&status_fridge=")
        + String(status.statusFridge == 1 ? "1" : "0")
        + String("&status_humidifier=")
        + String(status.statusHumidifier == 1 ? "1" : "0")
        + String("&status_fan=")
        + String(status.statusFan == 1 ? "1" : "0")
        + String("&status_heater=")
        + String(status.statusHeater == 1 ? "1" : "0")
        + String(" HTTP/1.1");

    // client.print(F("?"));
    // client.print(F("temperature="));
    // client.print(status.temperature);
    // client.print(F("&setMinTemperature="));
    // client.print(settings.minTemp);
    // client.print(F("&setMaxTemperature="));
    // client.print(settings.maxTemp);
    //
    // client.print(F("&humidity="));
    // client.print(status.humidity);
    // client.print(F("&setMinHumidity="));
    // client.print(settings.minHumidity);
    // client.print(F("&setMaxHumidity="));
    // client.print(settings.maxHumidity);
    //
    // client.print(F("&status_fridge="));
    // client.print(status.statusFridge == 1 ? F("1") : F("0"));
    // client.print(F("&status_humidifier="));
    // client.print(status.statusHumidifier == 1 ? F("1") : F("0"));
    // client.print(F("&status_fan="));
    // client.print(status.statusFan == 1 ? F("1") : F("0"));
    // client.print(F("&status_heater="));
    // client.print(status.statusHeater == 1 ? F("1") : F("0"));


    // client.print(F("?"));
    client.println(line);
    client.println("HOST: droogkast.pruts0r.nl");
    client.println("Connection: close");
    client.println();
    client.stop();

          // client.println("GET /2 HTTP/1.1");
          // client.println("Host: droogkast.pruts0r.nl");
          // client.println("Connection: close");
          // client.println();
          // client.stop();

  } else {
    debugln("ERROR: Cannot connect to server to push status! Restarting the Enc28J60..");
  }
}
