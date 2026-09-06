#ifndef __MPU_DMP_H
#define __MPU_DMP_H

#include <stdint.h>

/* ============================================================================
 * DMP（Digital Motion Processor）接口层
 * ----------------------------------------------------------------------------
 * DMP 是 MPU6050 芯片内部的运动处理引擎：固件加载进芯片后，它在片内做
 * 姿态融合（互补/卡尔曼），主机只需从 FIFO 读出四元数，不用自己跑算法。
 *
 * 使用条件（三步，详见 docs/task2-接线与验收说明.md 第 6 节）：
 *   1. 拿到 InvenSense eMPL 库文件（正点原子 HAL 例程里有现成移植）：
 *        inv_mpu.c / inv_mpu.h
 *        inv_mpu_dmp_motion_driver.c / inv_mpu_dmp_motion_driver.h
 *        dmpKey.h / dmpmap.h
 *   2. 把它们加入 task2/Src、task2/Inc，并添加进 Keil 工程
 *   3. 在 board_config.h 里把 DMP_ENABLED 改成 1，重新编译
 *
 * 未启用时（DMP_ENABLED=0）：本文件是空实现，MPU_DMP_Read() 返回 -1，
 * Task_Mpu 自动只发送原始六轴（6 通道），任务一的验收不受影响。
 * ============================================================================*/

/* 0=关闭 DMP（默认），1=启用。见 board_config.h 说明 */
#ifndef DMP_ENABLED
#define DMP_ENABLED     (0)
#endif

/* 初始化 DMP（内部会加载固件到 MPU6050、配置 FIFO），0 成功 */
int MPU_DMP_Init(void);

/* 从 FIFO 读出欧拉角（单位：度）。pitch/roll 范围 ±90°/±180°，yaw 累计 ±180°，0 成功 */
int MPU_DMP_Read(float *pitch, float *roll, float *yaw);

#endif /* __MPU_DMP_H */
