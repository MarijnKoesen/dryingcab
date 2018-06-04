// Drying cabinet v2.0
//
// My arduino drying / controlled environment cabinet. This software uses
// as Arduino Mega in combination with a DHT22, 4 way relay and an ESP8266
// (WiFi) adapter to created a connected fridge which keeps the temperature
// and humidity levels stable between the configured thresholds.
//
// IMPORTANT: 1) for this to work correctly we need proper watchdog support
//
// Connection instructions (see dryingcab.h for details, or to change pins):
// - Connect DHT22 data to pin 5
// - Connect the relay for the fridge to pin PIN_RELAY_FRIDGE (13)
// - Connect the relay for the humidifier to pin PIN_RELAY_HUMIDIFIER (12)
// - Connect the relay for the fan to pin PIN_RELAY_FAN (11)
// - Connect the relay for the heater (black lamp) to pin PIN_RELAY_HEATER (10)
// - Connect the relay further more to gnd and +5v
// - Connect the 4x20 I2C LCD to the SDA(20) & SCL (21), gnd and +5v
// - For further connection instructions see the connection diagram in this project (/doc)

// Notes on the algorithm:
// - 30 seconds was too long, we humidified for too long, getting a humidity that was too high
// - 15 seconds is working fine
// - We don't want to go too short, to avoid turning on the electric stuff too often/soon after shutting it down
// - When the humidity is too high we put on the fan so that we blow out wet air, and suck in new warmer, dryer air
// - To ensure we have the recommend airflow we turn the fan on for 1 second every 15 seconds, this circulates the air

// Notes about the construction:
// - About the fan:
// -- Make sure the fan is pointed so that it's sucking air out, otherwise you will get overpressure
// -- Make another hole on the opposite side (diagonally) of the fan, so that new air is sucked in there
// -- Place the fan at the top, so that it sucks warm air out
// -- Create the other (small) hole in the bottom

// My problems and challenges when developing this, what worked, and what didn't with regards to the ethernet
// - In the previous versions I was using a Enc28J60 UTP board, but there were a lot of problems with this, see previous
//   versions of this project, in this commentblock, where it's explained in more detail
// - Also I wanted to have control over de settings from the outside, without needing to re-program the fridge
//   to achieve this I wanted to add an LCD and some buttons. So I can control settings easily.
// -- Currently there is no support for the buttons though, this will be done in a later stage as the fridge is still
//    busy with my capocollo which is more important and cannot be interrupted ;-)
// - To get rid of the Enc28J60 board I decided to use the ESP8266 WiFi module instead
// - Connecting to Enc28J60 is done using a Serial connection, but since the Arduino UNO only has 1 Serial connection
//   it's hard to debug and use the WiFi at the same time. I tried to user SoftwareSerial, but that was just a PITA.
//   First because Enc28J60 doesn't really like it, as the best baud rate is 115200, but SoftwareSerial only supports
//   up to 9600. Apart from that I couldn't really get it to work. So I decided to change it, and go with Arduino Mega instead
// - Currently i'm running with watchdog enabled
// -- This may be a bit of legacy, as I needed it for the Enc28J60 board, but I think it's good practice to keep this running
//    so that for what ever reason something goes wrong/is hanging, the arduino will just be reset and will start again.
// -- The idea is that I set watchdog on 8sec, so if I don't give the watchdog an ok every 8 seconds it reboots the arduino
// - The project is developed in Atom using PlatformIO, with this setup i'm able to create a better project
//   structure with separate files, includes, libraries and a makefile that I can run. I prefer this a lot over the
//   standard Arduino IDE, which works, but I don't really like it, you can't even format source code in it for example.
// - For the WiFi I ended up using the Arduino WiFiEsp library, the stock Android library doesn't allow you to specify
//   which Serial port to use, and since I'm on a mega a want to be able to specify Serial1.
// - The ESP8266 needs 3.3V, but the Arduino can't supply enough ampere on the 3.3v port, so I read online
// -- To fix thix we need to create a voltage devider so we can go from the 5v output 3.3v
//    as the 5v output _is_ able to supply enough current
// -- To make out lives easier we're using the the LM1117T-3.3 with fixed 3.3v output
// - If I start to see problems with WiFi I can still try to hook up the reset pin of the esp8266 to a digital pin
//   on the mega, say pin 1, then you can do the following:
//     // In setup();
//     pinMode(1, OUTPUT);
//     // And thenn something like this:
//     void resetEsp() { digitalWrite(1, HIGH); delay(1000); digitalWrite(1, LOW); }
//   as per: http://www.galaxysofts.com/new/master-reset-esp8266-wifi-module-using-arduino-mega-2560/




#include "dryingcab.h"

Settings settings;
Status status;


// ==========================
// DHT (temp/humidity sensor)
// ==========================
// Initialize DHT sensor for normal 16mhz Arduino
// NOTE: For working with a faster chip, like an Arduino Due or Teensy, you
// might need to increase the threshold for cycle counts considered a 1 or 0.
// You can do this by passing a 3rd parameter for this threshold.  It's a bit
// of fiddling to find the right value, but in general the faster the CPU the
// higher the value.  The default for a 16mhz AVR is a value of 6.  For an
// Arduino Due that runs at 84mhz a value of 30 works.
// Example to initialize DHT sensor for Arduino Due:
//DHT dht(DHTPIN, DHTTYPE, 30);
#include <DHT.h>
DHT dht(PIN_DHT, DHTTYPE);

// ==========================
// Setup out periherals
// ==========================
void setup() {
    // avoid reset loops
    MCUSR = 0;

    // https://bigdanzblog.wordpress.com/2014/10/24/arduino-watchdog-timer-wdt-example-code/
    // immediately disable watchdog timer so set will not get interrupted
    wdt_disable();
    wdt_reset();

    // Now init the lcd
    lcd_init();

    // Initialize the temp+humidity sensor
    dht.begin();

    pinMode(PIN_RELAY_FRIDGE, OUTPUT);
    pinMode(PIN_RELAY_HUMIDIFIER, OUTPUT);
    pinMode(PIN_RELAY_FAN, OUTPUT);
    pinMode(PIN_RELAY_HEATER, OUTPUT);

    // We need to put the relays in an 'off' state, else they will be switched on by (Arduino's pin) default and, potentially, in the first loop
    // go off again as they don't need to be on, causing a off/on/off switch in a short amount of time which we want to prevent
    turnOff(PIN_RELAY_FRIDGE);
    turnOff(PIN_RELAY_HUMIDIFIER);
    turnOff(PIN_RELAY_FAN);
    turnOff(PIN_RELAY_HEATER);

    // And set default settings
    settings.minHumidity = 50;
    settings.maxHumidity = 75;

    settings.minTemp = 13;
    settings.maxTemp = 15;

    status.humidity = 75.7;
    status.temperature = 13.5;

    //   // The following forces a pause before enabling WDT. This gives the IDE a chance to
    //   // call the bootloader in case something dumb happens during development and the WDT
    //   // resets the MCU too quickly. Once the code is solid, remove this.
    delay(2000);
    wdt_enable(WDTO_8S);
}

// ==========================
// Our main application loop
// ==========================
void loop() {
  debugln("\n\nStarting lcd...");
  LcdOutput lcdOutput;
  debugln("Lcd started");
  debugln("Lcd outputing..");
  lcdOutput.output(status, settings);
  debugln("Lcd outputted");

  debugln("Create wifi");
  WiFiOutput wifiOutput(WIFI_NETWORK, WIFI_PASSWORD);
  debugln("Wifi created...");
  wifiOutput.connect();
  // debugln("Wifi connected...");
  // wifiOutput.output(status, settings);
  // debugln("Create outputtedd...");

  while(1) {
    readSensors();
    processSensorData();
    updateStatus();

    lcdOutput.output(status, settings);
    wifiOutput.output(status, settings);

    #ifdef DEBUG
    // Move to DebugOutput.output()
      Serial.print("Temp: ");
      Serial.println(status.temperature);

      Serial.print("Humidity: ");
      Serial.println(status.humidity);
    #endif

    // Wait for 15 seconds, and check again so that we don't turn on/off the fridge constantly
    //
    // But if we'd delay(30000) our network connection would get closed/fucked up, so we need
    // to keep the network alive by sleeping shortly, and checking for packets, this way the
    // network stays alive
    int sleepInSeconds = 15;

    for (int i = 0; i < sleepInSeconds; i++) {
      // Reset the watchdog, so that the arduino will not get reset
      wdt_reset();

      delay(1000);

      if (status.statusFan == true) {
        // If we have a high humidity and the fan is already on, keep the fan on for 15 seconds, to try to get the humidity down
      } else {
        // If we have a proper humidity, turn the fan on for 1 second every 15 seconds to ensure we have some airflow
        switch(i) {
          case 0:
            turnOn(PIN_RELAY_FAN);
            break;

          case 1:
            turnOff(PIN_RELAY_FAN);
            break;
        }
      }
    }
  }
}

void readSensors() {
  status.temperature = getTemperatureInCelcius();
  status.humidity = getHumidity();
}

void processSensorData() {
  if (status.temperature >= settings.maxTemp) {
    turnOn(PIN_RELAY_FRIDGE);
  }

  if (status.temperature < settings.minTemp) {
    turnOff(PIN_RELAY_FRIDGE);
  }

 if (status.humidity < settings.minHumidity) {
    turnOn(PIN_RELAY_HUMIDIFIER);
  }

  if (status.humidity > settings.maxHumidity) {
    turnOff(PIN_RELAY_HUMIDIFIER);
  }

  if (status.humidity > settings.maxHumidity) {
    // Currently the outside humidity is higher then inside the fridge
    // so turning on the fan only makes it worse.
    // TODO: add another temp/humidty sensor outside of the fridge, and only
    // turn on the fan is the outside humidity is lower than inside
    //turnOn(PIN_RELAY_FAN);
  } else {
    //turnOff(PIN_RELAY_FAN);
  }
}

void updateStatus() {
  status.statusFridge = digitalRead(PIN_RELAY_FRIDGE) == LOW;
  status.statusHumidifier = digitalRead(PIN_RELAY_HUMIDIFIER) == LOW;
  status.statusFan = digitalRead(PIN_RELAY_FAN) == LOW;
  status.statusHeater = digitalRead(PIN_RELAY_HEATER) == LOW;
}

//
// int failedLogTries = 0;
// void sendLogData() {
//   // generate two fake values as payload - by using a separate stash,
//   // we can determine the size of the generated message ahead of time
//   #ifdef DEBUG
//   Serial.println(F("GO"));
//   #endif
//
// //   if (client.connect(server, 80)) {
// //     client.print(F("GET /push-status.php?"));
// //
// //     client.print(F("temperature="));
// //     client.print(logData.fridgeTemperature);
// //
// //     client.print(F("&humidity="));
// //     client.print(logData.humidity);
// //
// //     client.print(F("&status_fridge="));
// //     client.print(logData.statusFridge == 1 ? F("1") : F("0"));
// //
// //     client.print(F("&status_humidifier="));
// //     client.print(logData.statusHumidifier == 1 ? F("1") : F("0"));
// //
// //     client.print(F("&status_fan="));
// //     client.print(logData.statusFan == 1 ? F("1") : F("0"));
// //
// //     client.print(F("&status_heater="));
// //     client.print(logData.statusHeater == 1 ? F("1") : F("0"));
// //
// //     client.print(F(" HTTP/1.1\n"));
// //
// //     client.print(F("HOST: droogkast.pruts0r.nl\n\n"));
// //     client.stop();
// //   } else {
// //     #ifdef DEBUG
// //     Serial.println(F("ERROR: Cannot connect to server to push status! Restarting the Enc28J60.."));
// //     #endif
// //
// //     failedLogTries++;
// //     if (failedLogTries > 3) {
// //       reboot = true;
// //     }
// //
// //     // Try to re-init our network module if it fails, next time round it should be fine again...
// //     // Found: http://www.tweaking4all.com/hardware/arduino/arduino-enc28j60-ethernet/
// //     client.stop();
// //     //Enc28J60.init(mymac);
// //   }
// }
//
//
// // void connectLanWithDhcp() {
// // #ifdef DEBUG
// //   Serial.println(F("Starting network connection..."));
// // #endif
// //
// //   Ethernet.begin(mymac);
// //
// // #ifdef DEBUG
// //   Serial.println(F("Network started.]"));
// //   Serial.print(F("IP Address        : "));
// //   Serial.println(Ethernet.localIP());
// //   Serial.print(F("Subnet Mask       : "));
// //   Serial.println(Ethernet.subnetMask());
// //   Serial.print(F("Default Gateway IP: "));
// //   Serial.println(Ethernet.gatewayIP());
// //   Serial.print(F("DNS Server IP     : "));
// //   Serial.println(Ethernet.dnsServerIP());
// // #endif
// // }

void turnOn(int pin)
{
  digitalWrite(pin, LOW);
}

void turnOff(int pin)
{
  digitalWrite(pin, HIGH);
}

float getTemperatureInCelcius()
{
  return dht.readTemperature();
}

/**
 * Reading temperature or humidity takes about 250 milliseconds!
 * Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
 */
float getHumidity()
{
  return dht.readHumidity();
}

void showDebugModeOnLcd()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("RUNNING IN DEBUG MODE!!"));
}
