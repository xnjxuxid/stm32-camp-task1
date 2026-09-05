#ifndef __CAN_H
#define __CAN_H

#include "stm32f4xx_hal.h"

extern CAN_HandleTypeDef hcan1;

void MX_CAN1_Init(void);

#endif /* __CAN_H */
