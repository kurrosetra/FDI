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
void rgb_draw_pixel(int32_t x, int32_t y, uint8_t color);
void rgb_write_constrain(int32_t x, int32_t y, char c, uint8_t color, uint8_t fonstSize,
		int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax);
void rgb_write(int32_t x, int32_t y, char c, uint8_t color, uint8_t fontSize);
int32_t rgb_print_constrain(int32_t x, int32_t y, char* s, uint32_t size, uint8_t color,
		uint8_t fontSize, int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax);
void rgb_print(int32_t x, int32_t y, char* s, uint32_t size, uint8_t color, uint8_t fontSize);

#endif /* MATRIX_H_ */
