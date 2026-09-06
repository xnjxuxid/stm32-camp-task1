/**
  ******************************************************************************
  * @file    mpu6050.c
  * @brief   MPU6050 驱动（软件 IIC）
  *
  *  初始化流程（对照 Register Map 手册）：
  *    1. WHO_AM_I  确认器件在位（0x68）
  *    2. PWR_MGMT_1 = 0x80   器件复位（100ms 后再操作）
  *    3. PWR_MGMT_1 = 0x01   退出睡眠，时钟源选 PLL（陀螺仪 Y 轴，比内部 RC 稳）
  *    4. SMPLRT_DIV = 0x00   采样分频（直接读寄存器时无关紧要）
  *    5. CONFIG     = 0x03   DLPF 低通 44Hz（加速度）/42Hz（陀螺），抑制手抖毛刺
  *    6. GYRO_CONFIG  = 0x00 ±250 °/s
  *    7. ACCEL_CONFIG = 0x00 ±2 g
  ******************************************************************************
  */
#include <string.h>
#include "mpu6050.h"
#include "soft_i2c.h"
#include "board_config.h"

/* 底层读写封装：全部走软件 IIC */
int MPU_Write_Len(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *buf)
{
    return SoftI2C_WriteRegs(addr, reg, buf, len);
}

int MPU_Read_Len(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *buf)
{
    return SoftI2C_ReadRegs(addr, reg, buf, len);
}

static int MPU_WriteReg(uint8_t reg, uint8_t val)
{
    return SoftI2C_WriteReg(MPU6050_ADDR_7BIT, reg, val);
}

int MPU6050_ReadWhoAmI(uint8_t *id)
{
    return SoftI2C_ReadReg(MPU6050_ADDR_7BIT, MPU_REG_WHO_AM_I, id);
}

int MPU6050_Init(void)
{
    uint8_t id = 0;
    int retry;
    uint32_t i;

    SoftI2C_Init();

    /* 1) 在位检查（给软件 IIC 一点重试余量，模块上电稳定需要时间） */
    for (retry = 0; retry < 10; retry++)
    {
        if ((MPU6050_ReadWhoAmI(&id) == 0) && (id == MPU6050_WHO_AM_I_VAL))
        {
            break;
        }
        /* 失败：先尝试解锁总线（从机上电瞬间可能拉住 SDA） */
        SoftI2C_BusRecover();
        for (i = 0; i < 200000u; i++) { __NOP(); }    /* ≈ 1ms 延时 */
    }
    if (retry >= 10 || id != MPU6050_WHO_AM_I_VAL)
    {
        return -1;                                      /* 器件不在位 / 接线错误 */
    }

    /* 2) 唤醒：退出睡眠 + 时钟源 = PLL（陀螺仪 Y 轴）
     *    ⚠️ 不使用 DEVICE_RESET(0x80)：
     *    复位需要 ~100ms 才完成，短延时后写寄存器会被复位过程覆盖/丢弃，
     *    现象就是"读数全 0"（器件保持默认 SLEEP 状态）。
     *    上电默认即 0x40(SLEEP)，直接写 0x01 唤醒即可，无需复位。 */
    if (MPU_WriteReg(MPU_REG_PWR_MGMT_1, 0x01u) != 0) { return -2; }

    /* 3) 唤醒后等时钟源切换稳定（PLL 锁定），≈10ms */
    for (i = 0; i < 600000u; i++) { __NOP(); }

    /* 4) 读回验证：确认 SLEEP 位(bit6) 已清零 —— 把"唤醒失败"显式暴露出来 */
    {
        uint8_t pwr = 0;
        if (SoftI2C_ReadReg(MPU6050_ADDR_7BIT, MPU_REG_PWR_MGMT_1, &pwr) != 0)
        {
            return -3;
        }
        if (pwr & 0x40u)                   /* SLEEP 仍为 1 = 唤醒失败 */
        {
            return -8;
        }
    }

    /* 4) 采样分频 */
    if (MPU_WriteReg(MPU_REG_SMPLRT_DIV, 0x00u) != 0) { return -4; }

    /* 5) DLPF = 3：加速度 44Hz / 陀螺 42Hz 低通（数据平滑，画波形好看） */
    if (MPU_WriteReg(MPU_REG_CONFIG, 0x03u) != 0) { return -5; }

    /* 6) 量程：陀螺 ±250°/s，加速度 ±2g */
    if (MPU_WriteReg(MPU_REG_GYRO_CONFIG, 0x00u)  != 0) { return -6; }
    if (MPU_WriteReg(MPU_REG_ACCEL_CONFIG, 0x00u) != 0) { return -7; }

    return 0;
}

int MPU6050_ReadRaw(int16_t accel[3], int16_t gyro[3])
{
    uint8_t buf[14];

    /* 0x3B 起连续 14 字节：ACCEL xyz(6) + TEMP(2) + GYRO xyz(6)，地址自动递增 */
    if (SoftI2C_ReadRegs(MPU6050_ADDR_7BIT, MPU_REG_ACCEL_XOUT_H, buf, 14u) != 0)
    {
        return -1;
    }

    accel[0] = (int16_t)(((uint16_t)buf[0]  << 8) | buf[1]);
    accel[1] = (int16_t)(((uint16_t)buf[2]  << 8) | buf[3]);
    accel[2] = (int16_t)(((uint16_t)buf[4]  << 8) | buf[5]);

    gyro[0]  = (int16_t)(((uint16_t)buf[8]  << 8) | buf[9]);
    gyro[1]  = (int16_t)(((uint16_t)buf[10] << 8) | buf[11]);
    gyro[2]  = (int16_t)(((uint16_t)buf[12] << 8) | buf[13]);

    return 0;
}
