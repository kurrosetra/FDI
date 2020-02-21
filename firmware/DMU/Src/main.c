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
#define USE_EEPROM_FUNCTION		1
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "matrix.h"
#include "stm_hal_serial.h"
#if USE_EEPROM_FUNCTION==1
#include "eeprom.h"
#endif	//if USE_EEPROM_FUNCTION==1
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define DISPLAY_TEXT_MAX_SIZE		64
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
	anim_start,
	anim_start_blinking,
	anim_running,
	anim_end,
	anim_info
} StateAnimation_e;

typedef struct
{
	StateAnimation_e state;
	uint32_t timeout;
} Animation_t;

typedef enum
{
	DISPLAY_TEXT = 0,
	DISPLAY_BLOCK
} DisplayType_e;

typedef struct
{
	DisplayType_e type;
	uint8_t fontSize;
	uint8_t color;
	char msg[DISPLAY_TEXT_MAX_SIZE];
	uint32_t timeout;
} DisplayText_t;

typedef struct
{
	Animation_t animation;
	DisplayText_t activeText;
	DisplayText_t newText;
	DisplayText_t defaultText;
	bool newTextAvailable;
} StateKeeper_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBUG					0
#if DEBUG==1
uint16_t fps = 0;
#endif	//if DEBUG==1

#define CMD_BUFSIZE				128
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

/* USER CODE BEGIN PV *//* TODO Global Variables*/
Ring_Buffer_t tx1Buffer = { { 0 }, 0, 0 };
Ring_Buffer_t rx1Buffer = { { 0 }, 0, 0 };
TSerial serial = { &rx1Buffer, &tx1Buffer, &huart1 };

volatile uint8_t swapBufferStart = 0;
volatile uint32_t activeFrameRowAdress = 0;
volatile uint32_t matrix_row = 0;
volatile uint8_t deactiveMatrix = 0;

char inCmd[CMD_BUFSIZE];

StateKeeper_t stateKeeper;
uint8_t activeDisplayState = st_to_default;

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
static void MX_USART1_UART_Init(void);
static void MX_IWDG_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
/* TODO Function Prototypes*/
static void startDmaTransfering();
static void dmaTransferCompleted(DMA_HandleTypeDef *hdma);

static HAL_StatusTypeDef parsingCommand(char *cmd, StateKeeper_t *state);
static void commandHandler();
static void commandInit();
#if USE_EEPROM_FUNCTION==1
static void commandEEInit();
static void commandEESave(uint8_t color, uint8_t fontSize, char *buf);
#endif	//if USE_EEPROM_FUNCTION==1

static void displayHandler();
static void displayAnimation(uint8_t currentState, bool blinking);
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
	MX_USART1_UART_Init();
	MX_IWDG_Init();
	MX_TIM2_Init();
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
	uint16_t headerLen = sprintf(headerStart, "RESPATI");
	rgb_print(43, 0, headerStart, headerLen, TEXT_COLOR, 1);
	swapBufferStart = 1;

	for ( uint8_t i = 0; i < 5; i++ )
	{
		HAL_IWDG_Refresh(&hiwdg);
		HAL_Delay(100);
	}
	rgb_frame_clear(0);
	swapBufferStart = 1;
	for ( uint8_t i = 0; i < 5; i++ )
	{
		HAL_IWDG_Refresh(&hiwdg);
		HAL_Delay(100);
	}

#if DEBUG==1
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
			rgb_print(64, 0, headerStart, headerLen, TEXT_COLOR, 2);
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
	HAL_GPIO_WritePin(GPIOA, r1_Pin | g1_Pin | clk_Pin | oe_Pin, GPIO_PIN_RESET);

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

	/*Configure GPIO pins : r1_Pin g1_Pin clk_Pin oe_Pin */
	GPIO_InitStruct.Pin = r1_Pin | g1_Pin | clk_Pin | oe_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PA2 PA3 PA4 PA5
	 PA6 PA8 PA11 PA12 */
	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6
			| GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_12;
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
/* TODO Function Declaration*/
static void displayAnimation(uint8_t currentState, bool blinking)
{
	char *text;
	uint8_t fontColor;
	uint8_t fontSize;
	static int16_t xAnimPos = 0;
	static int16_t yAnimPos = 0;
	int16_t xAnimEndPos = 0;
	static uint8_t blinkCounter = 0;

	if (currentState == st_ops)
	{
		text = stateKeeper.activeText.msg;
		fontColor = stateKeeper.activeText.color;
		fontSize = stateKeeper.activeText.fontSize;
	}
	else
	{
		text = stateKeeper.defaultText.msg;
		fontColor = stateKeeper.defaultText.color;
		fontSize = stateKeeper.defaultText.fontSize;
	}

	if (stateKeeper.animation.state == anim_to_start)
	{
		if (currentState == st_ops)
		{
			if (stateKeeper.newTextAvailable && (stateKeeper.newText.type == DISPLAY_TEXT))
			{
				stateKeeper.activeText = stateKeeper.newText;
				stateKeeper.newTextAvailable = false;
				text = stateKeeper.activeText.msg;
				fontColor = stateKeeper.activeText.color;
				fontSize = stateKeeper.activeText.fontSize;
			}
		}

		if ((strchr(text, '\r') != NULL) || (strchr(text, '\n') != NULL))
		{
			rgb_frame_clear(0);
			rgb_print_constrain(0, 0, text, strlen(text), fontColor, fontSize, 0,
			MATRIX_MAX_WIDTH, 0, MATRIX_MAX_HEIGHT);

			swapBufferStart = 1;
			stateKeeper.animation.state = anim_info;
		}
		else
		{
			blinkCounter = 0;
			if (blinking)
				stateKeeper.animation.state = anim_start_blinking;
			else
			{
				rgb_frame_clear(0);
				xAnimPos = 0;
				yAnimPos = (MATRIX_MAX_HEIGHT - 8 * fontSize) / 2;
				rgb_print_constrain(xAnimPos, yAnimPos, text, strlen(text), fontColor, fontSize, 0,
				MATRIX_MAX_WIDTH, 0, MATRIX_MAX_HEIGHT);

				swapBufferStart = 1;

				stateKeeper.animation.timeout = HAL_GetTick() + 1000;
				stateKeeper.animation.state = anim_start;
			}
		}
	}
	else if (stateKeeper.animation.state == anim_info)
	{
		if (currentState == st_ops)
		{
			if (stateKeeper.newTextAvailable && (stateKeeper.newText.type == DISPLAY_TEXT))
				stateKeeper.animation.state = anim_to_start;
		}
	}
	else if (stateKeeper.animation.state == anim_start_blinking)
	{
		if (HAL_GetTick() >= stateKeeper.animation.timeout)
		{
			if (++blinkCounter > 6)
				stateKeeper.animation.state = anim_running;
			else
			{
				rgb_frame_clear(0);
				if (bitRead(blinkCounter, 0))
				{
					xAnimPos = 0;
					yAnimPos = (MATRIX_MAX_HEIGHT - 8 * fontSize) / 2;
					rgb_print_constrain(xAnimPos, yAnimPos, text, strlen(text), fontColor, fontSize,
							0,
							MATRIX_MAX_WIDTH, 0, MATRIX_MAX_HEIGHT);
				}
				swapBufferStart = 1;

				stateKeeper.animation.timeout = HAL_GetTick() + 250;
			}
		}
	}
	else if (stateKeeper.animation.state == anim_start)
	{
		if (HAL_GetTick() >= stateKeeper.animation.timeout)
			stateKeeper.animation.state = anim_running;
	}
	else if (stateKeeper.animation.state == anim_running)
	{
		if (HAL_GetTick() >= stateKeeper.animation.timeout)
		{
			rgb_frame_clear(0);
			xAnimEndPos = rgb_print_constrain(xAnimPos, yAnimPos, text, strlen(text), fontColor,
					fontSize, 0, MATRIX_MAX_WIDTH, 0, MATRIX_MAX_HEIGHT);
			swapBufferStart = 1;
			if (xAnimEndPos >= MATRIX_MAX_WIDTH)
			{
				xAnimPos--;
				stateKeeper.animation.timeout = HAL_GetTick() + 30;
			}
			else
			{
				stateKeeper.animation.state = anim_end;
				stateKeeper.animation.timeout = HAL_GetTick() + 1000;
			}
		}
	}
	else if (stateKeeper.animation.state == anim_end)
	{
		if (HAL_GetTick() >= stateKeeper.animation.timeout)
			stateKeeper.animation.state = anim_to_start;
	}
}

static void displayCheckState()
{
	if (stateKeeper.newTextAvailable)
	{
		if (stateKeeper.newText.type == DISPLAY_TEXT)
		{
			activeDisplayState = st_ops;
			stateKeeper.animation.state = anim_to_start;
		}
		else if (stateKeeper.newText.type == DISPLAY_BLOCK)
			activeDisplayState = st_block;

		stateKeeper.animation.state = anim_to_start;
	}
}

static void displayHandler()
{
	if (activeDisplayState != st_default && !stateKeeper.newTextAvailable)
	{
		if (HAL_GetTick() >= stateKeeper.activeText.timeout)
		{
			stateKeeper.activeText = stateKeeper.defaultText;
			stateKeeper.activeText.timeout = 0;
			activeDisplayState = st_default;
			stateKeeper.animation.state = anim_to_start;
			stateKeeper.animation.timeout = HAL_GetTick() + 1000;
		}
	}

	if (activeDisplayState == st_block)
	{
		if ((stateKeeper.newTextAvailable) && (stateKeeper.newText.type == DISPLAY_BLOCK))
		{
			stateKeeper.activeText = stateKeeper.newText;
			rgb_frame_clear(stateKeeper.activeText.color);
			swapBufferStart = 1;
			stateKeeper.newTextAvailable = false;
		}

		if (HAL_GetTick() >= stateKeeper.activeText.timeout)
			activeDisplayState = st_to_default;
	}
	else if (activeDisplayState == st_ops)
	{
		if ((stateKeeper.newTextAvailable) && (stateKeeper.newText.type == DISPLAY_BLOCK))
			activeDisplayState = st_block;

		displayAnimation(st_ops, 1);
	}
	else if (activeDisplayState == st_to_default)
		displayCheckState();
	else
	{
		displayAnimation(st_default, 1);
		displayCheckState();
	}

}

static HAL_StatusTypeDef parsingCommand(char *cmd, StateKeeper_t *state)
{
	char *pointer = cmd;
	char tem[DISPLAY_TEXT_MAX_SIZE];
	uint16_t pos;
	bool defCmd = false;
	uint16_t i;
	uint16_t msgIndex;
	uint8_t fontSize = 0, fontColor = 0;
	DisplayType_e newType = DISPLAY_TEXT;

	/* skip header byte */
	pointer++;

	/* acquire cmd type */
	memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
	pos = strlen(pointer);
	for ( i = 0; i < pos; i++ )
	{
		if (*pointer != ',')
			tem[i] = *pointer++;
		else
			break;
	}

	if ((i >= pos) && (strlen(tem) == 0))
		return HAL_ERROR;

	if (strcmp(tem, "BLK") == 0)
		newType = DISPLAY_BLOCK;
	else if (strcmp(tem, "MSG") == 0)
		newType = DISPLAY_TEXT;
	else if (strcmp(tem, "DEF") == 0)
	{
		newType = DISPLAY_TEXT;
		defCmd = true;
	}
	else
		return HAL_ERROR;

	/* acquire text color */
	pointer++;
	memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
	pos = strlen(pointer);
	for ( i = 0; i < pos; i++ )
	{
		if (*pointer == ',')
			break;
		else
			tem[i] = *pointer++;
	}
	if ((i >= pos) && (strlen(tem) == 0))
		return HAL_ERROR;

	fontColor = atoi(tem) % 8;

	if (newType == DISPLAY_BLOCK)
	{
		/* acquire block timeout */
		pointer++;
		memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
		pos = strlen(pointer);
		for ( i = 0; i < pos; i++ )
		{
			if (*pointer == '*')
				break;
			else
				tem[i] = *pointer++;
		}
		if ((i >= pos) && (strlen(tem) == 0))
			return HAL_ERROR;

		state->newText.type = newType;
		state->newText.color = fontColor;
		state->newTextAvailable = true;
		state->newText.timeout = atoi(tem) + HAL_GetTick();
	}
	else if (newType == DISPLAY_TEXT)
	{
		/* acquire text size */
		pointer++;
		memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
		pos = strlen(pointer);
		for ( i = 0; i < pos; i++ )
		{
			if (*pointer == ',')
				break;
			else
				tem[i] = *pointer++;
		}
		if ((i >= pos) && (strlen(tem) == 0))
			return HAL_ERROR;
		fontSize = atoi(tem) % 5;

		/* acquire text */
		pointer++;
		if (*pointer != '\"')
			return HAL_ERROR;

		pointer++;
		memset(tem, 0, DISPLAY_TEXT_MAX_SIZE);
		pos = strlen(pointer);
		msgIndex = 0;
		for ( i = 0; i < pos; i++ )
		{
			if (*pointer == '\"')
			{
				if ((msgIndex > 0) && ((tem[msgIndex - 1] == '\\')))
					msgIndex--;
				else
					break;
			}

			tem[msgIndex++] = *pointer++;
		}
		if ((i >= pos) && (strlen(tem) == 0))
			return HAL_ERROR;

		if (defCmd)
		{
			state->defaultText.type = newType;
			state->defaultText.color = fontColor;
			state->defaultText.fontSize = fontSize;
			/* re-write default message */
			memset(state->defaultText.msg, 0, DISPLAY_TEXT_MAX_SIZE);
			memcpy(state->defaultText.msg, tem, strlen(tem));
			/* save to eeprom */
#if USE_EEPROM_FUNCTION==1
			commandEESave(fontColor, fontSize, state->defaultText.msg);
#endif	//if USE_EEPROM_FUNCTION==1
		}
		else
		{
			state->newText.type = newType;
			state->newText.color = fontColor;
			state->newText.fontSize = fontSize;
			/* re-write msg buffer */
			memset(state->newText.msg, 0, DISPLAY_TEXT_MAX_SIZE);
			memcpy(state->newText.msg, tem, strlen(tem));

			state->newTextAvailable = true;
			state->newText.timeout = CMD_TIMEOUT + HAL_GetTick();
		}
	}

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
		parsingCommand(inCmd, &stateKeeper);
}

static void commandInit()
{
	/* initialize stateKeeper */
	stateKeeper.activeText.color = 0;
	stateKeeper.activeText.fontSize = 1;
	stateKeeper.activeText.type = DISPLAY_TEXT;
	memset(stateKeeper.activeText.msg, 0, DISPLAY_TEXT_MAX_SIZE);
	stateKeeper.activeText.timeout = HAL_GetTick() + CMD_TIMEOUT;

	stateKeeper.defaultText.color = 1;
	stateKeeper.defaultText.fontSize = 2;
	stateKeeper.defaultText.type = DISPLAY_TEXT;
	sprintf(stateKeeper.defaultText.msg, "PT KERETA API INDONESIA (PERSERO)");

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

/* TODO End of Function Declaration*/
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
