// Drying cabinet v1.2
// 
// My arduino drying / controlled environment cabinet. This software uses 
// as Arduino (nano in my case) in combination with a DHT22, 4 way relay
// and a Enc28J60 network (UTP) adapter to created a connected fridge
// which keeps the temperature and humidity levels stable between the 
// configured thresholds.
//
// Connection instructions:
// - Connect DHT22 data to pin 4 (see #define DHTPIN)
// - Connect the relay for the fridge to pin RELAY_FRIDGE (6)
// - Connect the relay for the humidifier to pin RELAY_HUMIDIFIER (7)
// - Connect the relay for the fan to pin RELAY_FAN (8)
// - Connect the relay for the heater (black lamp) to pin RELAY_HEATER (9)


// TODO discuss challenges and problems:
// - Ethernet driver was unstable (after some time it would just not send anything anymore)
// -- Tried multiple things, like resetting using 2 methods (watchdog and reset function at address 0) both didn't work/help
// - Than decided to use UIPEthernet instead
// - But UIPEthernet was unstable as well, first test it stopped sending anything after 3 hours
// -- Now not using DNS anymore in the connect, instead using IP
// -- Added the Enc28J60.init(mymac) to see if that helps
// -- Using the errate_12 branch (27-5-2015) as that patch is not in master yet and might fix it:
//    http://forum.mysensors.org/topic/536/problems-with-enc28j60-losing-connection-freezing-using-uipethernet-or-ethershield-read-this/36
// 

// Enable the DEBUG define if you want to use the serial output
//#define DEBUG

// ========================
// THERMOMETER / HYDROMETER
// ========================
#include "DHT.h"


#define DHTPIN 4       // what pin we're connected to
#define DHTTYPE DHT22   // DHT11 or DHT22

// Initialize DHT sensor for normal 16mhz Arduino
DHT dht(DHTPIN, DHTTYPE);
// NOTE: For working with a faster chip, like an Arduino Due or Teensy, you
// might need to increase the threshold for cycle counts considered a 1 or 0.
// You can do this by passing a 3rd parameter for this threshold.  It's a bit
// of fiddling to find the right value, but in general the faster the CPU the
// higher the value.  The default for a 16mhz AVR is a value of 6.  For an
// Arduino Due that runs at 84mhz a value of 30 works.
// Example to initialize DHT sensor for Arduino Due:
//DHT dht(DHTPIN, DHTTYPE, 30);


// ========================
// RELAY
// ========================

#define RELAY_FRIDGE        6
#define RELAY_HUMIDIFIER    7
#define RELAY_FAN           8
#define RELAY_HEATER        9


// ========================
// LOG DATA
// ========================

typedef struct LogData {
  float fridgeTemperature;
  float humidity;
  int statusFridge;
  int statusHumidifier;
  int statusFan;
  int statusHeater;
};

struct LogData logData, emptyLogData;

// ========================
// LAN
// ========================

#include <UIPEthernet.h>

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

EthernetClient client;
IPAddress server = IPAddress(192,168,1,3);



void setup () {
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  // Make sure we are connected... digital!
  connectLanWithDhcp();
  
  // Initialize the temp+humidity sensor
  dht.begin();

  pinMode(RELAY_FRIDGE, OUTPUT);
  pinMode(RELAY_HUMIDIFIER, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);

  // We need to put the relays in an 'off' state, else they will be switched on by (Arduino's pin) default and, potentially, in the first loop
  // go off again as they don't need to be on, causing a off/on/off switch in a short amount of time which we want to prevent
  turnOff(RELAY_FRIDGE);
  turnOff(RELAY_HUMIDIFIER);
  turnOff(RELAY_FAN);
  turnOff(RELAY_HEATER);
}


void turnOn(int pin)
{
  digitalWrite(pin, LOW);
}

void turnOff(int pin)
{
  digitalWrite(pin, HIGH);
}

/**
 * return temp in celcius
 */
float getTemperature()
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

void loop () {
  logData = emptyLogData;
  logData.fridgeTemperature = getTemperature();
  logData.humidity = getHumidity();

  if (logData.fridgeTemperature >= 15.0) {
    turnOn(RELAY_FRIDGE);
  }

  if (logData.fridgeTemperature < 13.0) {
    turnOff(RELAY_FRIDGE);
  }

 if (logData.humidity < 50.0) {
    turnOn(RELAY_HUMIDIFIER);
  }

  if (logData.humidity > 60) {
    turnOff(RELAY_HUMIDIFIER);
  }
  
  if (logData.humidity > 75.0) {
    turnOn(RELAY_FAN);
  } else {
    turnOff(RELAY_FAN);
  }

  logData.statusFridge = digitalRead(RELAY_FRIDGE) == LOW;
  logData.statusHumidifier = digitalRead(RELAY_HUMIDIFIER) == LOW;
  logData.statusFan = digitalRead(RELAY_FAN) == LOW;
  logData.statusHeater = digitalRead(RELAY_HEATER) == LOW;

  sendLogData();
  
#ifdef DEBUG
  Serial.print(F("Temp: "));
  Serial.println(logData.fridgeTemperature);
  Serial.print(F("Humidity: "));
  Serial.println(logData.humidity);  
#endif
  
  // Wait for 30 seconds, and check again so that we don't turn on/off the fridge constantly
  //
  // But if we'd delay(30000) our network connection would get closed/fucked up, so we need
  // to keep the network alive by sleeping shortly, and checking for packets, this way the
  // network stays alive
  int sleepInSeconds = 15;
  
  for (int i = 0; i < sleepInSeconds*4; i++) {
    delay(250);

    if (logData.statusFan == true) {
      // If we have a high humidity and the fan is already on, keep the fan on for 30 seconds, to try to get the humidity down
    } else {
      // If we have a proper humidity, turn the fan on for 1 second every 15 seconds to ensure we have some airflow
      switch(i) {
        case 0: // 0 seconds
          turnOn(RELAY_FAN);
          break;
        
        case 4: // 1 second
          // turn the fan on for 1 second every 15 seconds
          turnOff(RELAY_FAN);
          break;
      }
    }
  }
}

void sendLogData() {      
  // generate two fake values as payload - by using a separate stash,
  // we can determine the size of the generated message ahead of time
  #ifdef DEBUG
  Serial.println(F("GO"));
  #endif
  
  if (client.connect(server, 80)) {
    client.print(F("GET /push-status.php?"));
    
    client.print(F("temperature="));
    client.print(logData.fridgeTemperature);
  
    client.print(F("&humidity="));
    client.print(logData.humidity);
  
    client.print(F("&status_fridge="));    
    client.print(logData.statusFridge == 1 ? F("1") : F("0"));

    client.print(F("&status_humidifier="));    
    client.print(logData.statusHumidifier == 1 ? F("1") : F("0"));

    client.print(F("&status_fan="));    
    client.print(logData.statusFan == 1 ? F("1") : F("0"));

    client.print(F("&status_heater="));    
    client.print(logData.statusHeater == 1 ? F("1") : F("0"));    
 
    client.print(F(" HTTP/1.1\n"));
  
    client.print(F("HOST: droogkast.pruts0r.nl\n\n"));
    client.stop();
  } else {
    #ifdef DEBUG
    Serial.println(F("ERROR: Cannot connect to server to push status! Restarting the Enc28J60.."));
    #endif
    
    // Try to re-init our network module if it fails, next time round it should be fine again...
    // Found: http://www.tweaking4all.com/hardware/arduino/arduino-enc28j60-ethernet/
    Enc28J60.init(mymac);
  }
}


void connectLanWithDhcp() {
#ifdef DEBUG
  Serial.println(F("Starting network connection..."));  
#endif

  Ethernet.begin(mymac);
  
#ifdef DEBUG
  Serial.println(F("Network started.]"));
  Serial.print(F("IP Address        : "));
  Serial.println(Ethernet.localIP());
  Serial.print(F("Subnet Mask       : "));
  Serial.println(Ethernet.subnetMask());
  Serial.print(F("Default Gateway IP: "));
  Serial.println(Ethernet.gatewayIP());
  Serial.print(F("DNS Server IP     : "));
  Serial.println(Ethernet.dnsServerIP());
#endif
}