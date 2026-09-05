#ifndef __BREATH_LED_H
#define __BREATH_LED_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board_config.h"

/* 启动 PWM + 1 kHz 节拍（呼吸由 TIM7 中断 + TIM3 PWM 硬件产生） */
void     BreathLED_Init(void);

/* 修改一个完整呼吸周期的时长（ms） */
void     BreathLED_SetPeriodMs(uint16_t period_ms);
uint16_t BreathLED_GetPeriodMs(void);

/* 1=呼吸，0=熄灭 */
void     BreathLED_Enable(uint8_t on);

/* 由 HAL_TIM_PeriodElapsedCallback 在 TIM7 中断里调用 */
void     BreathLED_TickISR(TIM_HandleTypeDef *htim);

#endif /* __BREATH_LED_H */
