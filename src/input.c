#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>

#include <inttypes.h>
#include <stdio.h>

#include "input.h"
#include "paint.h"
#include "luazephyrlib.h"
#include "drivers/74HC165.h"
#include "imgdata.h"

#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

static struct k_work_delayable button_work;

atomic_t inputs = ATOMIC_INIT(0);

static void button_work_handler(struct k_work *work)
{
    int local_inputs = SR165Read();
    if (local_inputs < 0)
    {
        printk("SR165 read failed\n");
        return;
    }

    int prev_inputs = atomic_get(&inputs);
    atomic_set(&inputs, local_inputs);

    if (local_inputs != prev_inputs)
    {
        k_sem_give(&input_sem);
    }

    if (local_inputs != 0)
    {
        k_work_reschedule(&button_work, K_MSEC(25));
    }
}

K_SEM_DEFINE(input_sem, 0, 1);
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // k_work_submit(&button_work);
    k_work_reschedule(&button_work, K_MSEC(10)); // wait for button to settle
}

static void input_thread(void *a, void *b, void *c)
{
    k_msleep(500);
    int ret;

    if (!gpio_is_ready_dt(&button))
    {
        printk("Error: button device %s is not ready\n",
               button.port->name);
        return;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret != 0)
    {
        printk("Error %d: failed to configure %s pin %d\n",
               ret, button.port->name, button.pin);
        return;
    }

    ret = gpio_pin_interrupt_configure_dt(&button,
                                          GPIO_INT_EDGE_BOTH);
    if (ret != 0)
    {
        printk("Error %d: failed to configure interrupt on %s pin %d\n",
               ret, button.port->name, button.pin);
        return;
    }

    k_work_init_delayable(&button_work, button_work_handler);
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);
    printk("Set up button at %s pin %d\n", button.port->name, button.pin);
}

int get_bit(unsigned char byte, int n)
{
    if (n < 0 || n > 7)
    {
        return -1;
    }

    return (byte >> n) & 0x01;
}

K_THREAD_DEFINE(input_tid, 1024, input_thread, NULL, NULL, NULL, 10, 0, 0);

// SYS_INIT(input_init2, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);