## What is a Furrydex?
Furrydex is like a "pokedex" for furries! Use it to know when other furrydex owners are nearby, and add furries to your furrydex by tapping two furrydexes together.

It's also a versatile hardware platform, for whatever you might want that for :3c
Currently, this is very early in development. It will be a *while* until its usable!

## Planned features are:
- [x] - 122x250 monochrome LCD screen (Should also support e-paper for DIY, since that is more easily accessible.)
- [ ] - Profiles: A shareable profile able to contain pictures, menus, and programs/files
- [ ] - Streetpass-like radio: When two furrydexes are near, it will notify both of them. (& if its a saved one or not!)
- [ ] - NFC (Tap two furrydexes together to "capture" each-others profiles! Or, use it directly with Lua)
- [ ] - Profile browser: Browse through all profiles
- [ ] - File explorer: Browse files on device and run scripts
- [x] - Lua interpreter to make easily shareable programs and interface with external/internal hardware
- [ ] - Module port for expanding functionality. Expose GPIOs, power, and SPI bus. Should allow the furrydex to be used as an mp3 player, phone, gps, and more.
- [ ] - Infrared transceiver (Easy and cheap to add, so why not)
- [ ] - SD card passthrough, for easy modification/programming and profile making. (Kinda implemented)

## Hardware
The nRF54LM20A is preferred, but the nRF52840 will have support as long as I dont reach its limitations.
External radio chips are being considered to expand functionality if there is space inside.

## Can I buy it?
Not yet! But I do plan to sell these eventually at https://www.macroplastics.ca
The price target is 50USD or lower.

## Can I make it?
It's not complete yet, but if you want to help development then yes!

Reccomended IDE for this is VSCode, since that's the officially used one for the nRF series.

You'll need the nRF Connect extension, opening this repo with that and opening the extension tab should allow you to add a build configuration.

The nRF54LM20DK is the preferred hardware right now, but the nRF52840 ProMicro will also work if you are on a budget of like $10. Check the device tree for pin assignments, I have yet to make a schematic.
