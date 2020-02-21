/*
 * stm_hal_serial.c
 *
 *  Created on: Nov 24, 2018
 *      Author: miftakur
 */

#include "stm_hal_serial.h"

void serial_start_transmitting(TSerial *serial);

void serial_init(TSerial *serial)
{
	__HAL_UART_ENABLE_IT(serial->huart, UART_IT_RXNE);
	/* TODO add idle line detection*/
//	__HAL_UART_ENABLE_IT(serial->huart, (UART_IT_RXNE|UART_IT_IDLE));
}

uint8_t USARTx_IRQHandler(TSerial *serial)
{
	uint32_t isrflags = READ_REG(serial->huart->Instance->SR);
	uint8_t ret = HAL_UART_RETURN_NONE;

	/* UART in mode Receiver -------------------------------------------------*/
//	if (((isrflags & USART_SR_RXNE) != RESET) && ((cr1its & USART_CR1_RXNEIE) != RESET))
	if ((isrflags & USART_SR_RXNE) != RESET)
	{
		ring_buffer_write(serial->TBufferRx, (char) (serial->huart->Instance->DR & 0xFF));
		ret = HAL_UART_RETURN_RX;
	}

	return ret;
}

char serial_read(TSerial *serial)
{
	return ring_buffer_read(serial->TBufferRx);
}

void serial_write(TSerial *serial, char c)
{
	ring_buffer_write(serial->TBufferTx, c);
	serial_start_transmitting(serial);
}

void serial_read_str(TSerial *serial, char *str)
{
	ring_buffer_read_str(serial->TBufferRx, str);
}

void serial_write_str(TSerial *serial, char *str, uint16_t len)
{
	for ( uint16_t i = 0; i < len; i++ )
		ring_buffer_write(serial->TBufferTx, *str++);

	serial_start_transmitting(serial);
//	uint32_t cr1its = READ_REG(serial->huart->Instance->CR1);
//	uint32_t isrflags = READ_REG(serial->huart->Instance->SR);
//	if (((isrflags & USART_SR_TXE) != RESET) && ((cr1its & USART_CR1_TXEIE) == RESET)) {
//		/* Enable the UART Transmit data register empty Interrupt */
//		__HAL_UART_ENABLE_IT(serial->huart, UART_IT_TXE);
//	}
}

bool serial_available(TSerial *serial)
{
	return (serial->TBufferRx->head != serial->TBufferRx->tail);
}

void serial_start_transmitting(TSerial *serial)
{
	uint32_t cr1its = READ_REG(serial->huart->Instance->CR1);
	uint32_t isrflags = READ_REG(serial->huart->Instance->SR);

	if (((isrflags & USART_SR_TXE) != RESET) && ((cr1its & USART_CR1_TXEIE) == RESET))
	{
		/* Enable the UART Transmit data register empty Interrupt */
		__HAL_UART_ENABLE_IT(serial->huart, UART_IT_TXE);
	}
}
