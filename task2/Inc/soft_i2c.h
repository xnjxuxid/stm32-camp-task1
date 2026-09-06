#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "stm32f4xx_hal.h"

/* ============================================================================
 * 软件 IIC（GPIO 位操作模拟，开漏输出 + 上拉）
 * ----------------------------------------------------------------------------
 * 为什么用软件 IIC 而不是硬件 I2C（任务书思考题，答辩要点）：
 *  1) 硬件 I2C 一旦时序异常（从机拉死 SDA）可能锁死总线，恢复要复杂的手册流程；
 *     软件 IIC 出问题随时可以用"发 9 个时钟 + Stop"解锁，可控性完全在自己手里；
 *  2) 硬件 I2C 引脚固定（F4 的 I2C1 在 PB6/PB9 等复用脚上），本项目 PB6/PB7 已被
 *     串口占用、PB8/PB9 被 CAN 占用，软件 IIC 可以用任意空闲 GPIO（PB0/PB1）；
 *  3) 软件 IIC 时序参数（半位延时）可调，方便匹配不同速率的从机；
 *  4) 代价：占用 CPU 时间（阻塞式）。本项目 5 ms 周期读 14 字节约 2 ms，可接受。
 * ==========================================================================*/

void    SoftI2C_Init(void);

/* 完整事务接口（推荐使用） */
int     SoftI2C_WriteRegs(uint8_t devAddr7, uint8_t reg, const uint8_t *buf, uint16_t len);
int     SoftI2C_ReadRegs (uint8_t devAddr7, uint8_t reg,       uint8_t *buf, uint16_t len);
int     SoftI2C_ReadReg  (uint8_t devAddr7, uint8_t reg,       uint8_t *val);       /* 读单字节 */
int     SoftI2C_WriteReg (uint8_t devAddr7, uint8_t reg,       uint8_t val);        /* 写单字节 */

/* 总线解锁：SDA 被从机拉死时，发 9 个时钟 + STOP 释放总线 */
void    SoftI2C_BusRecover(void);

#endif /* __SOFT_I2C_H */
