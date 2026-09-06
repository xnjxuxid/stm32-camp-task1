#include <stdio.h>
#include <string.h>

#include "app_tasks.h"
#include "can_bus.h"
#include "uart_vofa.h"
#include "breath_led.h"
#include "board_config.h"
#include "mpu6050.h"
#include "mpu_dmp.h"
#include "vofa_send.h"
#include "soft_i2c.h"
#include "attitude.h"

/* ============================================================================
 * 任务二：4 个任务（任务一 3 个 + 新增 MPU 任务）
 *   Task_CanRx    —— CAN 队列消费者：解帧、校验、发任务通知（继承任务一）
 *   Task_Breath   —— 等任务通知改呼吸周期；没通知就阻塞（继承任务一）
 *   Task_UartEcho —— 处理 DMA 收到的数据，按格式回传 VOFA（继承任务一）
 *   Task_Mpu      —— ★ 新增：vTaskDelayUntil 固定 5 ms 绝对周期
 *                     1) 软件IIC 读原始六轴 -> JustFloat 6 通道
 *                     2) 读 DMP 欧拉角   -> JustFloat 3 通道（需启用 DMP）
 * ==========================================================================*/

static TaskHandle_t s_taskCanHandle    = NULL;
static TaskHandle_t s_taskBreathHandle = NULL;
static TaskHandle_t s_taskUartHandle   = NULL;
static TaskHandle_t s_taskMpuHandle    = NULL;

/* ============================ 任务一：CAN 数据处理 ========================= */
static void Task_CanRx(void *argument)
{
    CanRxPacket_t pkt;
    uint8_t       sum;
    uint8_t       cmd;
    uint16_t      value;

    for (;;)
    {
        if (xQueueReceive(CAN_GetQueue(), &pkt, portMAX_DELAY) == pdPASS)
        {
            if ((pkt.data[0] != CAN_FRAME_HEAD) || (pkt.data[7] != CAN_FRAME_TAIL))
            {
                continue;
            }

            sum = (uint8_t)(pkt.data[1] + pkt.data[2] + pkt.data[3]);
            if (sum != pkt.data[4])
            {
                continue;
            }

            cmd   = pkt.data[1];
            value = (uint16_t)(((uint16_t)pkt.data[2] << 8) | pkt.data[3]);

            switch (cmd)
            {
                case CAN_CMD_SET_BREATH_PERIOD:
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
        if (xTaskNotifyWait(0x00000000,
                            0xFFFFFFFF,
                            &notifyValue,
                            portMAX_DELAY) == pdPASS)
        {
            BreathLED_SetPeriodMs((uint16_t)notifyValue);
        }
        /* 呼吸波形由 TIM7 中断 + TIM3 PWM 硬件产生 ——
         * 本任务阻塞时呼吸照常，满足任务书"任务二运行中呼吸灯仍按原频率工作"。*/
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

            len = snprintf(txBuf, sizeof(txBuf), "Receive Data ：（%s）\n", clean);
            if (len > 0)
            {
                UART_SendData_DMA((uint8_t *)txBuf, (uint16_t)len);
            }
        }
    }
}

/* ====================== 任务四：MPU6050（5ms 绝对周期） ==================== */
/*
 * ★ 任务书核心考点：vTaskDelayUntil（绝对周期）
 *   vTaskDelay(5)      = "这次干完活再等 5ms" → 周期 = 干活时间 + 5ms，会漂移
 *   vTaskDelayUntil(5) = "距上次唤醒满 5ms 再唤醒" → 周期严格 5ms，不漂移
 *   VOFA 时间戳能看出差别：后者相邻点间隔恒定，前者随工作量抖动。
 *
 * 串口占用说明：本任务与 Task_UartEcho 共用串口发送，
 * UART_SendData_DMA 内部有互斥量，数据不会撕裂；VOFA 用 JustFloat 协议看曲线，
 * 需要演示串口回传时在上位机切回文本模式即可。
 */
static void Task_Mpu(void *argument)
{
    TickType_t lastWake;
    float     ch[9];
    int16_t   accel[3];
    int16_t   gyro[3];
    uint16_t  zeroCnt = 0;
    uint8_t   mpuDown = 0;

    /* --- 初始化（失败则慢速重试，不拖垮整个系统） --- */
    for (;;)
    {
        if (MPU6050_Init() == 0)
        {
            /* 诊断：读回电源寄存器。0x01/0x00 = 已唤醒（SLEEP=0）；
             * 若读回 0x40 = 还在睡眠（SLEEP=1），数据寄存器将永远是 0 */
            uint8_t pwr = 0;
            (void)SoftI2C_ReadReg(MPU6050_ADDR_7BIT, MPU_REG_PWR_MGMT_1, &pwr);
            printf("MPU6050 ready (soft IIC 0x68), PWR_MGMT_1=0x%02X (SLEEP bit should be clear)\r\n", pwr);
            break;
        }
        printf("MPU6050 init failed - check wiring: VCC/GND, SCL=PB0, SDA=PB1, AD0=GND\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));      /* 1 秒后重试 */
    }

#if DMP_ENABLED
    if (MPU_DMP_Init() == 0)
    {
        printf("DMP firmware loaded, euler angles ON (9 channels)\r\n");
    }
    else
    {
        printf("DMP init FAILED - raw data only (6 channels)\r\n");
    }
#else
    printf("DMP disabled - raw 6-axis only (6 channels)\r\n");
#endif

    lastWake = xTaskGetTickCount();           /* 记录首个唤醒基准点 */

    for (;;)
    {
        /* ---- IIC 总线锁死自恢复状态机 ----
         * 六轴同时全 0 在物理上不可能（静止时 z≈+1g，运动时陀螺也不为 0）。
         * "读成功"但全 0 = SDA 被从机拉死（读回全 0 且假 ACK），
         * 常见于运动时杜邦线接触不良。连续 40 帧（200ms）全 0 触发恢复。 */
        if (mpuDown)
        {
            SoftI2C_BusRecover();             /* 9 个时钟 + STOP 解锁总线 */
            if (MPU6050_Init() == 0)
            {
#if DMP_ENABLED
                if (MPU_DMP_Init() != 0)
                {
                    printf("MPU recovered, DMP re-init FAILED (raw only)\r\n");
                }
                else
#endif
                {
                    printf("MPU recovered\r\n");
                }
                mpuDown  = 0;
                zeroCnt  = 0;
                lastWake = xTaskGetTickCount();   /* 重置周期基准，避免追赶爆发 */
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(1000));  /* 1 秒后再试 */
            }
            continue;
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(MPU_TASK_PERIOD_MS));

        /* -------- 1) 原始六轴 + 姿态解算 -------- */
        if (MPU6050_ReadRaw(accel, gyro) == 0)
        {
            if ((accel[0] | accel[1] | accel[2] |
                 gyro[0]  | gyro[1]  | gyro[2]) == 0)
            {
                if (++zeroCnt >= 40u)
                {
                    zeroCnt = 0;
                    mpuDown = 1;
                    printf("IIC bus locked (all-zero data) - recovering...\r\n");
                }
            }
            else
            {
                zeroCnt = 0;

                /* 互补滤波每周期无条件更新（维持滤波器状态连续） */
                Attitude_Update(accel, gyro, 0.005f);

                /* JustFloat 恒定 9 通道一帧：ch0~2 加速度(g)、ch3~5 角速度(°/s)、
                 * ch6~8 姿态角(°)。⚠️ 必须一帧发全——
                 * 如果角度单独用 ch[0..2] 发一帧，会覆盖 VOFA 里的加速度通道
                 * （已修复的 bug）。 */
                ch[0] = (float)accel[0] / MPU_ACCEL_LSB_PER_G;    /* 单位 g    */
                ch[1] = (float)accel[1] / MPU_ACCEL_LSB_PER_G;
                ch[2] = (float)accel[2] / MPU_ACCEL_LSB_PER_G;
                ch[3] = (float)gyro[0]  / MPU_GYRO_LSB_PER_DPS;   /* 单位 °/s  */
                ch[4] = (float)gyro[1]  / MPU_GYRO_LSB_PER_DPS;
                ch[5] = (float)gyro[2]  / MPU_GYRO_LSB_PER_DPS;

#if DMP_ENABLED && DMP_USE_OUTPUT
                /* DMP 输出（换到 DMP 正常的模块后把 DMP_USE_OUTPUT 置 1） */
                {
                    float pitch, roll, yaw;
                    if (MPU_DMP_Read(&pitch, &roll, &yaw) == 0)
                    {
                        ch[6] = pitch;
                        ch[7] = roll;
                        ch[8] = yaw;
                    }
                    else
                    {
                        ch[6] = Attitude_GetPitch();
                        ch[7] = Attitude_GetRoll();
                        ch[8] = Attitude_GetYaw();
                    }
                }
#else
                /* 当前芯片 DMP 引擎无效：使用互补滤波解算（数据真实） */
                ch[6] = Attitude_GetPitch();
                ch[7] = Attitude_GetRoll();
                ch[8] = Attitude_GetYaw();
#endif

                VOFA_SendJustFloat(ch, 9);       /* 恒定 9 通道一帧 */
            }
        }
    }
}

/* ================================ 任务创建 ================================ */
void App_Tasks_Create(void)
{
    CAN_Bus_Init();         /* CAN 过滤器 + 启动 + 队列(6)（继承任务一） */
    UART_VOFA_Init();       /* 串口空闲中断 + DMA 接收 + 队列(3)（继承任务一） */
    BreathLED_Init();       /* PWM + TIM7 节拍（继承任务一） */

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

    xTaskCreate(Task_Mpu,
                "Mpu5ms",
                MPU_TASK_STACK,
                NULL,
                MPU_TASK_PRIO,
                &s_taskMpuHandle);
}
