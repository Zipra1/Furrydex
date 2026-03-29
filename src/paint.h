#ifndef PAINT_H
#define PAINT_H

#include <stdint.h>
#include <stdbool.h>

void paintCharacter(char character, unsigned char *buf, int translate_width, int translate_height);
void paintText(unsigned char *buf, int kerning, int translate_width, int translate_height, const char *string);
int paintTextWrap(unsigned char *buf, int kerning, int translate_width, int translate_height, int box_width, const char *string);
void invert(uint8_t *buf, size_t size);
void invertRegion(unsigned char *buf, int buf_w, int buf_h, int start_x, int start_y, int end_x, int end_y);
void paintPixel(uint8_t *buf, int buf_w, int buf_h, int x, int y, int colour);
void paintLine(unsigned char *buf, int buf_w, int buf_h, int x0, int y0, int x1, int y1, int colour);
void paintRegion(uint8_t *buf, int buf_w, int buf_h, int start_x, int start_y, int end_x, int end_y, int colour);
void convertBuffer(uint8_t *buffer, uint8_t *target_buffer);
void FlipBuffer(unsigned char *buf, int physical_width, int height, bool flip_h, bool flip_v);
void paintFilledCircle(uint8_t *buf, int buf_w, int buf_h, int cx, int cy, int r, int colour);
void paintPageBubbles(uint8_t *buf, int buf_w, int buf_h, int num_bubble, int selected_bubble);
void blit(uint8_t *dst, int dst_w, int dst_h, const uint8_t *src, int src_w, int src_h, int x, int y);
void blitMask(uint8_t *dst, int dst_w, int dst_h, const uint8_t *src, int src_w, int src_h, const uint8_t *mask, int x, int y);

#endif