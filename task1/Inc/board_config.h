#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/* ============================================================================
 * 任务一 硬件资源映射
 * 目标板：立创·梁山派·天空星 STM32F407VGT6（LQFP100，1024 KB Flash，168 MHz）
 * ----------------------------------------------------------------------------
 *  ⚠️ 天空星板卡专属注意事项（换板子请务必核对）：
 *   1) 外部高速晶振 HSE = 8 MHz（CubeMX 默认 25 MHz，必须改！）
 *   2) PA9/PA10 没有引到排针（板载 DAP-Link 占用）→ 串口用 USART1 重映射 PB6/PB7
 *   3) PA4~PA7 被板载 W25Q128（SPI1）占用 → 呼吸灯不能用 PA6，改用 PB4 (TIM3_CH1)
 *   4) PA11/PA12 是 USB OTG FS 引脚 → CAN1 改用重映射 PB8/PB9
 *   5) 板载用户 LED 是 PB2，但 PB2 没有定时器通道 → 只能做普通闪烁，不能做 PWM 呼吸
 * ----------------------------------------------------------------------------
 *  呼吸灯 PWM   : TIM3_CH1 -> PB4      （外接 LED + 1kΩ 到 GND；不要用 PA6！）
 *  呼吸节律节拍 : TIM7 更新中断 1 kHz  （呼吸由硬件定时器驱动，不依赖任何任务）
 *  CAN          : CAN1 重映射 -> PB8(RX) / PB9(TX)，外接 TJA1050（5V 供电）
 *                 TXD: MCU 3.3V 直驱；RXD: 模块输出≈5V，PB8 为 FT 引脚可直连（可串 1k）
 *                 波特率 500 kbps（APB1 42 MHz：PSC=6, BS1=9TQ, BS2=4TQ）
 *  VOFA 串口    : USART1 重映射 -> PB6(TX) / PB7(RX)，115200
 *                 RX = DMA2_Stream2(Circular)，TX = DMA2_Stream7(Normal)
 *  SWD 下载     : 板载 DAP-Link / ST-Link -> PA13(SWDIO) / PA14(SWCLK)
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

/* ---------- 呼吸灯 ---------- */
#define BREATH_PWM_TIM          (&htim3)
#define BREATH_PWM_CHANNEL      TIM_CHANNEL_1
#define BREATH_PWM_PERIOD       (BREATH_PWM_TIM->Init.Period)
#define BREATH_TICK_TIM         (&htim7)
#define BREATH_TICK_FREQ_HZ     (1000u)
#define BREATH_PERIOD_MIN_MS    (100u)
#define BREATH_PERIOD_MAX_MS    (10000u)
#define BREATH_PERIOD_DEFAULT   (2000u)     /* 上电默认一个完整呼吸周期 2 s */

/* ---------- 队列长度（任务书硬性要求） ---------- */
#define CAN_QUEUE_LENGTH        (6u)   /* CAN 接收队列长度必须为 6  */
#define UART_QUEUE_LENGTH       (3u)   /* 串口接收队列长度必须为 3  */

/* ---------- 串口缓冲 ---------- */
#define UART_RX_BUF_SIZE        (128u)
#define UART_TX_BUF_SIZE        (256u)

/* =========================== CAN 自定义协议 ===========================
 *  字节: [0]      [1]     [2..3]        [4]      [5..6]   [7]
 *        0xAA头帧  CMD    数据(大端u16) 校验和    保留0    0x55尾帧
 *  校验和 = (CMD + DATA_H + DATA_L) & 0xFF
 *
 *  命令：
 *    0x01  修改呼吸周期，DATA = 周期毫秒数（如 500 = 0.5 s 一个来回）
 *    0x02  熄灭呼吸灯
 *    0x03  点亮呼吸灯
 *
 *  举例（把呼吸周期改成 1000 ms）：
 *    AA 01 03 E8 EC 00 00 55     （E8 = 1000 低字节，03 = 高字节；
 *                                 校验和 = 01+03+E8 = 0xEC）
 * =====================================================================*/
#define CAN_FRAME_HEAD          (0xAAu)
#define CAN_FRAME_TAIL          (0x55u)

#define CAN_CMD_SET_BREATH_PERIOD   (0x01u)  /* 修改呼吸周期，单位 ms */
#define CAN_CMD_LED_OFF             (0x02u)  /* 熄灭呼吸灯            */
#define CAN_CMD_LED_ON              (0x03u)  /* 点亮呼吸灯            */

#endif /* __BOARD_CONFIG_H */
