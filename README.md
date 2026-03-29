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
- [ ] - Two module ports: 1x internal & 1x external module ports for expanding functionality. Expose GPIOs, power, and SPI bus.
- [ ] - Infrared transceiver (Easy and cheap to add, so why not)
- [ ] - SD card passthrough, for easy modification/programming and profile making.

## Hardware
The nRF52840 is currently being used for prototyping, but the final version(s) will probably be using the nRF54LM20A since it's better in every way

## Can I buy it?
Not yet! But I do plan to sell these eventually at https://www.macroplastics.ca
The price target is 50USD or lower.

## Can I make it?
It's not nearly complete yet, but if you want to help development then yes!

Reccomended IDE for this is VSCode, since that's the officially used one for the nRF series.

You'll need the nRF Connect extension, opening this repo with that and opening the extension tab should allow you to add a build configuration.

The only board present at the moment is the ProMicro nRF52840: you can get these for very cheap on aliexpress. But get a few, since DOA's are common for such cheap boards. Check the device tree for pin assignments.
> No i dont have a devkit ( •̀ ω •́ )↗
