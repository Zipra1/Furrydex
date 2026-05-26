#include "lcd.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <stdio.h>
#include <stdlib.h>

/*
This driver is for a ST7305 display. If you have a display of a different chip, you will need a different driver.
All you will need is a way to send the frame buffer, as well as configure any hardware-level settings (ex. refresh rate, power mode, anti-tear pin)
paint.c takes care of actual rendering and is display-agnostic.
*/

#define USE_HORIZONTAL 0

#define DC0_NODE DT_ALIAS(dc0)
#define RST0_NODE DT_ALIAS(rst0)
#define CS0_NODE DT_ALIAS(cs0)
#define TE0_NODE DT_ALIAS(te0)
#define SPI_NODE DT_NODELABEL(spi3)

static const struct gpio_dt_spec te = GPIO_DT_SPEC_GET(TE0_NODE, gpios);
static const struct gpio_dt_spec dc = GPIO_DT_SPEC_GET(DC0_NODE, gpios);
static const struct gpio_dt_spec rst = GPIO_DT_SPEC_GET(RST0_NODE, gpios);
static const struct gpio_dt_spec cs = GPIO_DT_SPEC_GET(CS0_NODE, gpios);

static const struct device *spi_dev = DEVICE_DT_GET(SPI_NODE);

static struct spi_config spi_cfg = {
    .frequency = 40000000, // 40 MHz
    .operation = SPI_OP_MODE_MASTER |
                 SPI_TRANSFER_MSB |
                 SPI_WORD_SET(8) |
                 SPI_MODE_CPOL |
                 SPI_MODE_CPHA,
    .slave = 0,
};

static struct gpio_callback te_cb_data;

static K_MUTEX_DEFINE(te_mutex);
static K_CONDVAR_DEFINE(te_condvar);

static void te_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_condvar_broadcast(&te_condvar);
}

void waitForTE(void)
{
    k_mutex_lock(&te_mutex, K_FOREVER);
    k_condvar_wait(&te_condvar, &te_mutex, K_FOREVER);
    k_mutex_unlock(&te_mutex);
}

void spi_send_byte(uint8_t byte)
{
    struct spi_buf tx_buf = {
        .buf = &byte,
        .len = 1,
    };

    struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1,
    };

    spi_write(spi_dev, &spi_cfg, &tx);
}

void sendCommand(uint8_t byte)
{
    gpio_pin_set_dt(&dc, 0);
    gpio_pin_set_dt(&cs, 1); // cs gets set to 1 because it is active low. todo: cs doesn't need to be manually set.
    spi_send_byte(byte);
    gpio_pin_set_dt(&cs, 0);
    gpio_pin_set_dt(&dc, 1);
}

void sendData(uint8_t byte)
{
    gpio_pin_set_dt(&cs, 1);
    spi_send_byte(byte);
    gpio_pin_set_dt(&cs, 0);
}

void lcdReset()
{
    // gpio_pin_set_dt(&rst, 1);
    k_msleep(100);
    gpio_pin_set_dt(&rst, 0);
    k_msleep(100);
    gpio_pin_set_dt(&rst, 1);
}

void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) // why are these u16?
{
    sendCommand(0x2a);
    sendData(x1);
    sendData(x2);
    sendCommand(0x2b);
    sendData(y1);
    sendData(y2);
    sendCommand(0x2c);
}

int initDisplay()
{
    int ret;

    if (!gpio_is_ready_dt(&dc))
    {
        return 0;
    }

    if (!gpio_is_ready_dt(&rst))
    {
        return 0;
    }

    if (!gpio_is_ready_dt(&cs))
    {
        return 0;
    }

    if (!gpio_is_ready_dt(&te))
    {
        return 0;
    }

    if (!device_is_ready(spi_dev))
    {
        return 0;
    }

    ret = gpio_pin_configure_dt(&dc, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return 0;
    }

    ret = gpio_pin_configure_dt(&rst, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return 0;
    }

    ret = gpio_pin_configure_dt(&cs, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return 0;
    }

    ret = gpio_pin_configure_dt(&te, GPIO_INPUT);
    if (ret < 0)
    {
        return 0;
    }

    ret = gpio_pin_interrupt_configure_dt(&te, GPIO_INT_EDGE_RISING);
    if (ret < 0)
    {
        return 0;
    }

    gpio_init_callback(&te_cb_data, te_isr, BIT(te.pin));
    gpio_add_callback(te.port, &te_cb_data);

    lcdReset();

    sendCommand(0XD6); // NVM Load Control
    sendData(0X17);
    sendData(0X02);

    sendCommand(0xD1); // Auto Power Control
    sendData(0x01);

    sendCommand(0xC0); // Gate Voltage Setting VGH=12V ; VGL=-5V
    sendData(0x11);
    sendData(0x04);

    sendCommand(0xC1); // VSH Setting
    sendData(0X41);    //
    sendData(0X41);
    sendData(0X41);
    sendData(0X41);

    sendCommand(0xC2); // VSL Setting VSL=0
    sendData(0x19);
    sendData(0x19);
    sendData(0x19);
    sendData(0x19);

    sendCommand(0XC4); // VSHN Setting
    sendData(0X41);    // VSHN1=-3.8V
    sendData(0X41);    // VSHN2=-3.8V
    sendData(0X41);    // VSHN3=-3.8V
    sendData(0X41);    // VSHN4=-3.8V

    sendCommand(0XC5); // VSLN Setting
    sendData(0X19);    // VSLN1=0.5V
    sendData(0X19);    // VSLN2=0.5V
    sendData(0X19);    // VSLN3=0.5V
    sendData(0X19);    // VSLN4=0.5V

    sendCommand(0XD8); // OSC Setting
    sendData(0XA6);
    sendData(0XE9);

    //    sendCommand(0xCB);//VCOMH Setting
    //    sendData(0x14);//14  0C   7

    sendCommand(0XB2); // Frame Rate Control
    sendData(0X05);    // HPM=16hz ; LPM=8hz
    // 0x15 for High Frame Rate mode
    // This should be live configurable. Find out what's going on here

    sendCommand(0XB3); // Update Period Gate EQ Control in HPM
    sendData(0XE5);    // Gate EQ on
    sendData(0XF6);    // HPM EQ Control
    sendData(0X05);
    sendData(0X46);
    sendData(0X77);
    sendData(0X77);
    sendData(0X77);
    sendData(0X77);
    sendData(0X76);
    sendData(0X45);

    sendCommand(0XB4); // Update Period Gate EQ Control in LPM
    sendData(0X05);    // LPM EQ Control
    sendData(0X46);
    sendData(0X77);
    sendData(0X77);
    sendData(0X77);
    sendData(0X77);
    sendData(0X76);
    sendData(0X45);

    sendCommand(0X62); // Gate Timing Control
    sendData(0X32);
    sendData(0X03);
    sendData(0X1F);

    sendCommand(0XB7); // Source EQ Enable
    sendData(0X13);

    sendCommand(0xB0); // Duty Setting
    sendData(0x64);    // 250duty/4=63

    sendCommand(0x11); // Sleep out
    k_msleep(100);     // delay_ms 100ms

    sendCommand(0XC9); // Source Voltage Select
    sendData(0X00);    // VSHP1; VSLP1 ; VSHN1 ; VSLN1

    sendCommand(0x36); // Memory Data Access Control
    if (USE_HORIZONTAL == 0)
        sendData(0x48);
    else if (USE_HORIZONTAL == 1)
        sendData(0x4C);

    sendCommand(0x3A); // Data Format Select 4 write for 24 bit
    sendData(0x11);
    sendCommand(0xB9); // Source Setting
    sendData(0x20);
    sendCommand(0xB8); // Panel Setting Frame inversion
    sendData(0x29);

    sendCommand(0x21); // Display inversion on (0x20 for off)

    // sendCommand(0x2A);////Column Address Setting S61~S182
    // sendData(0x05);
    // sendData(0x36);
    // sendCommand(0x2B);////Row Address Setting G1~G250
    // sendData(0x00);
    // sendData(0xC7);

    sendCommand(0X35); // TE
    sendData(0X00);

    sendCommand(0xD0);
    sendData(0xFF);

    sendCommand(0x29); // Display on

    // sendCommand(0x39); // LPM

    k_msleep(120);
    return 0;
}

// void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color) // why is this u16?
// //                      5              0             55           200
// {
//     uint16_t i, j;
//     LCD_Address_Set(xsta, ysta, xend - 1, yend - 1); // ������ʾ��Χ
//     for (i = ysta; i < yend; i++)
//     {
//         for (j = xsta; j < xend * 3; j++)
//         {
//             sendData(color);
//             k_msleep(10);
//         }
//     }
// }

void Display(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, const unsigned char *frame_buffer) // 2308ms
{
    // int64_t start_time = k_uptime_get();

    struct spi_buf tx_buf = {
        .buf = (void *)frame_buffer,
        .len = CONFIG_FURRYDEX_FRAME_BYTES_DISPLAY};
    struct spi_buf_set tx_bufs = {
        .buffers = &tx_buf,
        .count = 1};

    LCD_Address_Set(25, 0, 35, 124);

    // int64_t duration = k_uptime_get() - start_time;
    gpio_pin_set_dt(&cs, 1);
    spi_write(spi_dev, &spi_cfg, &tx_bufs);
    gpio_pin_set_dt(&cs, 0);

    // duration = k_uptime_get() - start_time;
    // printf("LCD frame took: %lld ms\n", duration);
}

#define ST7305_CMD_HPM 0x38    /* High Power Mode ON  (§8.1.22) */
#define ST7305_CMD_LPM 0x39    /* Low Power Mode ON   (§8.1.23) */
#define ST7305_CMD_FRCTRL 0xB2 /* Frame Rate Control  (§8.2.3)  */

int setFPS(int fps)
{
    if (fps == 25) // 0.25Hz
    {
        sendCommand(ST7305_CMD_LPM);
        
        sendCommand(0xD8); // OSC setting
        sendData(0xA6);
        sendData(0xE9);
    }
}