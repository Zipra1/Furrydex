## What is a Furrydex?
Furrydex is like a "pokedex" for furries! Use it to know when other furrydex owners are nearby, and exchange profiles.

It's also a versatile hardware platform, for whatever you might want that for :3c
Currently, this is very early in development. It will be a *while* until its usable!

## Planned features are:
- 122x250 monochrome LCD screen (Should also support e-paper for DIY, since that is more easily accessible.)
- Profiles: A shareable profile able to contain pictures, menus, and programs/files
- Streetpass-like radio: When two furrydexes are near, it will notify both of them. (& if its a saved one or not!)
- NFC (Tap two furrydexes together to "capture" each-others profiles! Or, use it directly with Furscript)
- Profile browser: Browse through all profiles
- File explorer: Browse files on device and run scripts
- Lua interpreter to make easily shareable programs and interface with external/internal hardware
- Two module ports: 1x internal & 1x external module ports for expanding functionality. Expose GPIOs, power, and SPI bus
- Infrared transceiver (Easy and cheap to add, so why not)
- SD card passthrough, for easy modification and profile making.

## Hardware
Based on the nRF52 series of microcontrollers and Zephyr.
I'd like the firmware to run on both nRF52833 and nRF52840, but nRF52833 may be dropped if its limits are reached.
nRF52 series is chosen due to very low power draw, native NFC, and 2.4GHz radio.

## Can I buy it?
Not yet! But I do plan to sell these eventually at https://www.macroplastics.ca
The price target is 50USD or lower.

## Can I make it?
Not yet! But I do plan for these to be DIY-friendly using off the shelf components.