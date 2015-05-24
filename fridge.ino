#define SERIAL_OUTPUT false

// ========================
// THERMOMETER / HYGROMETER
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

#include <EtherCard.h>

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

byte Ethernet::buffer[700];
static uint32_t timer;
static byte session;
int res = 0;
Stash stash;
const char website[] PROGMEM = "droogkast.pruts0r.nl";

#define LAN_PIN_CS 10




void setup () {
  if (SERIAL_OUTPUT) {
     Serial.begin(115200);
  }

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

 if (logData.humidity < 60.0) {
    turnOn(RELAY_HUMIDIFIER);
  }

  if (logData.humidity > 70.0) {
    turnOff(RELAY_HUMIDIFIER);
  }

  logData.statusFridge = digitalRead(RELAY_FRIDGE) == LOW;
  logData.statusHumidifier = digitalRead(RELAY_HUMIDIFIER) == LOW;
  logData.statusFan = digitalRead(RELAY_FAN) == LOW;
  logData.statusHeater = digitalRead(RELAY_HEATER) == LOW;

  sendLogData();
  
  if (SERIAL_OUTPUT) {
     Serial.print("Temp: ");
     Serial.println(logData.fridgeTemperature);
     Serial.print("Humidity: ");
     Serial.println(logData.humidity);  
  }
  
  // Wait for 30 seconds, and check again so that we don't turn on/off the fridge constantly
  //
  // But if we'd delay(30000) our network connection would get closed/fucked up, so we need
  // to keep the network alive by sleeping shortly, and checking for packets, this way the
  // network stays alive
  int sleepInSeconds = 30;
  for (int i = 0; i < sleepInSeconds*4; i++) {
      ether.packetLoop(ether.packetReceive());
      delay(250);
  }
}

void sendLogData() {  
  /*
  //Hack to avoid RAM memory freeze
  if (stash.freeCount() <= 40) {
    Stash::initMap(56);
  }
  */
  // Cleanup to avoid the stash to become fucked up: https://github.com/jcw/ethercard/issues/18  
  stash.cleanup();
    
  // generate two fake values as payload - by using a separate stash,
  // we can determine the size of the generated message ahead of time
  byte sd = stash.create();
  
  stash.print(F("temperature="));
  stash.print(logData.fridgeTemperature);
  
  stash.print(F("&humidity="));
  stash.print(logData.humidity);
  
  stash.print(F("&status_fridge="));    
  stash.print(logData.statusFridge == 1 ? "1" : "0");

  stash.print(F("&status_humidifier="));    
  stash.print(logData.statusHumidifier == 1 ? "1" : "0");

  stash.print(F("&status_fan="));    
  stash.print(logData.statusFan == 1 ? "1" : "0");

  stash.print(F("&status_heater="));    
  stash.print(logData.statusHeater == 1 ? "1" : "0");    
  stash.save();

  // generate the header with payload - note that the stash size is used,
  // and that a "stash descriptor" is passed in as argument using "$H"
  Stash::prepare(PSTR("GET http://$F/push-status.php?$H HTTP/1.1" "\r\n"
    "Host: $F" "\r\n"
    "\r\n" "\n"
    ),
  website, sd, website);

  // send the packet - this also releases all stash buffers once done
  session = ether.tcpSend(); 

  const char* reply = ether.tcpReply(session);
 
  if (reply != 0) {
     res = 0;

     if (SERIAL_OUTPUT) {
        Serial.println(reply);
     }
  }
}


void connectLanWithDhcp() {
  if (SERIAL_OUTPUT) {
     Serial.println(F("\n[webClient]"));
  }

  if (ether.begin(sizeof Ethernet::buffer, mymac, LAN_PIN_CS) == 0) 
    if (SERIAL_OUTPUT) {
      Serial.println(F("Failed to access Ethernet controller"));
    }
  if (!ether.dhcpSetup()) {
    if (SERIAL_OUTPUT) {
      Serial.println(F("DHCP failed"));
    }
  }

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  

  if (!ether.dnsLookup(website)) {
    if (SERIAL_OUTPUT) {
      Serial.println("DNS failed");
    }
  }
    
  ether.printIp("SRV: ", ether.hisip);
}