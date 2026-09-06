# 华南虎夏令营 · 任务一（CAN + 呼吸灯 + 串口 DMA）

> 主控：立创·梁山派·天空星 **STM32F407VGT6**（HSE 8 MHz → 168 MHz）
> CAN 对端：**STM32F103C8T6** + TJA1050（5 V 供电，自制信号源）
> 栈：**HAL + FreeRTOS (CMSIS_V2)**，CubeMX 生成骨架 + 手写外设初始化

## 仓库结构

```
test/
├─ led_blink/      # 02/03 练习工程：点灯 → printf → FreeRTOS 三任务（task1 的前身）
├─ task1/          # ★ 任务一正式工程（CAN 队列 + 任务通知 + 呼吸灯 + 串口 DMA 回传）
├─ can_sender/     # F103C8T6 CAN 对端：每 500ms 发帧，5s 自动切换呼吸周期
└─ docs/           # 接线说明、验收步骤、踩坑记录
```

## 任务一数据流（任务书要求 → 实现）

```
CAN 接收中断 ──入队(长度6)──> Task_CanRx ──任务通知──> Task_Breath ──> 修改呼吸周期
USART 空闲中断 ──入队(长度3)──> Task_UartEcho ──DMA 发送──> VOFA："Receive Data ：（%s）\n"
呼吸波形 = TIM7(1kHz 中断) + TIM3(PWM, PB4) —— 硬件驱动，不依赖任何任务
```

任务书要求"即便没有通知，呼吸灯仍按之前的频率运行"：呼吸由 TIM7 中断 + TIM3 PWM 硬件产生，
`Task_Breath` 只负责收通知改周期，阻塞与否不影响呼吸 → 满足。

## 引脚分配（天空星专属坑位）

| 功能 | 引脚 | 说明 |
|---|---|---|
| 呼吸灯 PWM | **PB4** (TIM3_CH1) | 不能用 PA6（板载 W25Q128 占用） |
| CAN1 | **PB8(RX) / PB9(TX)** | 避开 USB 的 PA11/PA12；500 kbps |
| VOFA 串口 | **PB6(TX) / PB7(RX)** | PA9/PA10 被板载 DAP-Link 占用 |
| 下载 | PA13/PA14 (SWD) | 板载 DAP-Link |

## 踩坑全记录（真实调试过程，面试可讲）

| # | 坑 | 现象 | 根因 | 修复 |
|---|---|---|---|---|
| 1 | printf 卡死 | LED 半亮后程序停死 | 无 fputc 重定向 → 半主机模式 BKPT | usart.c 加 `fputc`→`HAL_UART_Transmit` |
| 2 | Keil 新建文件位置 | `cannot open source input file` | Add New Item 默认丢进 MDK-ARM 目录，不在 Include Paths | `.c` 放 `Src/`，`.h` 放 `Inc/` |
| 3 | 死代码 | 02 的呼吸 while(1) 不再执行 | 它位于 `osKernelStart()` 之后，调度器接管后永远不执行 | 删除，逻辑挪进任务 |
| 4 | 任务创建时机 | CMSIS-V2 规范告警 | 在 `osKernelInitialize()` 之前创建任务 | 挪进 `freertos.c` 的 `MX_FREERTOS_Init()` |
| 5 | CAN 链接错误 | 6 个 `Undefined symbol HAL_CAN_xxx` | 新外设的 HAL 驱动 `stm32f4xx_hal_can.c` 不在工程 | 手动 Add Existing Files |
| 6 | 串口收不到 | PB6 发送、DAP-Link 听不到 | 板载 DAP-Link 串口连的是 PA9/PA10 | 外接 USB-TTL 接 PB6/PB7（或引脚切回 PA9/PA10） |
| 7 | F103 启动卡死 | printf 打印半截戛然而止 | CubeMX 漏生成 `SysTick_Handler`，上电 1ms 第一次 SysTick 中断跳进 startup 的 WEAK 死循环 | it.c 手动补 `SysTick_Handler(){HAL_IncTick();}` |
| 8 | F103 假发送成功 | 一直打印 TX OK 但没人收到 | `HAL_CAN_AddTxMessage` 返回 OK 只代表"进了邮箱" | 读 `CAN->ESR`（LEC/BOFF/TEC）真实诊断 + Bus-Off 自动恢复 |

## 验收结果（2026-09-05 联合验收）

| 验收项 | 结果 |
|---|---|
| CAN 帧 → 队列(6) → 解析 → 任务通知 → 呼吸频率改变 | ✅ |
| 无通知时保持原频率呼吸（TIM7+PWM 硬件驱动） | ✅ |
| 串口不定长接收 → 队列(3) → DMA 回传 `Receive Data ：（%s）\n` | ✅ |
| F103 对端每 5s 自动切换周期（2s/1s/0.4s） | ✅ |

## Git 工作流

- `main`：任务一交付版本（tag: `v1.0-task1`）
- `task2-dev`：任务二（MPU6050 软件 IIC + DMP）开发分支
- commit 规范：`feat/fix/docs/chore(scope): 描述`，一条 commit 一个可验证的进展点
