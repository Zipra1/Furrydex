#include "AnalogIn.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

static const uint8_t ain_to_pin[AIN_COUNT] = { 2, 3, 4, 5, 28, 29, 30, 31 };

static const struct device *adc_dev;
static const struct device *gpio0_dev;
static bool adc_initialized[AIN_COUNT] = {false};
static bool pin_is_analog[AIN_COUNT] = {false};
static int16_t sample_buf;

#define ADC_RESOLUTION       10
#define ADC_GAIN             ADC_GAIN_1_6
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)

int PinSetMode(int channel, bool analog)
{
    if (channel < 0 || channel >= AIN_COUNT) return -EINVAL;

    if (!adc_dev)   adc_dev   = DEVICE_DT_GET(DT_ALIAS(adcctrl));
    if (!gpio0_dev) gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (analog) {
        gpio_pin_configure(gpio0_dev, ain_to_pin[channel], GPIO_DISCONNECTED);

        if (!adc_initialized[channel]) {
            if (!device_is_ready(adc_dev)) {
                printk("ADC device not ready\n");
                return -ENODEV;
            }
            struct adc_channel_cfg cfg = {
                .gain             = ADC_GAIN,
                .reference        = ADC_REFERENCE,
                .acquisition_time = ADC_ACQUISITION_TIME,
                .channel_id       = channel,
                .differential     = 0,
            };
            int ret = adc_channel_setup(adc_dev, &cfg);
            printk("adc_channel_setup(%d) = %d\n", channel, ret);
            if (ret != 0) return ret;
            adc_initialized[channel] = true;
        }
    } else {
        gpio_pin_configure(gpio0_dev, ain_to_pin[channel], GPIO_INPUT);
    }

    pin_is_analog[channel] = analog;
    return 0;
}

int16_t AnalogRead(int channel)
{
    if (channel < 0 || channel >= AIN_COUNT) return BAD_ANALOG_READ;
    if (!pin_is_analog[channel]) return BAD_ANALOG_READ;

    const struct adc_sequence sequence = {
        .channels    = BIT(channel),
        .buffer      = &sample_buf,
        .buffer_size = sizeof(sample_buf),
        .resolution  = ADC_RESOLUTION,
    };

    if (adc_read(adc_dev, &sequence) != 0) return BAD_ANALOG_READ;

    return sample_buf;
}

int DigitalRead(int channel)
{
    if (channel < 0 || channel >= AIN_COUNT) return -EINVAL;
    if (pin_is_analog[channel]) return -EINVAL;
    return gpio_pin_get(gpio0_dev, ain_to_pin[channel]);
}

int DigitalWrite(int channel, int value)
{
    if (channel < 0 || channel >= AIN_COUNT) return -EINVAL;
    if (pin_is_analog[channel]) return -EINVAL;
    return gpio_pin_set(gpio0_dev, ain_to_pin[channel], value);
}