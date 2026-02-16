#ifndef EPAPER_H
#define EPAPER_H

#include <stdint.h>
#include <stdbool.h>

enum epd_mode{
	EPD_MODE_FULL,
	EPD_MODE_FAST,
	EPD_MODE_PART,
};

int initDisplay(enum epd_mode mode);
void Display(const unsigned char* frame_buffer);
void displayPartial(const unsigned char* frame_buffer, uint8_t speed);
void displayPartialSequential(const unsigned char* frame_buffer, uint8_t speed);
void setCursor(unsigned int Xstart, unsigned int Ystart);
void setWindows(unsigned int Xstart, unsigned int Ystart, unsigned int Xend, unsigned int Yend);
void sendCommand(uint8_t byte);
void sendData(uint8_t byte);
void epdReset(void);
void waitUntilidle(void);
void powerOnPumps(void);
void powerOffPumps(void);
void epdSleep(void);

#endif