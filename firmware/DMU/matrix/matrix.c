/*
 * matrix.c
 *
 *  Created on: Sep 3, 2018
 *      Author: miftakur
 */

#include "matrix.h"
#include <string.h>
#include "font5x7.h"

volatile uint8_t activeBuffer = 0;
uint8_t frameBuffer[2][MATRIX_SCANROW][FRAME_SIZE];

void rgb_init()
{
	activeBuffer = 0;
	rgb_frame_clear(0);
	activeBuffer = 1;
	rgb_frame_clear(0);
	activeBuffer = 0;
}

void rgb_swap_buffer()
{
	if (activeBuffer)
		activeBuffer = 0;
	else
		activeBuffer = 1;
}

uint32_t rgb_get_buffer(uint8_t row)
{
	return (uint32_t) &frameBuffer[activeBuffer][row][0];
}

void rgb_frame_clear(uint8_t color)
{
	uint8_t bufferIndex = 0;
	if (activeBuffer)
		bufferIndex = 0;
	else
		bufferIndex = 1;

	uint8_t u8 = color;
	u8 |= color << 3;

	for ( int row = 0; row < MATRIX_SCANROW; row++ )
	{
		for ( int i = 0; i < FRAME_SIZE / 2; i++ )
		{
			frameBuffer[bufferIndex][row][i * 2] = u8;
			frameBuffer[bufferIndex][row][i * 2 + 1] = clk_Pin | u8;
		}
	}
}

void rgb_draw_pixel(int32_t x, int32_t y, uint8_t color)
{
	uint32_t row, lines;
	uint8_t rgbVal = 0;
	uint8_t bufferIndex = 0;

	if (activeBuffer)
		bufferIndex = 0;
	else
		bufferIndex = 1;

#if FRAME_ORIENTATION==FRAME_ORIENTATION_3
	x = MATRIX_MAX_WIDTH - 1 - x;
	y = MATRIX_MAX_HEIGHT - 1 - y;
#endif	//if FRAME_ORIENTATION==FRAME_ORIENTATION_3

#if (FRAME_ORIENTATION==FRAME_ORIENTATION_2)||(FRAME_ORIENTATION==FRAME_ORIENTATION_4)
	int32_t xy=x;

	x=y;
	y=xy;
#if FRAME_ORIENTATION==FRAME_ORIENTATION_4
	x = MATRIX_MAX_WIDTH - x;
	y = MATRIX_MAX_HEIGHT - y;
#endif	//elif FRAME_ORIENTATION==FRAME_ORIENTATION_4
#endif	//if (FRAME_ORIENTATION==FRAME_ORIENTATION_2)||(FRAME_ORIENTATION==FRAME_ORIENTATION_4)

	if ((x >= 0 && x < MATRIX_MAX_WIDTH) && (y >= 0 && y < MATRIX_MAX_HEIGHT))
	{
#if MATRIX_SCANROW==MATRIX_SCANROWS_16
		row = y % 16;
		lines = x;
#endif	//if MATRIX_SCANROW==MATRIX_SCANROWS_16
#if MATRIX_SCANROW==MATRIX_SCANROWS_8

#endif	//if MATRIX_SCANROW==MATRIX_SCANROWS_8

		/* TODO handle color*/
#if DISPLAY_P475
		rgbVal = color % 4;

		frameBuffer[bufferIndex][row][lines * 2] = rgbVal;
		frameBuffer[bufferIndex][row][lines * 2 + 1] = rgbVal | clk_Pin;
#endif	//if DISPLAY_P475
#if DISPLAY_P8
		rgbVal = color;

		frameBuffer[bufferIndex][row][lines * 2] |= rgbVal;
		frameBuffer[bufferIndex][row][lines * 2 + 1] |= rgbVal| clk_Pin;
#endif	//if DISPLAY_P8

	}
}

void rgb_write_constrain(int32_t x, int32_t y, char c, uint8_t color, uint8_t fontSize,
		int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax)
{
	uint8_t getFont[5];
	uint32_t fontPos = (uint32_t) c;
	uint32_t xPos, yPos;
	uint32_t xBit, yBit;
	uint32_t xSize, ySize;

	for ( xPos = 0; xPos < FONT_X_SIZE; xPos++ )
		getFont[xPos] = *(font5x7 + (fontPos * FONT_X_SIZE) + xPos);

	for ( xPos = 0; xPos < FONT_X_SIZE; xPos++ )
	{
		for ( yPos = 0; yPos < FONT_Y_SIZE; yPos++ )
		{
			if (bitRead(getFont[xPos], yPos))
			{
				for ( xSize = 0; xSize < fontSize; xSize++ )
				{
					for ( ySize = 0; ySize < fontSize; ySize++ )
					{
						yBit = y + (yPos * fontSize) + ySize;
						xBit = x + (xPos * fontSize) + xSize;
						if ((xBit >= xMin && xBit <= xMax) && (yBit >= yMin && yBit <= yMax))
						{
							rgb_draw_pixel(xBit, yBit, color);
						}
					}
				}
			}
		}
	}

#if USE_SMOOTH_FONT
	/* smoothing font when size>1 */
	if (fontSize > 1)
	{
		for ( xPos = 0; xPos < FONT_X_SIZE; xPos++ )
		{
			if (xPos == 1 || xPos == 3)
			{
				for ( yPos = 0; yPos < FONT_Y_SIZE; yPos++ )
				{
					if (bitRead(getFont[xPos], yPos))
					{
						//left
						if (!bitRead(getFont[xPos - 1], yPos))
						{
							//left-down
							if (bitRead(getFont[xPos - 1],
									yPos - 1) && !bitRead(getFont[xPos], yPos - 1))
							{
								for ( int i = 0; i < fontSize; i++ )
								{
									for ( xSize = 0; xSize < fontSize; xSize++ )
									{
										for ( ySize = 0; ySize < fontSize; ySize++ )
										{
											yBit = y + (yPos * fontSize) + ySize - i;
											xBit = x + (xPos * fontSize) + xSize - i;
											if ((xBit >= xMin && xBit <= xMax)
													&& (yBit >= yMin && yBit <= yMax))
											{
												rgb_draw_pixel(xBit, yBit, color);
											}
										}
									}
								}

							}
							//left-up
							if (bitRead(getFont[xPos - 1],
									yPos + 1) && !bitRead(getFont[xPos], yPos + 1))
							{
								for ( int i = 0; i < fontSize; i++ )
								{
									for ( xSize = 0; xSize < fontSize; xSize++ )
									{
										for ( ySize = 0; ySize < fontSize; ySize++ )
										{
											yBit = y + (yPos * fontSize) + ySize + i;
											xBit = x + (xPos * fontSize) + xSize - i;
											if ((xBit >= xMin && xBit <= xMax)
													&& (yBit >= yMin && yBit <= yMax))
											{
												rgb_draw_pixel(xBit, yBit, color);
											}
										}
									}
								}
							}
						}
						//right
						if (!bitRead(getFont[xPos + 1], yPos))
						{
							//right-down
							if (bitRead(getFont[xPos+1],yPos-1) && !bitRead(getFont[xPos], yPos - 1))
							{
								for ( int i = 0; i < fontSize; i++ )
								{
									for ( xSize = 0; xSize < fontSize; xSize++ )
									{
										for ( ySize = 0; ySize < fontSize; ySize++ )
										{
											yBit = y + (yPos * fontSize) + ySize - i;
											xBit = x + (xPos * fontSize) + xSize + i;
											if ((xBit >= xMin && xBit <= xMax)
													&& (yBit >= yMin && yBit <= yMax))
											{
												rgb_draw_pixel(xBit, yBit, color);
											}
										}
									}
								}
							}
							//right-up
							if (bitRead(getFont[xPos + 1],
									yPos + 1) && !bitRead(getFont[xPos], yPos + 1))
							{
								for ( int i = 0; i < fontSize; i++ )
								{
									for ( xSize = 0; xSize < fontSize; xSize++ )
									{
										for ( ySize = 0; ySize < fontSize; ySize++ )
										{
											yBit = y + (yPos * fontSize) + ySize + i;
											xBit = x + (xPos * fontSize) + xSize + i;
											if ((xBit >= xMin && xBit <= xMax)
													&& (yBit >= yMin && yBit <= yMax))
											{
												rgb_draw_pixel(xBit, yBit, color);
											}
										}
									}
								}

							}
						}
					}
				}
			}
		}
	}  // smoothing
#endif	//if USE_SMOOTH_FONT

}

void rgb_write(int32_t x, int32_t y, char c, uint8_t color, uint8_t fontSize)
{
	rgb_write_constrain(x, y, c, color, fontSize, 0, MATRIX_MAX_WIDTH - 1, 0,
	MATRIX_MAX_HEIGHT - 1);
}

void rgb_print(int32_t x, int32_t y, char* s, uint32_t size, uint8_t color, uint8_t fontSize)
{
	uint32_t rCharPos = 0;
	uint32_t nCharPos = 0;
	for ( uint32_t i = 0; i < size; i++ )
	{
		if (*(s + i) == '\r')
			rCharPos = i + 1;
		else if (*(s + i) == '\n')
		{
			nCharPos++;
			if (*(s + i - 1) == '\r')
				rCharPos = i + 1;
		}
		else
			rgb_write(x + ((i - rCharPos) * (FONT_X_SIZE * fontSize + fontSize)),
					y + (nCharPos * FONT_Y_SIZE * fontSize), *(s + i), color, fontSize);
	}
}

int32_t rgb_print_constrain(int32_t x, int32_t y, char* s, uint32_t size, uint8_t color,
		uint8_t fontSize, int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax)
{
	int32_t ret = 0;
	if (size)
	{
		uint32_t rCharPos = 0;
		uint32_t nCharPos = 0;
		for ( uint32_t i = 0; i < size; i++ )
		{
//			rgb_write_constrain(x + (i * (FONT_X_SIZE * fontSize + fontSize)), y, *(s + i), color,
//					fontSize, xMin, xMax, yMin, yMax);
			if (*(s + i) == '\r')
				rCharPos = i + 1;
			else if (*(s + i) == '\n')
			{
				nCharPos++;
				if (*(s + i - 1) == '\r')
					rCharPos = i + 1;
			}
			else
				rgb_write_constrain(x + ((i - rCharPos) * (FONT_X_SIZE * fontSize + fontSize)),
						y + (nCharPos * FONT_Y_SIZE * fontSize), *(s + i), color, fontSize, xMin,
						xMax, yMin, yMax);

		}
		ret = (int32_t)size * (FONT_X_SIZE * fontSize + fontSize) + x;
	}

	return ret;
}

