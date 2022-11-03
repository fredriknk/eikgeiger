# EIKGEIGER

The first official EIK IOT enabled geigercounter based on an [ESP32-WROOM-32E](./DOCUMENTATION/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf) 
and the [DIYGeiger RADMON + kit](https://sites.google.com/site/diygeigercounter/home/gk-radmon).

The geigercounter uses an ESP32 as both communication and MCU, it will be programmed with a 3.3V TTL programmer and will be powered from etiher USB-C, 3.5-6V input or 3.3V direct input.


[PCB layout](./DOCUMENTATION/opengeiger_ROUND-Assembly.pdf)

[SCHEMATIC](./DOCUMENTATION/Opengieger_schematic.pdf)


## FRONT

![Front](./PICTURES/TOP.PNG)

## BACK

![Back](./PICTURES/BOTTOM.PNG)

## BUCK BOOST CONVERTER
![Front](./PICTURES/HV-SOURCE.PNG)

The geiger counter circuit is a standard [Buck Boost converter](https://en.wikipedia.org/wiki/Buck%E2%80%93boost_converter) using the MCU as an unregulated PWM, controller. The boost converter
utilizing high voltage tolerant components and should be able to reach about 900v, but it wont be able to supply any current at that voltage. But it will be enough for a GM-Tube.

The geiger counter will use a SBM-20 tube as the detecting element, which has a nominal voltage of 400V

![Front](./PICTURES/SBM-20.jpg)

