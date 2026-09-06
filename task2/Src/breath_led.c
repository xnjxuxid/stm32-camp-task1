#include <math.h>
#include "breath_led.h"

static volatile uint16_t s_periodMs = BREATH_PERIOD_DEFAULT;  /* 一个完整呼吸周期 */
static volatile uint32_t s_tick     = 0u;                     /* 1 ms 累加 */
static volatile uint8_t  s_enable   = 1u;

void BreathLED_Init(void)
{
    HAL_TIM_PWM_Start(BREATH_PWM_TIM, BREATH_PWM_CHANNEL);
    __HAL_TIM_SET_COMPARE(BREATH_PWM_TIM, BREATH_PWM_CHANNEL, 0);
    HAL_TIM_Base_Start_IT(BREATH_TICK_TIM);      /* 1 kHz 节拍，硬件驱动呼吸 */
}

void BreathLED_SetPeriodMs(uint16_t period_ms)
{
    if (period_ms < BREATH_PERIOD_MIN_MS)
    {
        period_ms = BREATH_PERIOD_MIN_MS;
    }
    if (period_ms > BREATH_PERIOD_MAX_MS)
    {
        period_ms = BREATH_PERIOD_MAX_MS;
    }

    taskENTER_CRITICAL();
    s_periodMs = period_ms;
    s_tick     = 0u;                            /* 重新从最暗开始，视觉上更直观 */
    taskEXIT_CRITICAL();
}

uint16_t BreathLED_GetPeriodMs(void)
{
    return (uint16_t)s_periodMs;
}

void BreathLED_Enable(uint8_t on)
{
    s_enable = on ? 1u : 0u;
    if (!on)
    {
        __HAL_TIM_SET_COMPARE(BREATH_PWM_TIM, BREATH_PWM_CHANNEL, 0);
    }
}

/**
  * @brief  1 kHz 定时中断中更新 PWM 占空比 —— 呼吸灯不依赖任何任务
  *         因此"任务阻塞 / 任务二不运行"时呼吸灯依然按原频率工作。
  */
void BreathLED_TickISR(TIM_HandleTypeDef *htim)
{
    float    phase;
    float    duty;
    uint32_t ccr;
    uint16_t period;

    if (htim->Instance != TIM7)
    {
        return;
    }
    if (!s_enable)
    {
        return;
    }

    period = s_periodMs;
    if (period == 0u)
    {
        return;
    }

    if (++s_tick >= period)
    {
        s_tick = 0u;
    }

    /* 余弦呼吸曲线，0 ~ 1 */
    phase = 6.28318530718f * (float)s_tick / (float)period;
    duty  = 0.5f - 0.5f * cosf(phase);

    ccr = (uint32_t)(duty * (float)BREATH_PWM_PERIOD);
    __HAL_TIM_SET_COMPARE(BREATH_PWM_TIM, BREATH_PWM_CHANNEL, ccr);
}
