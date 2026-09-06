#ifndef _STM32_MPU6050_H_
#define _STM32_MPU6050_H_
/* ============================================================================
 * eMPL 平台适配层（本工程定制版，重写自 riverzhou/mpu6050 的同名文件）
 * ----------------------------------------------------------------------------
 * InvenSense eMPL（inv_mpu.c / inv_mpu_dmp_motion_driver.c）要求平台提供：
 *   i2c_write(slave_addr, reg_addr, length, const *data)
 *   i2c_read (slave_addr, reg_addr, length, *data)
 *   delay_ms (num_ms)
 *   get_ms   (*count)
 *   min(a,b) / labs / fabsf
 *   reg_int_cb(...)   —— 本工程不使用 INT 中断，空实现
 *
 * 映射到本工程的实现（在 mpu_dmp.c 中定义）：
 *   i2c_write -> eMPL_i2c_write  -> SoftI2C_WriteRegs（软件 IIC，PB0/PB1）
 *   i2c_read  -> eMPL_i2c_read   -> SoftI2C_ReadRegs
 *   delay_ms  -> eMPL_delay_ms   -> vTaskDelay（任务上下文，不阻塞系统）
 *   get_ms    -> eMPL_get_ms     -> xTaskGetTickCount
 * ============================================================================*/
#include <math.h>
#include <stdlib.h>
#include "stm32f4xx_hal.h"    /* __NOP 等 CMSIS intrinsic（eMPL 源码里有调用） */

int  eMPL_i2c_write(unsigned char slave_addr, unsigned char reg_addr,
                    unsigned char length, unsigned char const *data);
int  eMPL_i2c_read(unsigned char slave_addr, unsigned char reg_addr,
                   unsigned char length, unsigned char *data);
void eMPL_delay_ms(unsigned long num_ms);
int  eMPL_get_ms(unsigned long *count);

#define i2c_write   eMPL_i2c_write
#define i2c_read    eMPL_i2c_read
#define delay_ms    eMPL_delay_ms
#define get_ms(x)   do { (void)eMPL_get_ms(x); } while (0)
#define min(a,b)    ((a<b)?a:b)

/* 本工程轮询读取 FIFO，不使用 INT 引脚中断回调 */
#define reg_int_cb(cb, port, pin)   do {} while (0)

#endif /* _STM32_MPU6050_H_ */
