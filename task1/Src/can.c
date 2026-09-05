/**
  ******************************************************************************
  * @file    can.c
  * @brief   CAN1 初始化（PB8=RX / PB9=TX，500 kbps）
  *
  *  本文件等价于 CubeMX 勾选 CAN1 后生成的初始化代码（手工等价实现，
  *  这样不重新生成 CubeMX 工程也能直接编译下载）。
  *
  *  波特率计算：CAN 挂在 APB1 = 42 MHz
  *    TQ 总数 = 1(SYNC) + BS1(9) + BS2(4) = 14
  *    Baud    = 42 MHz / Prescaler(6) / 14 = 500 kbps
  ******************************************************************************
  */
#include "can.h"
#include "main.h"

CAN_HandleTypeDef hcan1;

void MX_CAN1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* ---------------- 时钟 ---------------- */
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* ---------------- 引脚：PB8=CAN1_RX, PB9=CAN1_TX (AF9) ---------------- */
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ---------------- CAN 参数 ---------------- */
    hcan1.Instance                  = CAN1;
    hcan1.Init.Prescaler            = 6;
    hcan1.Init.Mode                 = CAN_MODE_NORMAL;   /* 验收用正常模式 */
    hcan1.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1             = CAN_BS1_9TQ;
    hcan1.Init.TimeSeg2             = CAN_BS2_4TQ;
    hcan1.Init.TimeTriggeredMode    = DISABLE;
    hcan1.Init.AutoBusOff           = ENABLE;
    hcan1.Init.AutoWakeUp           = DISABLE;
    hcan1.Init.AutoRetransmission   = DISABLE;
    hcan1.Init.ReceiveFifoLocked    = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }

    /* ---------------- NVIC ----------------
     * 优先级必须 >= 5（FreeRTOS 中可在 ISR 里调用 FromISR API 的最低优先级），
     * 本项目 NVIC_PRIORITYGROUP_4，数值越大优先级越低。 */
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
}
