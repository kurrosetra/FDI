#ifndef __EEPROM_H
#define __EEPROM_H

/*
 Author:     Nima Askari
 WebSite:    http://www.github.com/NimaLTD
 Instagram:  http://instagram.com/github.NimaLTD
 Youtube:    https://www.youtube.com/channel/UCUhY7qY1klJm1d2kulr9ckw

 Version:    1.1.0


 Reversion History:
 (1.1.0)
 Add f0 devices.

 (1.0.0)
 First Release.

 */
#ifdef __cplusplus
extern "C"
{
#endif

	/* TODO add define for each stm32fxx here */
#if defined(STM32F103xB) && defined(USE_HAL_DRIVER)
#include "stm32f1xx.h"
#endif

#if defined(STM32F446xx) && defined(USE_HAL_DRIVER)
#include "stm32f4xx.h"
#endif	//if STM32F446xx

#include <stdbool.h>

//################################################################################################################
	bool EE_Format(void);
	bool EE_Read(uint16_t VirtualAddress, uint32_t* Data);
	bool EE_Write(uint16_t VirtualAddress, uint32_t Data);
	bool EE_Reads(uint16_t StartVirtualAddress, uint16_t HowManyToRead, uint32_t* Data);
	bool EE_Writes(uint16_t StartVirtualAddress, uint16_t HowManyToWrite, uint32_t* Data);
	uint16_t EE_GetSize(void);
	uint16_t EE_GetMaximumVirtualAddress(void);
//################################################################################################################

#ifdef __cplusplus
}
#endif

#endif
