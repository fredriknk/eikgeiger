# EIKGEIGER
![Front](./PICTURES/BAKER_1946.png)

Eikgeiger opensource project for detection of background radiation in Norway. It is developed in cooperation with Eik Ideverksted at NMBU. Use the design for anything you like comercially and privately, but i would love if you fork it so i can see what it is used for.

It uses an[ESP32-WROOM-32E](./DOCUMENTATION/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf) wifi module both communication and MCU, and is mostly based on surface mount components.

Thanks to the DIYgeiger radmon project for inspiration [DIYGeiger RADMON + kit](https://sites.google.com/site/diygeigercounter/home/gk-radmon).

If you want to handsolder this, i really recommend to check out the awesome IBOM assembly utility (use it with a desktop as it sucks on mobile)
[Assembly IBOM](https://htmlpreview.github.io/?https://raw.githubusercontent.com/fredriknk/eikgeiger/main/PRODUCTION/ibom.html)

[SCHEMATIC](./DOCUMENTATION/Opengieger_schematic.pdf)
## Specifications
| **Parameter**   | **Description**                                                     |
|-----------------|---------------------------------------------------------------------|
| _Input voltage_ | Usb-C cable, Header 4-6v, Direct 3.3v                               |
| _Current_       | Wifi: 150-250mA, Radio Off: 44mA, Only HV psup 4mA, Deep sleep 70uA |
| _Communication_ | 2.4ghz WIFI, Uart TTL, Uart over USB                                |
| _Baudrate_      | 115200                                                              |
| _PCB size_      | 31x119mm                                                            |
| _Case size_     | 30x35x121mm                                                         |

## Serial port controll comands

To reconfigure device settings you need to connect the eikgeiger to a computer with a serial terminal (I reccomend to just use the [Arduino Program](https://www.arduino.cc/en/software), but any serial terminal should work)
When you want to configure a value you use two characters plus a number. So to set the PWM frequency to 2000hz, you would write PF2000 in the terminal and send. Some commands need a specific number, eg to factory reset you must transmit FR9999

| **Characters**| **Unit**                | **Value** | **Factory Value** | **Description**                                                                                                                                                                                                                                           | **Example** |
|---------------|-------------------------|-----------|-------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------|
| VO            | _VOLT (value/4096 %)_   | 0-2000    | 340               | Set pwm duty cycle, this is a value which controlls the duty cycle of HV power supply, it is a 12bit (4096) value, but you cant go over 50% or the coil field colapses. So keep it under 2046=50% duty cycle. (SBM-20 tubes like it best between 340-400) | VO340       |
| PF            | _FREQUENCY (hz)_        | 0-20000   | 1000              | Set pwm frequency, from experimentation i have found 1000hz to be most effective but it can make some coil noise                                                                                                                                          | PF1000      |
| SA            | _SAVE_                  | 1         | -                 | Saves parameters to eeprom                                                                                                                                                                                                                                | SA1         |
| RE            | _RESTART_               | 1         | -                 | Restarts device                                                                                                                                                                                                                                           | RE1         |
| FR            | _FACTORY RESET_         | 9999/1111 | -                 | Resets all values to factory default and restarts device, 1111 will reset all settings except wifi and radmon credentials, 9999 resets everything                                                                                                         | FR9999      |
| BE            | _BEEPER (activation)_   | 0-1       | 0                 | Activate beeper output 0=Off 1=On                                                                                                                                                                                                                         | BE1         |
| BF            | _BEEPER FREQUENCY (hz)_ | 10-40000  | 6000              | Beeper frequency 6000hZ is a very nice and annoying frequency                                                                                                                                                                                             | BF6000      |
| LE            | _LED (activation)_      | 0-1       | 0                 | Activate LED output 0=Off 1=On                                                                                                                                                                                                                            | LE1         |
| FL            | _FLASHTIME (mS)_        | 1-50      | 10                | How long the beeper beeps and led flashes in mS for every click.                                                                                                                                                                                          | FL10        |
| EV            | _EVENTS (number)_       | 0-1000    | 0                 | How many events for one flash, in practice it will divide by that number, so a value of 10, it will need 10 detections for every led and ticker flash.                                                                                                    | EV0         |

## Description
The pcb has one pinheader (J5) which can be used for 1 x i2c or 2 x Onewire io,
and one header (J10) with 1xGpio, and 1x mosfet open collector for eg a clicker.
A piezo buzzer with 5mm pitch can be directly soldered in the header holes.
It is allso possible to split out the 5v from the usb to insert an ebay generic lipo charger/powersuply to make it battery driven with usb charging.

It allso implements an integrated FTDI chip to get serial over USB, 
and a transistor based rts#/dtr# pulldown function for the boot pin on the ESP 
(shamelessly stolen from the weemos d1 mini design) 
so it allows for automatic programming over USB without pushing the boot pin.

## FRONT

![Front](./PICTURES/TOP.PNG)

## BACK

![Back](./PICTURES/BOTTOM.PNG)

## FIRST PRODUCTION PROTOTYPE
The jumper cable bugfix was taken care of in rev 0.2.1 (Purple PCB silkscreen)
![Front real](./PICTURES/TOP_900px.jpg)
![Back real](./PICTURES/BOTTOM_900px.jpg)
![Case Real](./PICTURES/CASE_900px.jpg)
## BUCK BOOST CONVERTER
![HV SOURCE](./PICTURES/HV-SOURCE.PNG)

It is designed for SBM-20 GM tube integrated onto the PCB with 6.3mm fuse holders, but it can use any tube up to 1000v if you just solder on some leads. The power supply is a standard buck boost, but with a 4x voltage multiplier output. It seems reaonably of effective, and consumes about 4ma@3.3v delivering 350v out running at 1000hz pwm. It has a voltage feedback read by adc2 on the esp32 module, but from experimentation i found it more stable to just set a pwm level rather than actively regulating it

![SBM DATASHEET](./PICTURES/SBM-20.jpg)

