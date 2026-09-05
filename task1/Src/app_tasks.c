#include <stdio.h>
#include <string.h>

#include "app_tasks.h"
#include "can_bus.h"
#include "uart_vofa.h"
#include "breath_led.h"
#include "board_config.h"

/* ============================================================================
 * 任务一：3 个任务
 *   Task_CanRx    —— CAN 队列的消费者：解帧、校验、发任务通知
 *   Task_Breath   —— 等待任务通知，修改呼吸灯周期；没通知就阻塞
 *   Task_UartEcho —— 处理 DMA 收到的不定长数据，按格式回传 VOFA
 * ==========================================================================*/

static TaskHandle_t s_taskCanHandle    = NULL;
static TaskHandle_t s_taskBreathHandle = NULL;
static TaskHandle_t s_taskUartHandle   = NULL;

/* ============================ 任务一：CAN 数据处理 ========================= */
static void Task_CanRx(void *argument)
{
    CanRxPacket_t pkt;
    uint8_t       sum;
    uint8_t       cmd;
    uint16_t      value;

    for (;;)
    {
        /* 队列空则阻塞等待，CAN 中断负责入队 */
        if (xQueueReceive(CAN_GetQueue(), &pkt, portMAX_DELAY) == pdPASS)
        {
            /* 1) 检查头帧 / 尾帧 */
            if ((pkt.data[0] != CAN_FRAME_HEAD) || (pkt.data[7] != CAN_FRAME_TAIL))
            {
                continue;                       /* 非法帧，丢弃 */
            }

            /* 2) 校验和 */
            sum = (uint8_t)(pkt.data[1] + pkt.data[2] + pkt.data[3]);
            if (sum != pkt.data[4])
            {
                continue;                       /* 校验失败，丢弃 */
            }

            /* 3) 取出有效控制信息（数据为大端 u16） */
            cmd   = pkt.data[1];
            value = (uint16_t)(((uint16_t)pkt.data[2] << 8) | pkt.data[3]);

            switch (cmd)
            {
                case CAN_CMD_SET_BREATH_PERIOD:
                    /* 任务通知：携带新的呼吸周期值（ms） */
                    (void)xTaskNotify(s_taskBreathHandle,
                                      (uint32_t)value,
                                      eSetValueWithOverwrite);
                    break;

                case CAN_CMD_LED_OFF:
                    BreathLED_Enable(0);
                    break;

                case CAN_CMD_LED_ON:
                    BreathLED_Enable(1);
                    break;

                default:
                    break;
            }
        }
    }
}

/* =========================== 任务二：呼吸灯控制 =========================== */
static void Task_Breath(void *argument)
{
    uint32_t notifyValue = 0;

    for (;;)
    {
        /* 没有收到任务通知就一直阻塞，完全不占 CPU */
        if (xTaskNotifyWait(0x00000000,        /* 进入前不清位 */
                            0xFFFFFFFF,        /* 退出后清掉全部通知位 */
                            &notifyValue,
                            portMAX_DELAY) == pdPASS)
        {
            BreathLED_SetPeriodMs((uint16_t)notifyValue);
        }

        /* 注意：呼吸波形由 TIM7 中断 + TIM3 PWM 硬件产生，
         *       与本任务是否运行无关 —— 满足"没有通知时仍按之前频率呼吸"。*/
    }
}

/* ======================= 任务三：串口数据处理与回传 ======================= */
static void Task_UartEcho(void *argument)
{
    UartRxPacket_t pkt;
    char     clean[UART_RX_BUF_SIZE + 1];
    char     txBuf[UART_TX_BUF_SIZE];
    uint16_t n;
    uint16_t i;
    int      len;
    uint8_t  c;

    for (;;)
    {
        if (xQueueReceive(UART_GetQueue(), &pkt, portMAX_DELAY) == pdPASS)
        {
            /* 数据处理：剔除换行符，非可打印字符替换为 '.'，并补字符串结束符 */
            n = 0;
            for (i = 0; (i < pkt.len) && (n < UART_RX_BUF_SIZE); i++)
            {
                c = pkt.buf[i];
                if ((c == '\r') || (c == '\n'))
                {
                    continue;
                }
                clean[n++] = ((c >= 0x20) && (c < 0x7F)) ? (char)c : '.';
            }
            clean[n] = '\0';

            /* 按验收要求的格式组包并回传（串口 + DMA）
             * 格式：Receive Data ：（%s）\n   —— 注意是全角冒号和全角括号 */
            len = snprintf(txBuf, sizeof(txBuf), "Receive Data ：（%s）\n", clean);
            if (len > 0)
            {
                UART_SendData_DMA((uint8_t *)txBuf, (uint16_t)len);
            }
        }
    }
}

/* ================================ 任务创建 ================================ */
void App_Tasks_Create(void)
{
    CAN_Bus_Init();         /* CAN 过滤器 + 启动 + 队列(6) */
    UART_VOFA_Init();       /* 串口空闲中断 + DMA 接收 + 队列(3) */
    BreathLED_Init();       /* PWM + TIM7 节拍，呼吸开始 */

    xTaskCreate(Task_CanRx,
                "CanRx",
                TASK_CAN_STACK,
                NULL,
                TASK_CAN_PRIO,
                &s_taskCanHandle);

    xTaskCreate(Task_Breath,
                "Breath",
                TASK_BREATH_STACK,
                NULL,
                TASK_BREATH_PRIO,
                &s_taskBreathHandle);

    xTaskCreate(Task_UartEcho,
                "UartEcho",
                TASK_UART_STACK,
                NULL,
                TASK_UART_PRIO,
                &s_taskUartHandle);
}
