#ifndef __CAN_BUS_H
#define __CAN_BUS_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "board_config.h"

/* 中断里打包好的一帧 CAN 报文 */
typedef struct
{
    CAN_RxHeaderTypeDef header;     /* StdId / ExtId / DLC 等 */
    uint8_t             data[8];    /* 最多 8 字节数据 */
} CanRxPacket_t;

void          CAN_Bus_Init(void);
QueueHandle_t CAN_GetQueue(void);

/* 由 stm32f4xx_it.c 的 CAN1_RX0_IRQHandler 回调进来 */
void          CAN_RxFifo0Callback(CAN_HandleTypeDef *hcan);

#endif /* __CAN_BUS_H */
