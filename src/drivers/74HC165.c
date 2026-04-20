#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

static const struct device *spi_dev;
static bool sr165_initialized = false;

#define SHLD_NODE DT_ALIAS(hc165shld)
#define SR_NODE DT_NODELABEL(spi2)

static const struct gpio_dt_spec hc165shld = GPIO_DT_SPEC_GET(SHLD_NODE, gpios);
static const struct device *spi_dev = DEVICE_DT_GET(SR_NODE);

static struct spi_config spi_cfg = {
    .frequency = 1000000, // 1 MHz, should probably be 4MHz
    .operation = SPI_OP_MODE_MASTER |
                 SPI_TRANSFER_MSB |
                 SPI_WORD_SET(8) |
                 SPI_MODE_CPOL |
                 SPI_MODE_CPHA,
    .slave = 0,
};

static int sr165_init(void)
{
    int ret;

    if (sr165_initialized)
        return 0;

    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi2));

    if (!device_is_ready(spi_dev))
    {
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&hc165shld))
    {
        return 0;
    }

    ret = gpio_pin_configure_dt(&hc165shld, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return 0;
    }
    sr165_initialized = true;
    return 0;
}

int SR165Read(void)
{
    if (!sr165_initialized && sr165_init() != 0)
    {
        return -ENODEV;
    }

    gpio_pin_set_dt(&hc165shld, 0);
    k_busy_wait(1); // so short it doesn't rlly matter that it's a busy wait
    gpio_pin_set_dt(&hc165shld, 1);

    // SPI read 1 byte
    uint8_t rx_buf = 0;
    const struct spi_buf rx = {
        .buf = &rx_buf,
        .len = 1,
    };
    const struct spi_buf_set rx_set = {
        .buffers = &rx,
        .count = 1,
    };

    if (spi_read(spi_dev, &spi_cfg, &rx_set) != 0)
        return -EIO;

    return rx_buf;
}

int SR165ReadPin(int pin)
{
    if (pin < 0 || pin > 7)
        return -EINVAL;
    int val = SR165Read();
    if (val < 0)
        return val;
    return (val >> (7 - pin)) & 1;
}