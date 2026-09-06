#ifndef __MPU6050_H
#define __MPU6050_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* ============================================================================
 * MPU6050 驱动（六轴：3 轴加速度计 + 3 轴陀螺仪），基于软件 IIC
 * ----------------------------------------------------------------------------
 * 量程配置（本工程固定）：
 *   加速度计 ±2g   → 灵敏度 16384 LSB/g
 *   陀螺仪  ±250°/s → 灵敏度 131 LSB/(°/s)
 * ============================================================================*/

/* 寄存器地址（任务书外知识：MPU6050 寄存器手册 Register Map） */
#define MPU_REG_SELF_TEST_X     (0x0Du)
#define MPU_REG_SMPLRT_DIV      (0x19u)
#define MPU_REG_CONFIG          (0x1Au)     /* DLPF 低通滤波配置          */
#define MPU_REG_GYRO_CONFIG     (0x1Bu)     /* 陀螺仪量程                 */
#define MPU_REG_ACCEL_CONFIG    (0x1Cu)     /* 加速度计量程               */
#define MPU_REG_FIFO_EN         (0x23u)
#define MPU_REG_INT_PIN_CFG     (0x37u)
#define MPU_REG_INT_ENABLE      (0x38u)
#define MPU_REG_ACCEL_XOUT_H    (0x3Bu)     /* 原始数据起始（连续 14 字节：6 轴 + 温度） */
#define MPU_REG_TEMP_OUT_H      (0x41u)
#define MPU_REG_GYRO_XOUT_H     (0x43u)
#define MPU_REG_PWR_MGMT_1      (0x6Bu)     /* 电源管理：复位/唤醒/时钟源 */
#define MPU_REG_PWR_MGMT_2      (0x6Cu)
#define MPU_REG_WHO_AM_I        (0x75u)     /* 器件 ID                    */

/* 量程换算系数 */
#define MPU_ACCEL_LSB_PER_G     (16384.0f)  /* ±2g  */
#define MPU_GYRO_LSB_PER_DPS    (131.0f)    /* ±250°/s */

/* 返回值：0 成功，非 0 失败 */
int  MPU6050_Init(void);
int  MPU6050_ReadWhoAmI(uint8_t *id);
/* 一次连续读 14 字节：加速度 x/y/z + 温度 + 陀螺仪 x/y/z（地址自动递增） */
int  MPU6050_ReadRaw(int16_t accel[3], int16_t gyro[3]);
/* 清除自检残留：SELF_TEST_X/Y/Z=0，GYRO_CONFIG/ACCEL_CONFIG 重写（清 ST 位）
 * 自检后陀螺输出恒定 -7xxx 偏移就是自检模式没退出造成的 */
int  MPU6050_ClearSelfTest(void);

/* ---- eMPL（InvenSense DMP 库）需要的两个底层接口，实现在本文件 ---- */
int  MPU_Write_Len(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *buf);
int  MPU_Read_Len (uint8_t addr, uint8_t reg, uint8_t len, uint8_t *buf);

#endif /* __MPU6050_H */
