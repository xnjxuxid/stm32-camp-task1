/**
  ******************************************************************************
  * @file    soft_i2c.c
  * @brief   软件 IIC（GPIO 位操作模拟）：SCL=PB0，SDA=PB1（开漏 + 上拉）
  *
  *  时序：标准 I2C，半位延时 4 µs（≈ 125 kHz），MPU6050 支持到 400 kHz，
  *        可把 SOFT_I2C_HALF_BIT_US 调到 2 提速。
  *
  *  延时基准：DWT->CYCCNT（168 MHz 循环计数），比空循环 for-loop 精确得多。
  ******************************************************************************
  */
#include "soft_i2c.h"
#include "board_config.h"

/* ---------------- 底层位操作宏 ---------------- */
#define SCL_H()   HAL_GPIO_WritePin(SOFT_I2C_GPIO_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_SET)
#define SCL_L()   HAL_GPIO_WritePin(SOFT_I2C_GPIO_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_RESET)
#define SDA_H()   HAL_GPIO_WritePin(SOFT_I2C_GPIO_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_SET)
#define SDA_L()   HAL_GPIO_WritePin(SOFT_I2C_GPIO_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_RESET)
#define SDA_IN()  (HAL_GPIO_ReadPin(SOFT_I2C_GPIO_PORT, SOFT_I2C_SDA_PIN) == GPIO_PIN_SET)

/* 开漏模式下读 SDA 前必须先释放总线（写 1） */
#define SDA_RELEASE() SDA_H()

/* ---------------- 微秒延时（DWT 循环计数器） ---------------- */
static void SoftI2C_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - start) < ticks)
    {
        /* busy wait */
    }
}

static void HalfBit(void)
{
    SoftI2C_DelayUs(SOFT_I2C_HALF_BIT_US);
}

/* ---------------- 初始化 ---------------- */
void SoftI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1) 使能 DWT 循环计数器（Cortex-M4 调试单元） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0u;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    /* 2) GPIO：开漏输出 + 上拉（空闲时总线被上拉为高） */
    SOFT_I2C_GPIO_CLK();
    GPIO_InitStruct.Pin   = SOFT_I2C_SCL_PIN | SOFT_I2C_SDA_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SOFT_I2C_GPIO_PORT, &GPIO_InitStruct);

    /* 3) 释放总线（空闲态：SCL/SDA 均为高） */
    SCL_H();
    SDA_H();
    HalfBit();
}

/* ---------------- 基础时序 ---------------- */
static void I2C_Start(void)
{
    SDA_RELEASE(); HalfBit();
    SCL_H();       HalfBit();
    SDA_L();       HalfBit();          /* SCL 高时 SDA 下跳 = 起始 */
    SCL_L();       HalfBit();
}

static void I2C_Stop(void)
{
    SCL_L();       HalfBit();
    SDA_L();       HalfBit();
    SCL_H();       HalfBit();
    SDA_H();       HalfBit();          /* SCL 高时 SDA 上跳 = 停止 */
}

/* 写 1 字节（MSB 先行），返回 0=收到 ACK，1=NACK */
static int I2C_WriteByte(uint8_t b)
{
    int i;
    for (i = 7; i >= 0; i--)
    {
        if ((b >> i) & 0x01u) { SDA_H(); } else { SDA_L(); }
        HalfBit();
        SCL_H();            /* SCL 高电平期间采样 */
        HalfBit();
        SCL_L();
    }
    /* 第 9 位：读应答 */
    SDA_RELEASE();
    HalfBit();
    SCL_H();
    HalfBit();
    {
        int nack = SDA_IN();        /* 高 = NACK */
        SCL_L();
        HalfBit();
        return nack;
    }
}

/* 读 1 字节（MSB 先行），ack=0 发 ACK 继续读，ack=1 发 NACK 结束 */
static uint8_t I2C_ReadByte(int ack)
{
    int i;
    uint8_t b = 0;

    SDA_RELEASE();                  /* 释放 SDA，由从机驱动 */
    for (i = 7; i >= 0; i--)
    {
        SCL_H();
        HalfBit();
        if (SDA_IN()) { b |= (uint8_t)(1u << i); }
        SCL_L();
        HalfBit();
    }
    /* 第 9 位：主机应答 */
    if (ack) { SDA_L(); } else { SDA_H(); }
    HalfBit();
    SCL_H();
    HalfBit();
    SCL_L();
    SDA_RELEASE();
    HalfBit();
    return b;
}

/* ---------------- 完整事务 ---------------- */

/*
 * 写寄存器（可连续写 len 字节）：START -> 设备地址+W -> ACK -> 寄存器地址 -> ACK
 *                            -> data[0..len-1] -> STOP
 */
int SoftI2C_WriteRegs(uint8_t devAddr7, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    int err = 0;

    I2C_Start();
    err |= I2C_WriteByte((uint8_t)(devAddr7 << 1));      /* 地址 + W(0) */
    err |= I2C_WriteByte(reg);
    for (i = 0; i < len; i++)
    {
        err |= I2C_WriteByte(buf[i]);
    }
    I2C_Stop();

    return err;                                          /* 0 = 全部 ACK */
}

/*
 * 读寄存器（可连续读，地址自动递增——MPU6050 的数据寄存器支持连续读）：
 * START -> 地址+W -> 寄存器地址 -> START(重复) -> 地址+R -> 读 len 字节 -> STOP
 */
int SoftI2C_ReadRegs(uint8_t devAddr7, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    int err = 0;

    I2C_Start();
    err |= I2C_WriteByte((uint8_t)(devAddr7 << 1));      /* 地址 + W(0) */
    err |= I2C_WriteByte(reg);                           /* 要读的寄存器 */

    I2C_Start();                                         /* 重复起始 */
    err |= I2C_WriteByte((uint8_t)((devAddr7 << 1) | 1u));   /* 地址 + R(1) */

    for (i = 0; i < len; i++)
    {
        buf[i] = I2C_ReadByte((i < (len - 1)) ? 1 : 0);  /* 最后一个字节回 NACK */
    }
    I2C_Stop();

    return err;
}

int SoftI2C_ReadReg(uint8_t devAddr7, uint8_t reg, uint8_t *val)
{
    return SoftI2C_ReadRegs(devAddr7, reg, val, 1u);
}

int SoftI2C_WriteReg(uint8_t devAddr7, uint8_t reg, uint8_t val)
{
    return SoftI2C_WriteRegs(devAddr7, reg, &val, 1u);
}

/*
 * 总线解锁：SDA 被从机拉死时使用。
 * 方法：SCL 手动打 9 个时钟让从机走完它的位计数，再发一次 STOP。
 */
void SoftI2C_BusRecover(void)
{
    int i;

    SCL_H();
    SDA_RELEASE();
    for (i = 0; i < 9; i++)
    {
        SCL_L(); HalfBit();
        SCL_H(); HalfBit();
        if (SDA_IN()) { break; }            /* SDA 释放了就提前结束 */
    }
    I2C_Stop();
}
