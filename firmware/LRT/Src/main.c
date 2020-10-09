/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
#define USE_EEPROM_FUNCTION		0
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm_hal_serial.h"
#include "matrix_config.h"
#include "matrix.h"

#if USE_EEPROM_FUNCTION==1
#include "eeprom.h"
#endif	//if USE_EEPROM_FUNCTION==1

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define DISPLAY_TEXT_MAX_SIZE			384
#define DISPLAY_TEXT_MIN_SIZE			128
typedef enum
{
	st_to_default,
	st_default,
	st_block,
	st_ops
} StateMachineBit_e;

typedef enum
{
	anim_to_start,
	anim_start_head_anim,
	anim_head_anim,
	anim_running,
	anim_start_tail_anim,
	anim_tail_anim,
	anim_end,
	anim_check_new
} StateAnimation_e;

typedef enum
{
	HT_NONE = 0,
	HT_BLINK,
	HT_BLANK
} HeadAndTailAnimation_e;

typedef enum
{
	SD_NONE = 0,
	SD_LEFT_TO_RIGHT,
	SD_RIGHT_TO_LEFT,
	SD_DOWNWARD,
	SD_UPWARD
} ScollingDirection_e;

typedef enum
{
	TA_LEFT = 0,
	TA_RIGHT,
	TA_CENTER
} TextAlignment_e;

typedef enum
{
	VA_CENTER = 0,
	VA_UP
} VerticalAlignment_e;

typedef enum
{
	PB_Color_bit = 0,
	PB_Fontsize_bit = 3,
	PB_Default_bit = 5,
	PB_Timeout_bit = 6,
	PB_HeadAnim_bit = 0,
	PB_TailAnim_bit = 3,
	PB_Alignment_bit = 6,
	PB_Direction_bit = 8,
	PB_VAlignment_bit = 12
} ParamBits_e;

typedef struct
{
	StateAnimation_e state;

	HeadAndTailAnimation_e headAnimation;
	HeadAndTailAnimation_e tailAnimation;
	TextAlignment_e alignment;
	VerticalAlignment_e vAlign;
	ScollingDirection_e direction;

	int32_t currentX;
	int32_t currentY;
	uint32_t timeout;
} Animation_t;

typedef enum
{
	DISPLAY_TEXT = 0,
	DISPLAY_BLOCK,
	DISPLAY_RESV
} DisplayMode_e;

typedef struct
{
	DisplayMode_e type;
	uint8_t fontSize;
	uint8_t color;
	bool defaultMode;
	char msg[DISPLAY_TEXT_MAX_SIZE];
	uint32_t timeout;
	Animation_t animation;
} DisplayText_t;

typedef struct
{
	bool newTextAvailable;
	uint8_t state;

	DisplayText_t activeText;
	DisplayText_t newText;
	DisplayText_t DefaultText;
} RText_t;

typedef struct
{
	char buf[10][DISPLAY_TEXT_MIN_SIZE];
	uint8_t bufPointer;
	uint8_t maxBuf;
} FlipTextBuffer_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBUG					0
#if DEBUG==1
uint16_t fps = 0;
#endif	//if DEBUG==1

#define SW_VERSION				"v1.2.1b"

#define CMD_BUFSIZE				512
#define TEXT_COLOR				0b001

#define CMD_TIMEOUT				35000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
IWDG_HandleTypeDef hiwdg;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
DMA_HandleTypeDef hdma_tim1_up;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
/* TODO Global Variables*/
Ring_Buffer_t tx1Buffer = { { 0 }, 0, 0 };
Ring_Buffer_t rx1Buffer = { { 0 }, 0, 0 };
TSerial serial = { &rx1Buffer, &tx1Buffer, &huart1 };

volatile uint8_t swapBufferStart = 0;
volatile uint32_t activeFrameRowAdress = 0;
volatile uint32_t matrix_row = 0;
volatile uint8_t deactiveMatrix = 0;

char inCmd[CMD_BUFSIZE];
FlipTextBuffer_t flipTextBuffer;

RText_t rText;

#if USE_EEPROM_FUNCTION==1
const uint16_t START_EE_ADDRESS = 0;
const uint32_t FORMATED_BYTES = 0x6B757200;
const uint8_t EE_DATA_MAX_SIZE = DISPLAY_TEXT_MAX_SIZE / 4 + 1;
#endif	//if USE_EEPROM_FUNCTION==1

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */
static void startDmaTransfering();
static void dmaTransferCompleted(DMA_HandleTypeDef *hdma);

static HAL_StatusTypeDef parsingCommand(char *cmd);
static void commandHandler();
static void commandInit();
#if USE_EEPROM_FUNCTION==1
static void commandEEInit();
static void commandEESave(uint8_t color, uint8_t fontSize, char *buf);
#endif	//if USE_EEPROM_FUNCTION==1

static void displayHandler();
static void parsingFlipText();

static int indexOfStr(char *from, const char *str, uint32_t fromIndex);
static int indexOfChr(char *from, const char ch, uint32_t fromIndex);
static void substring(char *from, char *to, uint32_t left, uint32_t right);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_TIM1_Init();
	MX_TIM2_Init();
	MX_USART1_UART_Init();
	MX_IWDG_Init();
	/* USER CODE BEGIN 2 */
	/* TODO Initialization*/
	HAL_IWDG_Refresh(&hiwdg);

	/* init data */
	rgb_init();
	activeFrameRowAdress = rgb_get_buffer(0);

	/* enable tim1 dma */
	__HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);
	hdma_tim1_up.XferCpltCallback = dmaTransferCompleted;
	/* start timer 1 */
	HAL_TIM_Base_Start(&htim1);

	/* enable tim2 */
	HAL_TIM_Base_Start_IT(&htim2);

	HAL_IWDG_Refresh(&hiwdg);

	rgb_frame_clear(0);
	char headerStart[32];
	char version[32];
	uint16_t versionLen = sprintf(version, "%s", SW_VERSION);
	uint16_t headerLen = sprintf(headerStart, "RESPATI");
	uint8_t headerSize = 1;
	uint8_t headerColor = TEXT_COLOR;
//	rgb_print((MATRIX_MAX_WIDTH - 6 * headerSize * headerLen) / 2, 0, headerStart, headerLen,
//			headerColor, headerSize);
	rgb_print(0, 0, headerStart, headerLen, headerColor, headerSize);
	rgb_print(0, 8, SW_VERSION, versionLen, headerColor, headerSize);
	swapBufferStart = 1;

	for ( uint8_t i = 0; i < 5; i++ )
	{
		HAL_IWDG_Refresh(&hiwdg);
		HAL_Delay(100);
	}
	rgb_frame_clear(0);
	swapBufferStart = 1;

	while (swapBufferStart != 0)
		;

#if DEBUG==1
	char buf[DISPLAY_TEXT_MIN_SIZE];
	uint16_t bufLen;
	sprintf(rText.activeText.msg, "asu#tokek#kadal#babi");
	char *str = rText.activeText.msg;
	FlipTextBuffer_t *fB = &flipTextBuffer;

	parsingFlipText();

	rgb_frame_clear(0);
	rgb_print(0, 0, str, strlen(str), TEXT_COLOR, 1);

	bufLen = sprintf(buf, "%d %s %s %s %s", fB->maxBuf, fB->buf[0], fB->buf[1], fB->buf[2],
			fB->buf[fB->maxBuf - 1]);
	rgb_print(0, 8, buf, bufLen, TEXT_COLOR, 1);
	swapBufferStart = 1;

	while (1)
	HAL_IWDG_Refresh(&hiwdg);

	uint32_t _ms = 0, fpsTimer = 0;
	while (1)
	{
		_ms = HAL_GetTick();
		HAL_IWDG_Refresh(&hiwdg);

		if (_ms >= fpsTimer)
		{
			fpsTimer = _ms + 1000;

			rgb_frame_clear(0);
			headerLen = sprintf(headerStart, "f= %d", fps);
			fps = 0;
			rgb_print(0, 0, headerStart, headerLen, TEXT_COLOR, 2);
			headerLen = sprintf(headerStart, "%lu", htim2.Init.Period);
			rgb_print(64, 16, headerStart, headerLen, TEXT_COLOR, 2);
			swapBufferStart = 1;

		}
	}
#endif	//if DEBUG==1

	commandInit();
	serial_init(&serial);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		HAL_IWDG_Refresh(&hiwdg);
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		commandHandler();
		displayHandler();
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
			| RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief IWDG Initialization Function
 * @param None
 * @retval None
 */
static void MX_IWDG_Init(void)
{

	/* USER CODE BEGIN IWDG_Init 0 */

	/* USER CODE END IWDG_Init 0 */

	/* USER CODE BEGIN IWDG_Init 1 */

	/* USER CODE END IWDG_Init 1 */
	hiwdg.Instance = IWDG;
	hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
	hiwdg.Init.Reload = IWDG_1S;
	if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN IWDG_Init 2 */

	/* USER CODE END IWDG_Init 2 */

}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void)
{

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = CLK_PSC;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = CLK_ARR;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = OE_PSC;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = OE_ARR;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = CMD_BAUD;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/** 
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE()
	;

	/* DMA interrupt init */
	/* DMA1_Channel5_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE()
	;
	__HAL_RCC_GPIOD_CLK_ENABLE()
	;
	__HAL_RCC_GPIOA_CLK_ENABLE()
	;
	__HAL_RCC_GPIOB_CLK_ENABLE()
	;

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, r1_Pin | g1_Pin | b1_Pin | r2_Pin | g2_Pin | b2_Pin | clk_Pin | oe_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, da_Pin | db_Pin | dc_Pin | dd_Pin | stb_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : LED_BUILTIN_Pin */
	GPIO_InitStruct.Pin = LED_BUILTIN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(LED_BUILTIN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PC14 PC15 */
	GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : r1_Pin g1_Pin b1_Pin r2_Pin
	 g2_Pin b2_Pin clk_Pin oe_Pin */
	GPIO_InitStruct.Pin = r1_Pin | g1_Pin | b1_Pin | r2_Pin | g2_Pin | b2_Pin | clk_Pin | oe_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PA6 PA8 PA11 PA12 */
	GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : da_Pin db_Pin dc_Pin dd_Pin
	 stb_Pin */
	GPIO_InitStruct.Pin = da_Pin | db_Pin | dc_Pin | dd_Pin | stb_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : PB10 PB11 PB12 PB13
	 PB14 PB15 PB5 PB6
	 PB7 PB8 PB9 */
	GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
			| GPIO_PIN_15 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
/* TODO Function Declaration*/
static int indexOfStr(char *from, const char *str, uint32_t fromIndex)
{
	uint32_t len = strlen(from);
	if (fromIndex >= len)
		return -1;
	const char *found = strstr(from + fromIndex, str);
	if (found == NULL)
		return -1;
	return found - from;
}

static int indexOfChr(char *from, const char ch, uint32_t fromIndex)
{
	uint32_t len = strlen(from);
	if (fromIndex >= len)
		return -1;
	const char* temp = strchr(from + fromIndex, ch);
	if (temp == NULL)
		return -1;
	return temp - from;
}

static void substring(char *from, char *to, uint32_t left, uint32_t right)
{
	uint32_t len = strlen(from);
	char *out;

	if (left > right)
	{
		unsigned int temp = right;
		right = left;
		left = temp;
	}
	if (left > len)
		return;
	if (right > len)
		right = len;
	char temp = from[right];  // save the replaced character
	from[right] = '\0';
	out = from + left;  // pointer arithmetic
	sprintf(to, "%s", out);
	from[right] = temp;  //restore character
}

static void startSwapBuffer()
{
	while (swapBufferStart == 1)
		;
	swapBufferStart = 1;
}

static void parsingFlipText()
{
	int i, start, end;
	const char delimiter = '\n';

	memset(&flipTextBuffer, 0, sizeof(flipTextBuffer));
	char *str = rText.activeText.msg;

	i = start = end = 0;
	for ( i = 0; i < 10; i++ )
	{
		if (indexOfChr(str, delimiter, end) < 0)
		{
			end = indexOfChr(str, delimiter, start + 1);
			substring(str, flipTextBuffer.buf[i], start, end < 0 ? strlen(str) : end);
			break;
		}

		end = indexOfChr(str, delimiter, start + 1);
		substring(str, flipTextBuffer.buf[i], start, end < 0 ? strlen(str) : end);

		end++;
		start = end;
	}
	flipTextBuffer.maxBuf = i + 1;
}
static void runningVerTextDisplay()
{

	DisplayText_t *dt = &rText.activeText;
	Animation_t *actAnim = &dt->animation;
	int32_t x, y;
	uint32_t len;
	int32_t hThresshold;
	FlipTextBuffer_t *fb = &flipTextBuffer;

	rgb_frame_clear(0);
	for ( int i = 0; i < fb->maxBuf; i++ )
	{
		len = strlen(fb->buf[i]);
		if (len > 0)
		{
			if (actAnim->alignment == TA_CENTER)
				x = (MATRIX_MAX_WIDTH - (int32_t) dt->fontSize * 6 * (int32_t) len) / 2;
			else
				x = 0;

			y = (MATRIX_MAX_HEIGHT - 8 * dt->fontSize) / 2;
			if (actAnim->direction == SD_DOWNWARD)
				y -= (i * MATRIX_MAX_HEIGHT);
			else if (actAnim->direction == SD_UPWARD)
				y += (i * MATRIX_MAX_HEIGHT);

			rgb_print(x + actAnim->currentX, y + actAnim->currentY, fb->buf[i], len, dt->color,
					dt->fontSize);
		}
	}
	startSwapBuffer();

	if (actAnim->direction == SD_DOWNWARD)
	{
		if (actAnim->tailAnimation == HT_BLANK)
			hThresshold = (int32_t) fb->maxBuf * MATRIX_MAX_HEIGHT;
		else
			hThresshold = (int32_t) (fb->maxBuf - 1) * MATRIX_MAX_HEIGHT;

		if (actAnim->currentY % MATRIX_MAX_HEIGHT == 0)
			actAnim->timeout = HAL_GetTick() + 1500;
		else
			actAnim->timeout = HAL_GetTick() + 25;

		actAnim->currentY++;
		if (actAnim->currentY > hThresshold)
		{
			if (actAnim->tailAnimation == HT_BLINK)
				actAnim->state = anim_start_tail_anim;
			else
				actAnim->state = anim_end;
		}
	}
	else if (actAnim->direction == SD_UPWARD)
	{
		if (actAnim->tailAnimation == HT_BLANK)
			hThresshold = 0 - (int32_t) fb->maxBuf * MATRIX_MAX_HEIGHT;
		else
			hThresshold = 0 - (int32_t) (fb->maxBuf - 1) * MATRIX_MAX_HEIGHT;

		if (actAnim->currentY % MATRIX_MAX_HEIGHT == 0)
			actAnim->timeout = HAL_GetTick() + 1500;
		else
			actAnim->timeout = HAL_GetTick() + 25;

		actAnim->currentY--;
		if (actAnim->currentY < hThresshold)
		{
			if (actAnim->tailAnimation == HT_BLINK)
				actAnim->state = anim_start_tail_anim;
			else
				actAnim->state = anim_end;
		}
	}
}
static void runningHorTextDisplay()
{
	DisplayText_t *dt = &rText.activeText;
	Animation_t *actAnim = &dt->animation;
	int32_t xAnimEndPos;

	rgb_frame_clear(0);
	xAnimEndPos = rgb_print_constrain(actAnim->currentX, actAnim->currentY, dt->msg,
			strlen(dt->msg), dt->color, dt->fontSize, 0, MATRIX_MAX_WIDTH, 0,
			MATRIX_MAX_HEIGHT);
	startSwapBuffer();
	if (actAnim->tailAnimation == HT_BLANK)
		xAnimEndPos += MATRIX_MAX_WIDTH;
	if (xAnimEndPos >= MATRIX_MAX_WIDTH)
	{
		if (actAnim->direction == SD_LEFT_TO_RIGHT)
		{
			actAnim->currentX--;
			actAnim->timeout = HAL_GetTick() + 25;
		}
		else
		{
			actAnim->state = anim_end;
			actAnim->timeout = HAL_GetTick() + 1000;
		}
	}
	else
	{
		if (actAnim->tailAnimation == HT_BLINK)
			actAnim->state = anim_start_tail_anim;
		else if (actAnim->tailAnimation == HT_BLANK)
			actAnim->state = anim_end;
		else
		{
			actAnim->state = anim_end;
			actAnim->timeout = HAL_GetTick() + 1000;
		}
	}
}

static void displayTextAnimation()
{
	DisplayText_t *dt = &rText.activeText;
	Animation_t *actAnim = &dt->animation;
	StateAnimation_e *state = &actAnim->state;
	static uint8_t blinkCounter = 0;

	switch (*state)
	{
	case anim_to_start:
		/* format text according to scrolling direction */
		actAnim->currentX = 0;
		if (dt->animation.vAlign == 1)
			actAnim->currentY = 0;
		else
			actAnim->currentY = (MATRIX_MAX_HEIGHT - 8 * dt->fontSize) / 2 + (dt->fontSize / 2);
		if ((actAnim->direction == SD_DOWNWARD) || (actAnim->direction == SD_UPWARD))
		{
			parsingFlipText();
			actAnim->currentX = actAnim->currentY = 0;
		}

		/* add head animation */
		switch (actAnim->headAnimation)
		{
		case HT_BLINK:
			/* add timeout */
			actAnim->timeout = 250;
			/* move to anim_start_head_anim */
			blinkCounter = 0;
			*state = anim_start_head_anim;
			break;
		case HT_BLANK:
			/* add blank space with width = MATRIX_MAX_WIDTH */
			actAnim->currentX = MATRIX_MAX_WIDTH;
		default:
			actAnim->timeout = 1000;
			/* command blank display with refresh cmd_timeout */
			if (actAnim->direction == SD_NONE && dt->color == 0)
				*state = anim_check_new;
			else
				*state = anim_running;
			break;
		}

		if (actAnim->direction == SD_LEFT_TO_RIGHT)
		{
			rgb_frame_clear(0);
			rgb_print(actAnim->currentX, actAnim->currentY, dt->msg, strlen(dt->msg), dt->color,
					dt->fontSize);
			startSwapBuffer();
		}
		break;
	case anim_start_head_anim:
		if (HAL_GetTick() >= actAnim->timeout)
		{
			rgb_frame_clear(0);
			if (bitRead(blinkCounter, 0))
				rgb_print(actAnim->currentX, actAnim->currentY, dt->msg, strlen(dt->msg), dt->color,
						dt->fontSize);
			startSwapBuffer();
			actAnim->timeout = HAL_GetTick() + 250;

			if (++blinkCounter >= 6)
				actAnim->state = anim_running;
		}
		break;
	case anim_head_anim:
		break;
	case anim_running:
		if (HAL_GetTick() >= actAnim->timeout)
		{
			if ((actAnim->direction == SD_DOWNWARD) || (actAnim->direction == SD_UPWARD))
				runningVerTextDisplay();
			else
				runningHorTextDisplay();
		}
		break;
	case anim_start_tail_anim:
		blinkCounter = 0;
		actAnim->state = anim_tail_anim;
		actAnim->timeout = HAL_GetTick() + 250;
		break;
	case anim_tail_anim:
		rgb_frame_clear(0);
		if (bitRead(blinkCounter, 0))
		{
			rgb_print_constrain(actAnim->currentX, actAnim->currentY, dt->msg, strlen(dt->msg),
					dt->color, dt->fontSize, 0, MATRIX_MAX_WIDTH, 0,
					MATRIX_MAX_HEIGHT);
		}
		startSwapBuffer();
		actAnim->timeout = HAL_GetTick() + 250;
		if (++blinkCounter > 6)
			actAnim->state = anim_end;
		break;
	case anim_end:
		if (HAL_GetTick() >= actAnim->timeout)
			actAnim->state = anim_check_new;
		break;
	default:
		actAnim->state = anim_to_start;
		break;

	}
}

static void checkNewCommand()
{
	if (rText.newTextAvailable)
	{
		if (rText.newText.type == DISPLAY_TEXT)
		{
			//if currently running default animation
			if (rText.activeText.defaultMode)
			{
				rText.activeText = rText.newText;
				rText.activeText.timeout = HAL_GetTick() + CMD_TIMEOUT;
				rText.state = st_ops;
				rText.activeText.animation.state = anim_to_start;
				rText.newTextAvailable = false;
			}  //if (!rText.activeText.defaultMode)
			else
			{
				// if not in block type
				if (rText.activeText.type == DISPLAY_TEXT)
				{
					// if not running animation
					if (rText.activeText.animation.state == anim_check_new)
					{
						/* swap new text to active text */
						rText.activeText = rText.newText;
						rText.activeText.timeout = HAL_GetTick() + CMD_TIMEOUT;
						rText.state = st_ops;
						rText.activeText.animation.state = anim_to_start;
						rText.newTextAvailable = false;
					}
				}
			}  //if (!rText.activeText.defaultMode) .. else ..
		}  //if (rText.newText.type == DISPLAY_TEXT)
		else if (rText.newText.type == DISPLAY_BLOCK)
		{
			rText.activeText = rText.newText;
			rText.state = st_block;
			rgb_frame_clear(rText.activeText.color);
			startSwapBuffer();
			rText.newTextAvailable = false;
		}  //else if (rText.newText.type == DISPLAY_BLOCK){
	}  //if (rText.newTextAvailable)
}

static void displayHandler()
{
	checkNewCommand();

// if timeout
	if (rText.state != st_default && (HAL_GetTick() >= rText.activeText.timeout))
	{
		rText.activeText = rText.DefaultText;
		rText.activeText.timeout = 0;
		rText.state = st_default;
		rText.activeText.animation.state = anim_to_start;
	}

	if (rText.activeText.type == DISPLAY_TEXT)
		displayTextAnimation();
}

static HAL_StatusTypeDef parsingCommand(char *cmd)
{
	char *pointer = cmd;
	char tem[DISPLAY_TEXT_MAX_SIZE];
	uint16_t pos;
	uint16_t i;
	uint16_t msgIndex;
	uint32_t u32;
	uint16_t param1 = 0, param2 = 0;
	char cFind, cFind2;
	DisplayText_t *dt = &rText.newText;

	/* check header bytes */
	memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
	pos = strlen(pointer);
	cFind = ',';
	for ( i = 0; i < pos; i++ )
	{
		if (*pointer != cFind)
			tem[i] = *pointer++;
		else
			break;
	}

	if ((i >= pos) && (strlen(tem) == 0))
		return HAL_ERROR;

	if (strcmp(tem, "$RTEXT") != 0)
	{
		return HAL_ERROR;
	}

	memset(&rText.newText, 0, sizeof(rText.newText));

	/* find display type */
	pointer++;
	memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
	pos = strlen(pointer);
	cFind = ',';
	for ( i = 0; i < pos; i++ )
	{
		if (*pointer == cFind)
			break;
		else
			tem[i] = *pointer++;
	}
	if ((i >= pos) && (strlen(tem) == 0))
		return HAL_ERROR;
	u32 = atoi(tem);
	if (u32 < DISPLAY_RESV)
		dt->type = atoi(tem);
	else
		return HAL_ERROR;

	/* find params I */
	param1 = 0;
	pointer++;
	memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
	pos = strlen(pointer);
	cFind = ',';
	cFind2 = '*';
	for ( i = 0; i < pos; i++ )
	{
		if ((*pointer == cFind) || (*pointer == cFind2))
			break;
		else
			tem[i] = *pointer++;
	}
	if ((i >= pos) && (strlen(tem) == 0))
		return HAL_ERROR;
	param1 = strtol(tem, NULL, 16);

	/* find params II */
	param2 = 0;
	pointer++;
	memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
	pos = strlen(pointer);
	cFind = ',';
	cFind2 = '*';
	for ( i = 0; i < pos; i++ )
	{
		if ((*pointer == cFind) || (*pointer == cFind2))
			break;
		else
			tem[i] = *pointer++;
	}
	if ((i >= pos) && (strlen(tem) == 0))
		return HAL_ERROR;
	param2 = strtol(tem, NULL, 16);

	/* parsing params */
	/* color */
	dt->color = param1 & 0b111;
	if (dt->type == DISPLAY_BLOCK)
	{
		/* default mode */
		dt->defaultMode = 0;
		/* fontsize */
		/* timeout in 1s */
		dt->timeout = (param1 >> PB_Timeout_bit) & 0xFF;
		dt->timeout *= 1000;  //convert to ms
		dt->timeout += HAL_GetTick();  // add to current time
		/* head animation */
		/* tail animation */
		/* alignment */
		/* direction */
	}
	else if (dt->type == DISPLAY_TEXT)
	{
		/* default */
		dt->defaultMode = bitRead(param1, PB_Default_bit);
		/* fontsize */
		dt->fontSize = ((param1 >> PB_Fontsize_bit) & 0b11) + 1;
		/* timeout in 1s */
		/* head animation */
		dt->animation.headAnimation = (param2 >> PB_HeadAnim_bit) & 0b111;
		/* tail animation */
		dt->animation.tailAnimation = (param2 >> PB_TailAnim_bit) & 0b111;
		/* direction */
		dt->animation.direction = (param2 >> PB_Direction_bit) & 0xF;
		/* alignment */
		dt->animation.alignment = (param2 >> PB_Alignment_bit) & 0b11;
		/* vertical alignment */
		dt->animation.vAlign = (param2 >> PB_VAlignment_bit) & 0b11;
	}

	/* acquire text */
	if (dt->type == DISPLAY_TEXT)
	{
		cFind = '\"';
		pointer++;
		if (*pointer != cFind)
			return HAL_ERROR;

		pointer++;
		memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
		pos = strlen(pointer);
		msgIndex = 0;
		for ( i = 0; i < pos; i++ )
		{
			if (*pointer == cFind)
				break;

			tem[msgIndex++] = *pointer++;
		}
		if ((i >= pos) && (strlen(tem) == 0))
			return HAL_ERROR;

		memcpy(dt->msg, tem, strlen(tem));
	}

	if (!dt->defaultMode)
		rText.newTextAvailable = true;
	else
		rText.DefaultText = *dt;

	return HAL_OK;
}

static void commandHandler()
{
	bool command_available = false;
	char c[2] = { 0, 0 };

	if (serial_available(&serial))
	{
		c[0] = serial_read(&serial);

		if (c[0] == '$')
			memset(inCmd, 0, CMD_BUFSIZE);
		else if (c[0] == '*')
			command_available = true;

		strcat(inCmd, c);
	}

	if (command_available)
		parsingCommand(inCmd);
}

static void commandInit()
{
	rText.newTextAvailable = false;
	rText.state = st_to_default;
	memset(&rText.DefaultText, 0, sizeof(rText.DefaultText));
	memset(&rText.activeText, 0, sizeof(rText.activeText));
	memset(&rText.newText, 0, sizeof(rText.newText));

	/* Initialize RText's default text struct */
	DisplayText_t *dt = &rText.DefaultText;

	dt->color = 1;
	dt->defaultMode = true;
	dt->fontSize = 2;
	dt->type = DISPLAY_TEXT;
	dt->timeout = HAL_GetTick() + CMD_TIMEOUT;
	sprintf(dt->msg, "PT INDUSTRI KERETA API (PERSERO). MADIUN");

	dt->animation.alignment = TA_LEFT;
	dt->animation.direction = SD_LEFT_TO_RIGHT;
	dt->animation.headAnimation = HT_BLANK;
	dt->animation.tailAnimation = HT_BLANK;

//	char buf[DISPLAY_TEXT_MAX_SIZE];
//	uint16_t bufLen;
//	bufLen = sprintf(buf, "%d %d %d %d\r\n%s", dt->type, dt->color, dt->fontSize, dt->defaultMode,
//			dt->msg);
//	rgb_frame_clear(0);
//	rgb_print(0, 0, buf, bufLen, 1, 1);
//	swapBufferStart = 1;

#if USE_EEPROM_FUNCTION==1
	commandEEInit();
#endif	//if USE_EEPROM_FUNCTION==1
}

#if USE_EEPROM_FUNCTION==1

static void commandEEInit()
{
	uint32_t data = 0;
	uint32_t formatMask = 0xFFFFFF00;
	uint8_t u8;
	uint32_t defText[DISPLAY_TEXT_MAX_SIZE / 4];

	if (EE_Read(START_EE_ADDRESS, &data))
	{
		if ((data & formatMask) != FORMATED_BYTES)
		commandEESave(stateKeeper.defaultText.color, stateKeeper.defaultText.fontSize,
				stateKeeper.defaultText.msg);
		else
		{
			/* retrieve font color & size */
			u8 = data & 0xFF;
			stateKeeper.defaultText.type = DISPLAY_TEXT;
			stateKeeper.defaultText.color = u8 & 0xF;
			stateKeeper.defaultText.fontSize = u8 >> 4;

			if (EE_Reads(START_EE_ADDRESS + 1, DISPLAY_TEXT_MAX_SIZE / 4, defText))
			{
				memset(stateKeeper.defaultText.msg, 0, DISPLAY_TEXT_MAX_SIZE);
				for ( int i = 0; i < DISPLAY_TEXT_MAX_SIZE / 4; i++ )
				{
					data = defText[i];
					for ( int j = 0; j < 4; j++ )
					stateKeeper.defaultText.msg[i * 4 + j] = (char) (data >> (8 * j));
				}
			}
		}
	}
}

static void commandEESave(uint8_t color, uint8_t fontSize, char *buf)
{
	uint32_t data = FORMATED_BYTES;
	data |= (fontSize << 4) & 0xF0;
	data |= color & 0xF;

	/* temporary clear display */
	rgb_frame_clear(0);
	swapBufferStart = 1;
	while (swapBufferStart)
	;
	/* temporary de-active display matrix */
	deactiveMatrix = 1;
	while (deactiveMatrix)
	;

	EE_Format();

	EE_Write(START_EE_ADDRESS, data);
	for ( int i = 0; i < EE_DATA_MAX_SIZE - 1; i++ )
	{
		data = 0;
		for ( int j = 0; j < 4; j++ )
		data |= (uint32_t) (*buf++) << (8 * j);
		EE_Write(START_EE_ADDRESS + 1 + i, data);
	}

	/* retrieve old display buffer */
	rgb_swap_buffer();
	activeFrameRowAdress = rgb_get_buffer(matrix_row);
	/* activate display matrix again */
	startDmaTransfering();
}
#endif	//if USE_EEPROM_FUNCTION==1

void USART1_IRQHandler(void)
{
	USARTx_IRQHandler(&serial);
}

static void startDmaTransfering()
{
	HAL_DMA_Start_IT(&hdma_tim1_up, activeFrameRowAdress, (uint32_t) &DATA_GPIO->ODR,
	FRAME_SIZE);
}

static void dmaTransferCompleted(DMA_HandleTypeDef *hdma)
{
	/* clear clk & data */
	DATA_GPIO->BSRR = DATA_BSRR_CLEAR;

	/* set oe & stb high */
	oe_GPIO_Port->BSRR = oe_Pin;
	stb_GPIO_Port->BSRR = stb_Pin;
	CONTROL_GPIO->ODR = (matrix_row << CONTROL_SHIFTER);
//	/* add 3 clk to enable STB/LAT effect */
//	clk_GPIO_Port->BSRR = clk_Pin;
//	clk_GPIO_Port->BSRR = DATA_BSRR_CLK_CLEAR;
//	clk_GPIO_Port->BSRR = clk_Pin;
//	clk_GPIO_Port->BSRR = DATA_BSRR_CLK_CLEAR;
//	clk_GPIO_Port->BSRR = clk_Pin;
//	clk_GPIO_Port->BSRR = DATA_BSRR_CLK_CLEAR;
	/* set oe & stb LOW */
	stb_GPIO_Port->BSRR = CONTROL_BSRR_STB_CLEAR;
	oe_GPIO_Port->BSRR = DATA_BSRR_OE_CLEAR;

	/* update matrix row */
	if (++matrix_row >= MATRIX_SCANROW)
	{
		matrix_row = 0;
		if (swapBufferStart)
		{
			rgb_swap_buffer();
			swapBufferStart = 0;
		}

#if DEBUG
		fps++;
#endif	//if DEBUG

	}
	activeFrameRowAdress = rgb_get_buffer(matrix_row);

	/* add delay to display */
	/* enable tim2 */
	htim2.Instance->CNT = 0;
	if (matrix_row == 0 && deactiveMatrix)
		deactiveMatrix = 0;
	else
		HAL_TIM_Base_Start_IT(&htim2);

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	HAL_TIM_Base_Stop_IT(htim);
	startDmaTransfering();
}
/* TODO END of Function Declaration*/
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
