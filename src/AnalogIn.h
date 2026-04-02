#ifndef ANALOG_IN_H
#define ANALOG_IN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAD_ANALOG_READ -1
#define AIN_COUNT 8

// Switch a pin between analog and digital mode at runtime.
// analog=true  -> ADC input
// analog=false -> GPIO input (call gpio_pin_configure separately for output)
int PinSetMode(int channel, bool analog);

// Read voltage in volts. Channel must be in analog mode.
int16_t AnalogRead(int channel);

// Digital read/write. Channel must be in digital mode.
int DigitalRead(int channel);
int DigitalWrite(int channel, int value);

#ifdef __cplusplus
}
#endif

#endif