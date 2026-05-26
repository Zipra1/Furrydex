#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>

int initDisplay();
void Display(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, const unsigned char *frame_buffer);
void sendCommand(uint8_t byte);
void sendData(uint8_t byte);
void lcdReset(void);
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);


/** Wait until the LCD display sends a TE (new frame) interrupt signal */
void waitForTE(void);

/** Set the FPS of the LCD display
 *
 * @param fps Desired fps. Options:
 * 0.25, 0.5, 1, 2, 4, 8, 16, 25.5, 32, 51
 *
 * @return 0 if successful, otherwise negative error code.
 */
int setFPS(int fps);

#endif