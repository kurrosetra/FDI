/*
 * matrix.h
 *
 *  Created on: Sep 3, 2018
 *      Author: Miftakur
 */

#ifndef MATRIX_H_
#define MATRIX_H_

#include "matrix_config.h"

#define USE_SMOOTH_FONT			1

//volatile uint8_t activeBuffer = 0;
//uint8_t frameBuffer[2][MATRIX_SCANROW][FRAME_SIZE];

void rgb_init();

void rgb_swap_buffer();
uint32_t rgb_get_buffer(uint8_t row);

void rgb_frame_clear(uint8_t color);
void rgb_draw_pixel(int16_t x, int16_t y, uint8_t color);
void rgb_write_constrain(int16_t x, int16_t y, char c, uint8_t color, uint8_t fonstSize,
		int16_t xMin, int16_t xMax, int16_t yMin, int16_t yMax);
void rgb_write(int16_t x, int16_t y, char c, uint8_t color, uint8_t fontSize);
int16_t rgb_print_constrain(int16_t x, int16_t y, char* s, uint16_t size, uint8_t color,
		uint8_t fontSize, int16_t xMin, int16_t xMax, int16_t yMin, int16_t yMax);
void rgb_print(int16_t x, int16_t y, char* s, uint16_t size, uint8_t color, uint8_t fontSize);

#endif /* MATRIX_H_ */
