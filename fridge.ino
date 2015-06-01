// Drying cabinet v1.3
// 
// My arduino drying / controlled environment cabinet. This software uses 
// as Arduino (nano in my case) in combination with a DHT22, 4 way relay
// and a Enc28J60 network (UTP) adapter to created a connected fridge
// which keeps the temperature and humidity levels stable between the 
// configured thresholds.
//
// IMPORTANT: for this to work correctly we need proper watchdog support
//            currently you need to flash a custom bootloader on your nano
//            see instructions below
//
// Connection instructions:
// - Connect DHT22 data to pin 4 (see #define DHTPIN)
// - Connect the relay for the fridge to pin RELAY_FRIDGE (6)
// - Connect the relay for the humidifier to pin RELAY_HUMIDIFIER (7)
// - Connect the relay for the fan to pin RELAY_FAN (8)
// - Connect the relay for the heater (black lamp) to pin RELAY_HEATER (9)

// Notes on the algorithm:
// - 30 seconds was too long, we humidified for too long, getting a humidity that was too high
// - 15 seconds is working fine
// - we don't wan to go too short, to avoid turning on the electric stuff too often/soon after shutting it down
// - when the humidity is too high we put on the fan soo that we blow out wet air, and suck in new warmer, dryer air
// - To ensure we have the recommend airflow we turn the fan on for 1 second every 15 seconds, this circulates the air

// Notes about the construction:
// - About the fan:
// -- make sure the fan is pointed so that it's sucking air out, otherwise you will get overpressure
// -- Make another hole on the opposite side (diagonally) of the fan, so that new air is sucked in there
// -- Place the fan at the top, so that it sucks warm air out

// My problems and challenges when developing this, what worked, and what didn't with regards to the ethernet
// - Ethernet driver was unstable (after some time it would just not send anything anymore)
// -- Tried multiple things, like resetting using 2 methods (watchdog and reset function at address 0) both didn't work/help
// - Than decided to use UIPEthernet instead
// - But UIPEthernet was unstable as well, first test it stopped sending anything after 3 hours
// -- Tried using no DNS anymore in the connect, instead using IP
// -- Added the Enc28J60.init(mymac) to see if that helps if the connection failed
// -- Using the errate_12 branch (27-5-2015) as that patch is not in master yet and might fix it:
//    http://forum.mysensors.org/topic/536/problems-with-enc28j60-losing-connection-freezing-using-uipethernet-or-ethershield-read-this/36
// -- In the end it ran for about 12 hours and than failed
// - Currently i'm running UIPEthernet but with watchdog enabled
// -- The idea is that I set watchdog on 4sec, so if I don't give the watchdog an ok every 4 seconds it reboots the arduino
// -- I reset the arduino if my client.connect() didn't work for 3 time (in total, not consecutive)
// -- OOTB Watchdog is bugged, and if   you let the watchdog reset your arduino it will enter an endless reset loop
// -- This reset loop causes you arduino to do nothing, and the fridge will be turned off, and nothing is controller, very bad!
// -- To fix this, you can burn a custom bootloader 'optiboot' which has proper support for watchdog
// -- Burning the bootloader should be done with an external programma, like another uno or an Arduino ISP
// -- To force the optiboot bootloader find the 'board.txt' inside your Arduino.app directory and change the line:
// --- nano.menu.cpu.atmega328.bootloader.file=atmega/ATmegaBOOT_168_atmega328.hex
// --- to:
// --- nano.menu.cpu.atmega328.bootloader.file=optiboot/optiboot_atmega328.hex
// --- now restart your arduino ide
// --- and click on tools > burn bootloader (making sure the board/processor are ok)
// -- MAKE SURE to do a 'MCUSR = 0;' and 'wdt_disable();' as the first thing in setup(), else reset loop will still occur!
// -- Upto now it's been running for almost 24 hours (ATM 4 days and counting...) without a problem (although I had multiple restarts), so should be 'fine' now
//    

// Enable the DEBUG define if you want to use the serial output, used for debugging, make sure to connect a 
// serial client, otherwise buffers can overflow, so only enable when debugging, when in 'production' mode
// this should be disabled
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

// ========================
// FreeMemory
// ========================
//
// See http://playground.arduino.cc/Code/AvailableMemory
#include "MemoryFree.h"

// ========================
// Reset WatchDog
// ======================== 
#include <Arduino.h>
#include <avr/wdt.h>



// ========================
// Application
// ========================

// When reboot is set to true, we don't reset the WDT anymore and the arduino
// will get rebooted
boolean reboot = false;

void setup () {
  // avoid reset loops
  MCUSR = 0;
  
  // https://bigdanzblog.wordpress.com/2014/10/24/arduino-watchdog-timer-wdt-example-code/
  // immediately disable watchdog timer so set will not get interrupted
  wdt_disable();
  wdt_reset();  
    
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
  
  // The following forces a pause before enabling WDT. This gives the IDE a chance to
  // call the bootloader in case something dumb happens during development and the WDT
  // resets the MCU too quickly. Once the code is solid, remove this.
  delay(2000);  
  wdt_enable(WDTO_4S);
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

  Serial.print("freeMemory()=");
  Serial.println(freeMemory());
#endif
  
  // Wait for 15 seconds, and check again so that we don't turn on/off the fridge constantly
  //
  // But if we'd delay(30000) our network connection would get closed/fucked up, so we need
  // to keep the network alive by sleeping shortly, and checking for packets, this way the
  // network stays alive
  int sleepInSeconds = 15;
  
  for (int i = 0; i < sleepInSeconds; i++) {
    if (!reboot) {
      wdt_reset();
    }
    
    delay(1000);

    if (logData.statusFan == true) {
      // If we have a high humidity and the fan is already on, keep the fan on for 15 seconds, to try to get the humidity down
    } else {
      // If we have a proper humidity, turn the fan on for 1 second every 15 seconds to ensure we have some airflow
      switch(i) {
        case 0:
          turnOn(RELAY_FAN);
          break;
       
        case 1:
          turnOff(RELAY_FAN);
          break;
      }
    }
  }
}

int failedLogTries = 0;
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
    
    failedLogTries++;    
    if (failedLogTries > 3) {
      reboot = true;
    }
    
    // Try to re-init our network module if it fails, next time round it should be fine again...
    // Found: http://www.tweaking4all.com/hardware/arduino/arduino-enc28j60-ethernet/
    client.stop();
    //Enc28J60.init(mymac);
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