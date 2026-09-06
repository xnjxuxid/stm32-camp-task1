#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/* ============================================================================
 * 任务二 硬件资源映射
 * 目标板：立创·梁山派·天空星 STM32F407VGT6（LQFP100，1024 KB Flash，168 MHz）
 * ----------------------------------------------------------------------------
 *  ⚠️ 天空星板卡专属注意事项（换板子请务必核对）：
 *   1) 外部高速晶振 HSE = 8 MHz（CubeMX 默认 25 MHz，必须改！）
 *   2) PA9/PA10 没有引到排针（板载 DAP-Link 占用）→ 串口用 USART1 重映射 PB6/PB7
 *   3) PA4~PA7 被板载 W25Q128（SPI1）占用 → 呼吸灯不能用 PA6，改用 PB4 (TIM3_CH1)
 *   4) PA11/PA12 是 USB OTG FS 引脚 → CAN1 改用重映射 PB8/PB9
 *   5) 板载用户 LED 是 PB2，但 PB2 没有定时器通道 → 只能做普通闪烁，不能做 PWM 呼吸
 * ----------------------------------------------------------------------------
 *  【继承任务一】
 *  呼吸灯 PWM   : TIM3_CH1 -> PB4      （外接 LED + 1kΩ 到 GND；不要用 PA6！）
 *  呼吸节律节拍 : TIM7 更新中断 1 kHz  （呼吸由硬件定时器驱动，不依赖任何任务）
 *  CAN          : CAN1 重映射 -> PB8(RX) / PB9(TX)，外接 TJA1050（5V 供电）
 *                 波特率 500 kbps（APB1 42 MHz：PSC=6, BS1=9TQ, BS2=4TQ）
 *  VOFA 串口    : USART1 重映射 -> PB6(TX) / PB7(RX)，115200
 *                 RX = DMA2_Stream2(Circular)，TX = DMA2_Stream7(Normal)
 *  SWD 下载     : 板载 DAP-Link / ST-Link -> PA13(SWDIO) / PA14(SWCLK)
 * ----------------------------------------------------------------------------
 *  【任务二新增】
 *  软件 IIC     : SCL -> PB0 , SDA -> PB1 （GPIO 开漏 + 上拉）
 *                 ⚠️ 不能用 PB8/PB9（被任务一的 CAN 占用）；PB0/PB1 空闲且引出
 *                 GY-521 模块自带 4.7kΩ 上拉，无需外接
 *  MPU6050      : AD0 -> GND（器件地址 0x68），VCC -> 3.3V，INT/XDA/XCL 悬空
 *  DMP          : 需要用户提供 InvenSense DMP 库（见 docs/task2-接线与验收说明.md），
 *                 提供 DMP_ENABLED 宏开关：0 = 只跑原始六轴（立即可用）
 *                 1 = 启用 DMP 欧拉角（放置好库文件并加入工程后打开）
 * ==========================================================================*/

/* ---------- VOFA 串口集中配置（换串口只改这里 + CubeMX 引脚） ---------- */
#define VOFA_UART_HANDLE        huart1
#define VOFA_UART               (&VOFA_UART_HANDLE)
#define VOFA_UART_INSTANCE      USART1

/* ---------- CubeMX 生成的外设句柄，统一在此声明 ---------- */
extern TIM_HandleTypeDef  htim3;            /* 呼吸灯 PWM            */
extern TIM_HandleTypeDef  htim7;            /* 呼吸节律节拍 1 kHz    */
extern CAN_HandleTypeDef  hcan1;            /* CAN1                  */
extern UART_HandleTypeDef huart1;           /* VOFA 串口             */
extern DMA_HandleTypeDef  hdma_usart1_rx;   /* USART1 RX DMA         */
extern DMA_HandleTypeDef  hdma_usart1_tx;   /* USART1 TX DMA         */

/* ---------- 呼吸灯（继承任务一） ---------- */
#define BREATH_PWM_TIM          (&htim3)
#define BREATH_PWM_CHANNEL      TIM_CHANNEL_1
#define BREATH_PWM_PERIOD       (BREATH_PWM_TIM->Init.Period)
#define BREATH_TICK_TIM         (&htim7)
#define BREATH_TICK_FREQ_HZ     (1000u)
#define BREATH_PERIOD_MIN_MS    (100u)
#define BREATH_PERIOD_MAX_MS    (10000u)
#define BREATH_PERIOD_DEFAULT   (2000u)     /* 上电默认一个完整呼吸周期 2 s */

/* ---------- 队列长度（任务书硬性要求，继承任务一） ---------- */
#define CAN_QUEUE_LENGTH        (6u)   /* CAN 接收队列长度必须为 6  */
#define UART_QUEUE_LENGTH       (3u)   /* 串口接收队列长度必须为 3  */

/* ---------- 串口缓冲 ---------- */
#define UART_RX_BUF_SIZE        (128u)
#define UART_TX_BUF_SIZE        (256u)

/* ==================== 任务二：软件 IIC + MPU6050 ==================== */

/* ---- 软件 IIC 引脚（⚠️ 不能用 PB8/PB9，那是 CAN 的） ---- */
#define SOFT_I2C_GPIO_PORT      GPIOB
#define SOFT_I2C_SCL_PIN        GPIO_PIN_0
#define SOFT_I2C_SDA_PIN        GPIO_PIN_1
#define SOFT_I2C_GPIO_CLK()     __HAL_RCC_GPIOB_CLK_ENABLE()

/* 软件 IIC 半位延时（微秒）。2 µs ≈ 250 kHz（MPU6050 支持到 400 kHz）。
 * ⚠️ 任务二单周期内要读 14 字节原始数据 + DMP FIFO（约 34 字节），
 * 半位 4 µs 时 IIC 总耗时逼近 5 ms 周期上限，提速到 2 µs 留出裕量。 */
#define SOFT_I2C_HALF_BIT_US    (2u)

/* ---- MPU6050 ---- */
#define MPU6050_ADDR_7BIT       (0x68u)     /* AD0 接 GND 时的 7 位地址 */
#define MPU6050_ADDR_W          ((MPU6050_ADDR_7BIT << 1) | 0x00u)   /* 0xD0 写 */
#define MPU6050_ADDR_R          ((MPU6050_ADDR_7BIT << 1) | 0x01u)   /* 0xD1 读 */

/* WHO_AM_I 应答值：正品的 MPU6050 返回 0x68，
 * 部分兼容芯片（MPU6500/9250 早期批次）返回其它值，用于上电自检 */
#define MPU6050_WHO_AM_I_VAL    (0x68u)

/* ---- DMP 开关 ----
 * 0：不编译 DMP，任务只发送原始六轴（6 通道）——立即可验收
 * 1：启用 DMP 欧拉角（额外 3 通道）——需要先放置 DMP 库文件并加入 Keil 工程
 *    步骤见 docs/task2-接线与验收说明.md 第 6 节 */
#ifndef DMP_ENABLED
#define DMP_ENABLED             (1)     /* eMPL 库已集成（riverzhou/mpu6050），已启用 */
#endif

/* DMP 调试打印：1 = 静默 JustFloat、纯文本诊断（排查用）；0 = 正常 9 通道 JustFloat */
#ifndef DMP_DEBUG
#define DMP_DEBUG               (0)
#endif

/* ch6~8 姿态角输出源：
 * 0 = 互补滤波（当前芯片 DMP 引擎无效时的真实解算，视频用这个）
 * 1 = DMP 输出（换到 DMP 正常的模块后置 1） */
#ifndef DMP_USE_OUTPUT
#define DMP_USE_OUTPUT          (0)
#endif

/* ---- MPU 任务 ---- */
#define MPU_TASK_PERIOD_MS      (5u)        /* 任务书要求：固定 5 ms（vTaskDelayUntil） */
#define MPU_TASK_STACK          (512u)      /* DMP/浮点运算吃栈，给大一点 */
#define MPU_TASK_PRIO           (3)

/* ---- VOFA JustFloat 帧 ----
 * 协议：N 个 float（小端） + 帧尾 0x00 0x00 0x80 0x7F
 * VOFA+ 上位机选"JustFloat"协议即可实时画波形 */
#define JUSTFLOAT_TAIL          {0x00u, 0x00u, 0x80u, 0x7Fu}

/* =========================== CAN 自定义协议（继承任务一） ==========================
 *  字节: [0]=0xAA头帧 [1]=CMD [2..3]=数据(大端u16) [4]=校验和 [5..6]=保留 [7]=0x55尾帧
 *  校验和 = (CMD + DATA_H + DATA_L) & 0xFF
 * =====================================================================*/
#define CAN_FRAME_HEAD          (0xAAu)
#define CAN_FRAME_TAIL          (0x55u)

#define CAN_CMD_SET_BREATH_PERIOD   (0x01u)  /* 修改呼吸周期，单位 ms */
#define CAN_CMD_LED_OFF             (0x02u)  /* 熄灭呼吸灯            */
#define CAN_CMD_LED_ON              (0x03u)  /* 点亮呼吸灯            */

#endif /* __BOARD_CONFIG_H */
