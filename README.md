# Arduino Clock

This is a source code and schematics for a [two-display Arduino clock project](https://imgur.com/a/mQLIaO0).

![clock](https://raw.githubusercontent.com/unobtec/arduino-clock/master/images/clock.jpg)

# Components

1. Arduino Pro Micro (it is one of the smallest but still very capable boards to embed into your electronics projects, but you can use other types as well). I usually buy the electronic components from AliExpress. Just search for "Arduino Pro Micro 5V" there.

2. Two 0.96" 128x64 OLED displays with I2C interface (four wires). There are larger displays out there in the market as well, they all go by SSD1306 name. Search by "ssd1306 oled 128x64 i2c white" on AliExpress and check the photos of the back of the module. These displays can come with a slightly different PCB design; for this clock project, you will need the ones that have a resistor on the back that allows you to change the I2C device address (as shown on one of the photos in my Imgur post). Changing the address on one of the displays will allow you to connect both devices in parallel to the same Arduino board without any extra circuitry.

3. A rotary encoder (search for "rotary encoder with push button" on AliExpress). You will need just the encoder itself, not mounted to any PCB)

4. An optional switch to turn display on and off.

# Schematics

![schematics](https://raw.githubusercontent.com/unobtec/arduino-clock/master/images/schematics.png)

# Dependencies

You can install all dependencies via Arduino > Sketch > Include Library > Manage Libraries...

- `Encoder` library by Paul Stoffregen
- `ssd1306` library v1.7.0 by Alexey Dydna (later versions will require font recompiling)
- `Time` library by Michael Margolis

# Controlling the clock from your computer

The clock, when plugged into USB port of your computer, is recognized as a serial device whcih you can send commands to. Commands must be followed with either `<CR>`, `<LF>`, or `<CR><LF>` characters. Recognized commands are:

- `F24`: switch to a 24-hour format
- `F12`: switch to a 12-hour format
- `TYYYY-MM-DD HH:NN:SS`: set time. Example: `T2018-05-13 14:32:12`.

### Find your device:

```sh
$ ls /dev/cu.usbmodem*
```

If the modem is found, you will see something like `/dev/cu.usbmodem14121`. That's your device.

### Set the 24-hour format:

```sh
echo "F24" > /dev/cu.usbmodem14121
```

### Set current date/time:

```
date "+T%Y-%m-%d %H:%M:%S" > /dev/cu.usbmodem14121
```

See `bin/sync-clock` script that tries to do all of the above automatically.
