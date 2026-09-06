/**
  ******************************************************************************
  * @file    attitude.c
  * @brief   互补滤波姿态解算（Plan B）
  *
  *  陀螺积分 + 加速度计重力向量互补融合。
  *  量程假设与 mpu6050.c 一致：±2g（16384 LSB/g）、±250°/s（131 LSB/(°/s)）。
  ******************************************************************************
  */
#include <math.h>
#include "attitude.h"

#define ACC_LSB_PER_G     (16384.0f)
#define GYR_LSB_PER_DPS   (131.0f)

static float s_pitch = 0.0f;
static float s_roll  = 0.0f;
static float s_yaw   = 0.0f;

void Attitude_Reset(void)
{
    s_pitch = 0.0f;
    s_roll  = 0.0f;
    s_yaw   = 0.0f;
}

void Attitude_Update(const int16_t accel[3], const int16_t gyro[3], float dt)
{
    float ax, ay, az, gx, gy, gz;

    /* 饱和保护：满量程值（±32768）是数据路径锁死的特征，不参与解算 */
    ax = (accel[0] == -32768) ? 0.0f : (float)accel[0] / ACC_LSB_PER_G;
    ay = (accel[1] == -32768) ? 0.0f : (float)accel[1] / ACC_LSB_PER_G;
    az = (accel[2] == -32768) ? 0.0f : (float)accel[2] / ACC_LSB_PER_G;
    gx = ((gyro[0] == -32768) || (gyro[0] == 32767)) ? 0.0f :
         (float)gyro[0] / GYR_LSB_PER_DPS;
    gy = ((gyro[1] == -32768) || (gyro[1] == 32767)) ? 0.0f :
         (float)gyro[1] / GYR_LSB_PER_DPS;
    gz = ((gyro[2] == -32768) || (gyro[2] == 32767)) ? 0.0f :
         (float)gyro[2] / GYR_LSB_PER_DPS;

    /* 1) 陀螺积分：快、平滑，但零偏会累积成漂移 */
    s_pitch += gx * dt;
    s_roll  += gy * dt;
    s_yaw   += gz * dt;

    /* 2) 加速度计算姿态参考角：静止时可信，运动时噪声大 */
    {
        float pitchAcc = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.29578f;
        float rollAcc  = atan2f(ay, az) * 57.29578f;

        /* 3) 动态门限：加速度模长偏离 1g 太多（甩动/冲击）时跳过融合，
         *    避免运动加速度污染姿态 */
        float amag = sqrtf(ax * ax + ay * ay + az * az);

        if ((amag > 0.6f) && (amag < 1.6f))
        {
            /* 互补滤波：主用陀螺（98%），加速度慢慢把长期漂移拉回来（2%） */
            s_pitch = 0.98f * s_pitch + 0.02f * pitchAcc;
            s_roll  = 0.98f * s_roll  + 0.02f * rollAcc;
        }
    }

    /* 角度规整：yaw ±180°；pitch/roll 限幅防积分发散后长时间拉不回 */
    if (s_yaw   >  180.0f) { s_yaw   -= 360.0f; }
    if (s_yaw   < -180.0f) { s_yaw   += 360.0f; }
    if (s_pitch >  720.0f) { s_pitch =  720.0f; }
    if (s_pitch < -720.0f) { s_pitch = -720.0f; }
    if (s_roll  >  720.0f) { s_roll  =  720.0f; }
    if (s_roll  < -720.0f) { s_roll  = -720.0f; }
}

float Attitude_GetPitch(void) { return s_pitch; }
float Attitude_GetRoll(void)  { return s_roll; }
float Attitude_GetYaw(void)   { return s_yaw; }
