# 任务二开发计划（task2-dev 分支）

> 任务书要求：软件 IIC 读 MPU6050 原始六轴 → DMP 解算 Pitch/Roll/Yaw → VOFA 5 ms 绝对周期打印。
> 本分支用于任务二开发，完成并验收后合回 `main`（`git merge --no-ff task2-dev`）。

## 目标板与引脚（复用任务一工程结构）

| 功能 | 引脚 | 说明 |
|---|---|---|
| 软件 IIC SCL | **PB8**（开漏+上拉） | 任务一的 CAN 引脚，两个工程独立不冲突 |
| 软件 IIC SDA | **PB9**（开漏+上拉） | 同上 |
| MPU6050 AD0 | GND | 器件地址 0x68 |
| VOFA 串口 | PB6/PB7 (USART1) | 与任务一相同 |

## 开发步骤

- [ ] `soft_i2c.c/h`：GPIO 开漏 + DWT 精确延时的位操作 IIC
- [ ] WHO_AM_I 校验（应答 0x68）
- [ ] 原始六轴读取（加速度 0x3B 起、陀螺仪 0x43 起），VOFA 打 6 条曲线
- [ ] 移植 InvenSense DMP 库（inv_mpu / inv_mpu_dmp_motion_driver），接软件 IIC 底层
- [ ] 四元数→欧拉角，VOFA 打 3 条曲线（Pitch/Roll/Yaw）
- [ ] `vTaskDelayUntil` 实现**绝对周期 5 ms**（VOFA 时间戳验收）

## 验收标准（任务书）

1. 原始数据 6 通道，5 ms 周期打印（VOFA 时间戳验证）
2. DMP 三轴角度 3 通道，5 ms 周期打印
3. 手掰板子，曲线平滑无跳变；转一圈 Yaw 变化 ≈ 360°
