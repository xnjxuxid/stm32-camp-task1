#ifndef __DMA_H
#define __DMA_H

#include "stm32f4xx_hal.h"

extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

void MX_DMA_Init(void);

#endif /* __DMA_H */
