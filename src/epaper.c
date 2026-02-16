#include "epaper.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE (CONFIG_FURRYDEX_EPD_WIDTH * CONFIG_FURRYDEX_EPD_HEIGHT / 8)

#define DC0_NODE DT_ALIAS(dc0)
#define RST0_NODE DT_ALIAS(rst0)
#define CS0_NODE DT_ALIAS(cs0)
#define BUSY0_NODE DT_ALIAS(busy0)
#define SPI_NODE DT_NODELABEL(spi3)

unsigned char previous_frame_buffer[CONFIG_FURRYDEX_EPD_WIDTH * CONFIG_FURRYDEX_EPD_HEIGHT / 8] = {0}; // was in the middle of organizing code. move the rest of this shit to epaper.c thanks

static const struct gpio_dt_spec busy = GPIO_DT_SPEC_GET(BUSY0_NODE, gpios);
static const struct gpio_dt_spec dc = GPIO_DT_SPEC_GET(DC0_NODE, gpios);
static const struct gpio_dt_spec rst = GPIO_DT_SPEC_GET(RST0_NODE, gpios);
static const struct gpio_dt_spec cs = GPIO_DT_SPEC_GET(CS0_NODE, gpios);

static const struct device *spi_dev = DEVICE_DT_GET(SPI_NODE);

static struct spi_config spi_cfg = {
    .frequency = 25000000,                  // 25 MHz
    .operation = SPI_OP_MODE_MASTER |
                 SPI_TRANSFER_MSB |
                 SPI_WORD_SET(8) |
                 SPI_MODE_CPOL |
                 SPI_MODE_CPHA,
    .slave = 0,
};

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
	gpio_pin_set_dt(&cs, 1); //cs gets set to 1 because it is active low. todo: cs doesn't need to be manually set.
	spi_send_byte(byte);
	gpio_pin_set_dt(&cs, 0);
}

void sendData(uint8_t byte)
{
	gpio_pin_set_dt(&dc, 1);
	gpio_pin_set_dt(&cs, 1);
	spi_send_byte(byte);
	gpio_pin_set_dt(&cs, 0);
	gpio_pin_set_dt(&dc, 0);
}

void epdReset()
{
	gpio_pin_set_dt(&rst, 1);
	k_msleep(20);
	gpio_pin_set_dt(&rst, 0);
	k_msleep(20);
	gpio_pin_set_dt(&rst, 1);
	k_msleep(20);
}

void setWindows(unsigned int Xstart, unsigned int Ystart, unsigned int Xend, unsigned int Yend)
{
	sendCommand(0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
	sendData((Xstart>>3) & 0xFF);
	sendData((Xend>>3) & 0xFF);

	sendCommand(0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
	sendData(Ystart & 0xFF);
    sendData((Ystart >> 8) & 0xFF);
    sendData(Yend & 0xFF);
    sendData((Yend >> 8) & 0xFF);
}

void setCursor(unsigned int Xstart, unsigned int Ystart)
{
	sendCommand(0x4E); // SET_RAM_X_ADDRESS_COUNTER
    sendData((Xstart >> 3) & 0xFF);

    sendCommand(0x4F); // SET_RAM_Y_ADDRESS_COUNTER
    sendData(Ystart & 0xFF);
    sendData((Ystart >> 8) & 0xFF);
}

void waitUntilIdle()
{
	printf("busy\n");
	while (gpio_pin_get_dt(&busy)){
		k_msleep(4);
	}
	printf("busy over\n");
}

void epdSleep()
{
    sendCommand(0x10); //enter deep sleep
    sendData(0x01);
    k_msleep(200);

    gpio_pin_set_dt(&rst, 0);
}

int initDisplay(enum epd_mode mode)
{
	int ret;

	if (!gpio_is_ready_dt(&dc)) {
		return 0;
	}

	if (!gpio_is_ready_dt(&rst)) {
		return 0;
	}

	if (!gpio_is_ready_dt(&cs)) {
		return 0;
	}

	if (!gpio_is_ready_dt(&busy)) {
		return 0;
	}

	if (!device_is_ready(spi_dev)) {
    	return 0;
	}	

	ret = gpio_pin_configure_dt(&dc, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&rst, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&cs, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&busy, GPIO_INPUT);
	if (ret < 0) {
		return 0;
	}

	epdReset();

	if(mode == EPD_MODE_FULL){
		waitUntilIdle();
		sendCommand(0x12); // soft reset
		k_msleep(10);

		sendCommand(0x01); // Driver output control
		sendData((CONFIG_FURRYDEX_EPD_HEIGHT - 1) & 0xFF);
    	sendData(((CONFIG_FURRYDEX_EPD_HEIGHT - 1) >> 8) & 0xFF);
    	sendData(0x00);			// GD = 0; SM = 0; TB = 0;

		sendCommand(0x11); // data entry mode
		sendData(0x03); // changing this doesn't do anything?? 🥀

		waitUntilIdle();

		setWindows(0, 0, CONFIG_FURRYDEX_EPD_WIDTH - 1, CONFIG_FURRYDEX_EPD_HEIGHT - 1); // in datasheet, this is doing 0x44 and 0x45
		setCursor(CONFIG_FURRYDEX_CURSOR_START_X, CONFIG_FURRYDEX_CURSOR_START_Y); // wtf. this literally isnt doing anything ??

		sendCommand(0x3C); // BorderWaveform
		sendData(0x80); // try this as 0x80, too.

		// Skipping Step 4 here, mess with that later though. Temperature sensing, waveform LUT

		sendCommand(0x12); // what the fuck?
		sendData(0x00);
		sendData(0x80);

		waitUntilIdle(); // I don't know if I need this... Adding this in repsonse to the "what the fuck" command.
		
		sendCommand(0x18); // Read temp sensor
		sendData(0x80);
		waitUntilIdle();

		printf("Initialized FULL");
	}
	else if(mode == EPD_MODE_FAST){

	}
	else if(mode == EPD_MODE_PART){

	} else {
		return -1;
	}

	return 0;
}

void Display(const unsigned char* frame_buffer) // 2308ms
{
	int64_t start_time = k_uptime_get();

	struct spi_buf tx_buf = {
        .buf = (void *)frame_buffer,
        .len = CONFIG_FURRYDEX_EPD_MAX_BYTES
    };
    struct spi_buf_set tx_bufs = {
        .buffers = &tx_buf,
        .count = 1
    };

	struct spi_buf tx_buf_old = {
		.buf = (void *)previous_frame_buffer,
		.len = CONFIG_FURRYDEX_EPD_MAX_BYTES
	};
    struct spi_buf_set tx_bufs_old = {
		.buffers = &tx_buf_old,
		.count = 1
	};

	if (frame_buffer != NULL) {
        sendCommand(0x26);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs_old); 
		gpio_pin_set_dt(&cs, 0);
		gpio_pin_set_dt(&dc, 0);

        setCursor(CONFIG_FURRYDEX_CURSOR_START_X, CONFIG_FURRYDEX_CURSOR_START_Y);
        sendCommand(0x24);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs);
		gpio_pin_set_dt(&cs, 0);
    }
	int64_t duration = k_uptime_get() - start_time;
	//printf("EPD image upload took %lld ms\n", duration);

	sendCommand(0x22);
	sendData(0xf7);
	sendCommand(0x20);
	waitUntilIdle();
	
	duration = k_uptime_get() - start_time;
	printf("EPD full refresh took: %lld ms\n", duration);

	memcpy(previous_frame_buffer, frame_buffer, BUFFER_SIZE);
}

unsigned char EPD_2IN13_V4_LUT_PARTIAL[] = {
	0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x40,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x14,0x0,0x0,0x0,0x0,0x0,0x0,  
	0x1,0x0,0x0,0x0,0x0,0x0,0x0,
	0x1,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
	0x22,0x17,0x41,0x00,0x32,0x36,
};

void setPartialLUT(uint8_t speed) {
	EPD_2IN13_V4_LUT_PARTIAL[60] = speed;
    sendCommand(0x32); // Write LUT register
    for(int i=0; i < 153; i++) {
        sendData(EPD_2IN13_V4_LUT_PARTIAL[i]);
    }
	sendCommand(0x3f);
	sendData(*(EPD_2IN13_V4_LUT_PARTIAL+153));
	sendCommand(0x03);	// gate voltage
	sendData(*(EPD_2IN13_V4_LUT_PARTIAL+154));
	sendCommand(0x04);	// source voltage
	sendData(*(EPD_2IN13_V4_LUT_PARTIAL+155));	// VSH
	sendData(*(EPD_2IN13_V4_LUT_PARTIAL+156));	// VSH2
	sendData(*(EPD_2IN13_V4_LUT_PARTIAL+157));	// VSL
	sendCommand(0x2c);		// VCOM
	sendData(*(EPD_2IN13_V4_LUT_PARTIAL+158));
}

//static unsigned char scratch_buffer[CONFIG_FURRYDEX_EPD_MAX_BYTES];

/* full-partial refresh (broken, probably useless after adding custom luts)
void create_clean_buffer_static(const unsigned char* old_buffer, 
                                const unsigned char* new_buffer, 
                                unsigned char* out_buffer, 
                                size_t size) {
    for (size_t i = 0; i < size; i++) {
        // Logic: Force to white if both are currently black
        unsigned char both_black = ~(old_buffer[i] | new_buffer[i]);
        out_buffer[i] = new_buffer[i] | both_black;
    }
}

void displayFullPartial(const unsigned char* frame_buffer, uint8_t speed) // 1066ms // This breaks after like 20-ish uses, why? Fixes on reboot. Fascinating!
{
	int64_t start_time = k_uptime_get();

    struct spi_buf tx_buf = {
        .buf = (void *)frame_buffer,
        .len = CONFIG_FURRYDEX_EPD_MAX_BYTES
    };
    struct spi_buf_set tx_bufs = {
        .buffers = &tx_buf,
        .count = 1
    };

	create_clean_buffer_static(previous_frame_buffer, frame_buffer, scratch_buffer, CONFIG_FURRYDEX_EPD_MAX_BYTES);
	struct spi_buf tx_buf_empty = {
        .buf = (void *)scratch_buffer,
        .len = CONFIG_FURRYDEX_EPD_MAX_BYTES
    };

    struct spi_buf_set tx_bufs_empty = {
        .buffers = &tx_buf_empty,
        .count = 1
    };

	struct spi_buf tx_buf_old = {
		.buf = (void *)previous_frame_buffer,
		.len = CONFIG_FURRYDEX_EPD_MAX_BYTES
	};
    struct spi_buf_set tx_bufs_old = {
		.buffers = &tx_buf_old,
		.count = 1
	};

    if (frame_buffer != NULL) {
		setPartialLUT(speed);

		sendCommand(0x26);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs_old); 
		gpio_pin_set_dt(&cs, 0);
		gpio_pin_set_dt(&dc, 0);

        setCursor(CONFIG_FURRYDEX_CURSOR_START_X, CONFIG_FURRYDEX_CURSOR_START_Y);
        sendCommand(0x24);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs_empty);
		gpio_pin_set_dt(&cs, 0);

		sendCommand(0x22);
		sendData(0xC7); 
		sendCommand(0x20);
		waitUntilIdle();

        sendCommand(0x26);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs_empty); 
		gpio_pin_set_dt(&cs, 0);
		gpio_pin_set_dt(&dc, 0);

        setCursor(CONFIG_FURRYDEX_CURSOR_START_X, CONFIG_FURRYDEX_CURSOR_START_Y);
        sendCommand(0x24);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs);
		gpio_pin_set_dt(&cs, 0);
    }

    sendCommand(0x22);
    sendData(0xC7); 
    sendCommand(0x20);
    waitUntilIdle();

    int64_t total_duration = k_uptime_get() - start_time;
    printf("EPD fast refresh took %lld ms\n", total_duration);

    memcpy(previous_frame_buffer, frame_buffer, CONFIG_FURRYDEX_EPD_MAX_BYTES);
}
*/

void displayPartial(const unsigned char* frame_buffer, uint8_t speed) // 576ms
{
    int64_t start_time = k_uptime_get();

    struct spi_buf tx_buf = {
        .buf = (void *)frame_buffer,
        .len = CONFIG_FURRYDEX_EPD_MAX_BYTES
    };
    struct spi_buf_set tx_bufs = {
        .buffers = &tx_buf,
        .count = 1
    };

	struct spi_buf tx_buf_old = {
		.buf = (void *)previous_frame_buffer,
		.len = CONFIG_FURRYDEX_EPD_MAX_BYTES
	};
    struct spi_buf_set tx_bufs_old = {
		.buffers = &tx_buf_old,
		.count = 1
	};

    if (frame_buffer != NULL) {
		setPartialLUT(speed);

        sendCommand(0x26);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs_old); 
		gpio_pin_set_dt(&cs, 0);
		gpio_pin_set_dt(&dc, 0);

        setCursor(CONFIG_FURRYDEX_CURSOR_START_X, CONFIG_FURRYDEX_CURSOR_START_Y);
        sendCommand(0x24);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs);
		gpio_pin_set_dt(&cs, 0);
    }

    //int64_t upload_duration = k_uptime_get() - start_time;
    //printf("EPD image upload took %lld ms\n", upload_duration);

    sendCommand(0x22);
    sendData(0xC7); 
    sendCommand(0x20);
    memcpy(previous_frame_buffer, frame_buffer, BUFFER_SIZE);
    waitUntilIdle();

    int64_t total_duration = k_uptime_get() - start_time;
    printf("EPD faster refresh took %lld ms\n", total_duration);
}

void powerOnPumps() {
    sendCommand(0x22);
    sendData(0xC0); // Enable Clock and Analog (Pumps)
    sendCommand(0x20);
    waitUntilIdle();
    printf("EPD Pumps enabled");
}

void powerOffPumps() {
    sendCommand(0x22);
    sendData(0x03); // Disable Analog and Clock
    sendCommand(0x20);
    waitUntilIdle();
    printf("EPD Pumps disabled");
}

void displayPartialSequential(const unsigned char* frame_buffer, uint8_t speed) // Requires pumps to already be on // 373ms
{
    int64_t start_time = k_uptime_get();

    struct spi_buf tx_buf = {
        .buf = (void *)frame_buffer,
        .len = CONFIG_FURRYDEX_EPD_MAX_BYTES
    };
    struct spi_buf_set tx_bufs = {
        .buffers = &tx_buf,
        .count = 1
    };

	struct spi_buf tx_buf_old = {
		.buf = (void *)previous_frame_buffer,
		.len = CONFIG_FURRYDEX_EPD_MAX_BYTES
	};
    struct spi_buf_set tx_bufs_old = {
		.buffers = &tx_buf_old,
		.count = 1
	};

    if (frame_buffer != NULL) {
		setPartialLUT(speed);

        sendCommand(0x26);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs_old); 
		gpio_pin_set_dt(&cs, 0);
		gpio_pin_set_dt(&dc, 0);

        setCursor(CONFIG_FURRYDEX_CURSOR_START_X, CONFIG_FURRYDEX_CURSOR_START_Y);
        sendCommand(0x24);
        gpio_pin_set_dt(&dc, 1);
		gpio_pin_set_dt(&cs, 1);
        spi_write(spi_dev, &spi_cfg, &tx_bufs);
		gpio_pin_set_dt(&cs, 0);
    }

    //int64_t upload_duration = k_uptime_get() - start_time;
    //printf("EPD image upload took %lld ms\n", upload_duration);

    sendCommand(0x22);
    sendData(0x4); // mode 1 is a full refresh, so use mode 2
    sendCommand(0x20);
    memcpy(previous_frame_buffer, frame_buffer, BUFFER_SIZE);
    waitUntilIdle();

    int64_t total_duration = k_uptime_get() - start_time;
    printf("EPD fasterer refresh took %lld ms\n", total_duration);
}