# Drying cabinet v2.0

[![Build Status](https://travis-ci.org/MarijnKoesen/dryingcab.svg?branch=master)](https://travis-ci.org/MarijnKoesen/dryingcab)

This is my drying cabinet / curing chamber or just call it connected fridge which keeps the 
temperature and humidity levels (inside the fridge) stable between some configured thresholds.

I use this cabinet to make my own cured ham & dried sausage.

This project is built on an Arduino Mega in combination with a DHT22 temperature/humidity sensor, 4 way relay 
and an ESP8266 (WiFi) adapter to create the connected fridge.

For more information on design (decisions) etc, check out src/dryingcab.ino's comment at the top.



## Result

![Result graph](https://github.com/MarijnKoesen/dryingcab/raw/master/doc/result-graph.png)

Here you can see some graphs from the fridge. 

Notice the temperature keeps stable between 13 and 15 degrees and the humidity
between 50 and 75 percent.

When the temperature gets too high, the fridge turns on, causing the humitity to 
drop. 

After the temperature is at the lower threshold (13), the fridge is turned off again and
the humidity starts to rise again, actually going a bit higher then we want. 

At this time the fan turns on, and blows out some of the humid air, stabalizing 
everything again. 

Now the temperature slowly rises (because it's hotter outside) until it gets above
the threshold again and the whole proces starts over.



## Connection diagram

![Connection diagram](https://github.com/MarijnKoesen/dryingcab/raw/master/doc/fritzing-schematic.png)
    


## Part list

- 1x Arduino Mega
- 1x DHT-22
- 1x [LM1117T-3.3](https://www.vanallesenmeer.nl/LM1117T-3.3-Spanningsregelaar%283.3v%29)
- 1x [20x4 LCD](https://www.vanallesenmeer.nl/LCD-Display-2004A-20x4-wit-op-blauw-)
- 1x [LCD I2C](https://www.vanallesenmeer.nl/LCD-I2C-interface-)
- 1x [ESP8266](https://www.vanallesenmeer.nl/ESP8266-Serial-Wireless-WIFI-Module)
- 1x [4 channel relay board](https://www.vanallesenmeer.nl/Relais-board-4-kanaals-5V-)
- 1x 10k resistor
- 2x 10uf cap

