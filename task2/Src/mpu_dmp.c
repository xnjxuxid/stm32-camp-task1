/**
  ******************************************************************************
  * @file    mpu_dmp.c
  * @brief   DMP 接口层（可开关）
  *
  *  DMP_ENABLED = 0（默认）：空实现，工程立即可编译运行（只有原始六轴）。
  *  DMP_ENABLED = 1：需要先把 InvenSense eMPL 库文件放进工程（见头文件注释）。
  ******************************************************************************
  */
/* ⚠️ include 顺序：board_config.h 必须在 mpu_dmp.h 之前 ——
 * mpu_dmp.h 里有 #ifndef DMP_ENABLED #define 0 的兜底，
 * 顺序错了会用默认值 0 编译出空实现，导致 eMPL_* 链接错误 */
#include "board_config.h"
#include "mpu_dmp.h"

#if DMP_ENABLED

/* ============================================================================
 * 启用 DMP 时，需要以下 eMPL 库文件（正点原子 HAL 库例程中带现成移植）：
 *   inv_mpu.c / inv_mpu.h / inv_mpu_dmp_motion_driver.c / inv_mpu_dmp_motion_driver.h
 *   dmpKey.h / dmpmap.h
 *
 * 底层已在本工程实现（mpu6050.c）：
 *   MPU_Write_Len / MPU_Read_Len  -> 软件IIC（正点原子例程里这两个函数原本接硬件/软件IIC）
 *
 * 需要在正点原子 inv_mpu.c 里做的 3 处小适配：
 *   1) #include "delay.h" 改为不依赖（delay_ms 用 FreeRTOS 的 vTaskDelay 或 HAL_Delay）
 *   2) 它引用的 get_ms() 在本文件下方提供
 *   3) 头文件包含路径按 Keil 工程实际目录调整
 * ============================================================================*/
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "mpu6050.h"
#include "board_config.h"
#include "soft_i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * eMPL 平台接口实现（stm32_mpu6050.h 把 i2c_write 等宏映射到这里）
 * ============================================================================*/
int eMPL_i2c_write(unsigned char slave_addr, unsigned char reg_addr,
                   unsigned char length, unsigned char const *data)
{
    return SoftI2C_WriteRegs(slave_addr, reg_addr, data, length);
}

int eMPL_i2c_read(unsigned char slave_addr, unsigned char reg_addr,
                  unsigned char length, unsigned char *data)
{
    return SoftI2C_ReadRegs(slave_addr, reg_addr, data, length);
}

void eMPL_delay_ms(unsigned long num_ms)
{
    /* 初始化在任务上下文里调用，用 vTaskDelay 让出 CPU */
    vTaskDelay(pdMS_TO_TICKS(num_ms));
}

int eMPL_get_ms(unsigned long *count)
{
    *count = (unsigned long)(xTaskGetTickCount());
    return 0;
}

int MPU_DMP_Init(void)
{
    long gyroBias[3]  = {0, 0, 0};
    long accelBias[3] = {0, 0, 0};
    int  st;

    /* ---- step 1：器件层初始化（复用已配置的软件 IIC） ---- */
    printf("DMP: 1/6 mpu_init\r\n");
    if (mpu_init() != 0) { return -1; }

    /* ---- step 2：使能传感器 + FIFO ---- */
    printf("DMP: 2/6 sensors + fifo\r\n");
    if (mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL) != 0)        { return -2; }
    if (mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL) != 0)     { return -3; }
    if (mpu_set_sample_rate(200) != 0)                             { return -4; }

    /* ---- step 3：加载 DMP 固件（~3KB 走软件 IIC，需 1~2 秒） ---- */
    printf("DMP: 3/6 load firmware\r\n");
    if (dmp_load_motion_driver_firmware() != 0)                    { return -5; }

    /* ---- step 4：FIFO 输出率 + 特性 ---- */
    printf("DMP: 4/6 fifo rate + features\r\n");
    if (dmp_set_fifo_rate(200) != 0)                               { return -6; }
    if (dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT |
                           DMP_FEATURE_GYRO_CAL) != 0)             { return -7; }

    /* ---- step 5：自检 + 设置偏置（eMPL 标准流程，缺了它四元数输出零噪声！）----
     * ⚠️ 自检期间必须保持模块完全静止约 5 秒 */
    printf("DMP: 5/6 self test - KEEP MPU STILL ~5s\r\n");
    st = mpu_run_self_test(gyroBias, accelBias);
    printf("DMP: self test result = 0x%02X (1=gyro, 2=accel, 3=both)\r\n", st);

    if ((st & 0x03) == 0x03)      /* 陀螺 + 加速度都通过 */
    {
        mpu_set_gyro_bias_reg(gyroBias);
        mpu_set_accel_bias_6050_reg(accelBias);
        printf("DMP: bias applied\r\n");
    }
    else
    {
        printf("DMP: self test FAILED - continue without bias\r\n");
    }

    /* ---- step 5.5：清除自检残留（陀螺 -7000 恒定偏移的元凶） ---- */
    if (MPU6050_ClearSelfTest() != 0) { return -9; }
    {
        uint8_t stx = 0xFF, gc = 0xFF;
        (void)SoftI2C_ReadReg(MPU6050_ADDR_7BIT, 0x0D, &stx);
        (void)SoftI2C_ReadReg(MPU6050_ADDR_7BIT, MPU_REG_GYRO_CONFIG, &gc);
        printf("DMP: self-test regs cleared (ST_X=0x%02X GYRO_CFG=0x%02X)\r\n", stx, gc);
    }

    /* ---- step 6：进入 DMP 模式 ---- */
    printf("DMP: 6/6 enable DMP\r\n");
    if (mpu_set_dmp_state(1) != 0) { return -8; }

    printf("DMP: init OK\r\n");
    return 0;
}

int MPU_DMP_Read(float *pitch, float *roll, float *yaw)
{
    short quat[4];               /* q30 定点四元数 */
    short sensors;
    unsigned char more;
    unsigned long timestamp;
    static int failCnt = 0;

    {
        int r = dmp_read_fifo(quat, 0, 0, &timestamp, &sensors, &more);

#if DMP_DEBUG
        /* 诊断：约每 1 秒打印一次 FIFO 原始内容。
         * 四元数恒为 0 1073741824 0 0 (= q 0,1,0,0) 表示 DMP 引擎没在更新姿态 */
        {
            static int dbgCnt = 0;
            if (++dbgCnt >= 200)
            {
                dbgCnt = 0;
                printf("fifo r=%d sensors=0x%04X more=%u q=%d %d %d %d\r\n",
                       r, sensors, more,
                       quat[0], quat[1], quat[2], quat[3]);
            }
        }
#endif

        if (r != 0)
        {
            /* 连续失败多半是 FIFO 溢出（读取跟不上）——复位 FIFO 自恢复，
             * 否则溢出后 dmp_read_fifo 会永远失败 */
            if (++failCnt >= 20)
            {
                failCnt = 0;
                (void)mpu_reset_fifo();
            }
            return -1;           /* FIFO 空或溢出（本次没有新数据） */
        }
    }
    failCnt = 0;
    if ((sensors & INV_WXYZ_QUAT) == 0)
    {
        return -2;               /* 这帧不是四元数 */
    }

    /* q30 定点 -> 浮点 -> 欧拉角（yaw/pitch/roll 公式见 InvenSense 应用笔记） */
    {
        float q0 = quat[0] / 1073741824.0f;      /* 2^30 */
        float q1 = quat[1] / 1073741824.0f;
        float q2 = quat[2] / 1073741824.0f;
        float q3 = quat[3] / 1073741824.0f;

        *pitch = asinf(-2.0f * q1 * q3 + 2.0f * q0 * q2) * 57.2957795f;
        *roll  = atan2f(2.0f * q2 * q3 + 2.0f * q0 * q1,
                        -2.0f * q1 * q1 + 2.0f * q0 * q0 - 1.0f) * 57.2957795f;
        *yaw   = atan2f(2.0f * (q1 * q2 + q0 * q3),
                        q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 57.2957795f;
    }
    return 0;
}

#else /* !DMP_ENABLED：空实现，保证工程立即可编译运行 */

int MPU_DMP_Init(void)
{
    return -1;      /* 未启用 */
}

int MPU_DMP_Read(float *pitch, float *roll, float *yaw)
{
    (void)pitch; (void)roll; (void)yaw;
    return -1;
}

#endif /* DMP_ENABLED */
