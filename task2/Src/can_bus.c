#include "can_bus.h"

static QueueHandle_t s_canQueue = NULL;

void CAN_Bus_Init(void)
{
    CAN_FilterTypeDef filter;

    /* 过滤器：32 位掩码模式，全部接收（验收时可改为只收指定 ID） */
    filter.FilterBank           = 0;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0x0000;
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = 0x0000;   /* 掩码全 0 -> 不筛选 */
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;
    filter.SlaveStartFilterBank = 14;       /* 单 CAN 外设时无意义，填 14 即可 */
    HAL_CAN_ConfigFilter(&hcan1, &filter);

    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

    /* 队列长度为 6（任务书硬性要求） */
    s_canQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CanRxPacket_t));
    configASSERT(s_canQueue != NULL);
}

QueueHandle_t CAN_GetQueue(void)
{
    return s_canQueue;
}

/**
  * @brief  CAN 接收中断回调：只做"打包 + 入队"，绝不在这里解析协议
  * @note   运行在 ISR 上下文，只能调用带 FromISR 后缀的 API
  */
void CAN_RxFifo0Callback(CAN_HandleTypeDef *hcan)
{
    CanRxPacket_t pkt;
    BaseType_t    xHigherPriorityTaskWoken = pdFALSE;

    if (hcan->Instance != CAN1)
    {
        return;
    }

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &pkt.header, pkt.data) == HAL_OK)
    {
        if (s_canQueue != NULL)
        {
            /* 队列满时直接丢弃：中断里绝不阻塞 */
            (void)xQueueSendFromISR(s_canQueue, &pkt, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
