# 任务一 完整工程：接线 + 配置 + 验收

> 目标板：**立创·梁山派·天空星 STM32F407VGT6**（HSE 8 MHz / 168 MHz）
> 工程：**`桌面\test\task1\MDK-ARM\task1.uvprojx`** —— 双击打开，直接编译下载
>
> ✅ **不需要再点 CubeMX 的 GENERATE CODE**：
> CAN / DMA / TIM7 的初始化已手写完成（`can.c` / `dma.c` / `tim.c`），与 CubeMX 生成等价。
> `task1.ioc` 只是配置记录，重新生成反而会造成中断函数重复定义（见第八节）。

---

## 一、工程里哪些文件是关键

| 文件 | 作用 |
|---|---|
| `Inc/board_config.h` | ★ 引脚、队列长度、CAN 协议**全部集中在这里**，改配置只看这个 |
| `Src/can_bus.c` | CAN 接收中断：**只打包入队**（队列长度 6），不解析 |
| `Src/app_tasks.c` | 三个任务：`Task_CanRx`（解析+发通知）/ `Task_Breath`（等通知）/ `Task_UartEcho`（回传） |
| `Src/breath_led.c` | 呼吸灯：**TIM7 中断更新 PWM**，不依赖任何任务 |
| `Src/uart_vofa.c` | 空闲中断入队（队列长度 3）+ DMA 发送回 VOFA |
| `Src/can.c` `Src/dma.c` | CAN1(500k) 与 DMA 初始化 |
| `Src/tim.c` | 新增 `MX_TIM7_Init`（1 kHz 节拍） |
| `Src/stm32f4xx_it.c` | 新增 CAN1_RX0 / TIM7 / DMA2_Stream7 中断入口 |
| `Src/freertos.c` | `MX_FREERTOS_Init()` 里调用 `App_Tasks_Create()` |
| `Src/main.c` | 外设初始化 + CAN 接收回调 + TIM7 呼吸回调 |

**数据流**（答辩时画这张图）：

```
CAN 中断 ──入队(6)──> Task_CanRx ──任务通知(新周期值)──> Task_Breath ──> 改呼吸周期
                                                              │
USART 空闲中断 ──入队(3)──> Task_UartEcho ──DMA发送──> VOFA     │
                                                              ▼
                        TIM7(1kHz中断) + TIM3(PWM) ──> PB4 LED 呼吸（不依赖任务）
```

---

## 二、硬件清单（BOM）

| 器件 | 规格 | 数量 | 用途 | 状态 |
|---|---|---|---|---|
| 主控 | 立创天空星 STM32F407VGT6 | 1 | 主控 | ✅ 有 |
| CAN 收发器 | **TJA1050**（**5 V** 供电） | 1~2 | 天空星 1 个 + 对端 1 个 | ❌ 约 5 元 |
| CAN 对端 | USB-CAN 分析仪 **或** F103C8T6+收发器 | 1 | 应答 ACK + 发测试帧 | ⚠️ 见第九节 |
| USB-TTL | CH340/CP2102，**跳线 3.3V** | 1 | 串口调试 | ✅ 有 |
| LED + 1 kΩ | 直插 | 1 套 | 呼吸灯（PB4） | 需备 |
| 120 Ω 电阻 | — | 2 | CAN 终端电阻（模块常自带跳线） | 视模块 |

---

## 三、接线（逐脚，按板子丝印核对）

### 3.1 呼吸灯
```
PB4 ──[1 kΩ]── LED 阳极(长脚) ──▶│── LED 阴极(短脚) ── GND
```
> ⚠️ 不能用 PA6（板载 W25Q128 的 MISO）；板载 LED 在 PB2，但**没有定时器通道，做不了呼吸灯**。

### 3.2 CAN（TJA1050，**5 V 供电**）

| 模块引脚 | 天空星 | 说明 |
|---|---|---|
| TXD | **PB9** (CAN1_TX) | MCU 的 3.3 V 输出可直接驱动（TJA1050 VIH ≥ 2.0 V） |
| RXD | **PB8** (CAN1_RX) | 模块输出高电平 ≈ 5 V；PB8 是 **FT（5V 容忍）引脚**可直连，稳妥起见可串 1 kΩ |
| VCC | **5 V** | ⚠️ TJA1050 必须接 5 V（板子 5V 排针），接 3.3V 会工作异常 |
| GND | **GND（必须共地）** | |
| CANH / CANL | 对端 CANH / CANL | 双绞，两端各 1 个 120 Ω（模块跳线） |

> ⚠️ **丝印对应别搞反**：TJA1050 模块丝印是 **TXD / RXD**（不是 CANRX/CANTX）。
> 对应关系：**MCU 的 TX（PB9）→ 模块 TXD**，**MCU 的 RX（PB8）← 模块 RXD**（仍是"同名相对"，MCU 的 TX 接模块的 TXD）。
>
> 说明：TJA1050 是 5 V 器件，无待机模式（比 SN65HVD230 略耗电）；500 kbps 完全支持。
> F407 的 PB8 与 F103 的 PA11 都是 FT 引脚，5 V 信号直连安全。

### 3.3 串口（VOFA）

| USB-TTL | 天空星 |
|---|---|
| **RX** | **PB6** (USART1_TX) |
| **TX** | **PB7** (USART1_RX) |
| GND | GND（共地） |
| VCC | **不接**（跳线必须在 3.3V） |

参数：**115200 / 8 / N / 1**

### 3.4 总表

| 引脚 | 功能 | 连到 |
|---|---|---|
| PB4 | TIM3_CH1 PWM | LED + 1 kΩ → GND |
| PB8 | CAN1_RX | 收发器 CANRX |
| PB9 | CAN1_TX | 收发器 CANTX |
| PB6 | USART1_TX | USB-TTL **RX** |
| PB7 | USART1_RX | USB-TTL **TX** |
| PA13/PA14 | SWD | 板载 DAP-Link |
| 3.3V/GND | 电源 | 收发器、USB-TTL 共地 |

---

## 四、CubeMX 配置参数表（核对 / 答辩用）

### 时钟
| 项 | 值 |
|---|---|
| HSE 输入 | **8 MHz**（默认 25，必须改） |
| PLL M/N/P | **4 / 168 / 2** |
| SYSCLK | 168 MHz；AHB 168 / APB1 42 / APB2 84 MHz |

### SYS
| 项 | 值 | 原因 |
|---|---|---|
| Debug | **Serial Wire** | 释放 PB4 才能当 PWM |
| Timebase | **TIM1** | SysTick 给 FreeRTOS |

### FreeRTOS（CMSIS_V2）
TICK_RATE_HZ=**1000**、TOTAL_HEAP_SIZE=**15360**、heap_4、**USE_TASK_NOTIFICATIONS=Enabled**、**USE_MUTEXES=Enabled**

### TIM3（呼吸 PWM）
引脚 **PB4**=TIM3_CH1；PSC=**83**、Period=**999** → 84 MHz/84/1000 = **1 kHz PWM**

### TIM7（呼吸节拍）
PSC=**83**、Period=**999** → **1 kHz 更新中断**；NVIC 优先级 6

### CAN1
引脚 **PB8(RX)/PB9(TX)**；Mode=Normal；**PSC=6、BS1=9TQ、BS2=4TQ、SJW=1TQ**
→ 42 MHz / 6 / (1+9+4) = **500 kbps**；NVIC 优先级 **5**

### USART1 + DMA
引脚 **PB6(TX)/PB7(RX)**；115200/8/N/1
- RX：**DMA2_Stream2 / Channel4 / Circular**
- TX：**DMA2_Stream7 / Channel4 / Normal**
- NVIC：USART1_IRQn=5、DMA2_Stream7_IRQn=5

> ⚠️ NVIC 优先级说明：在中断里调用 `xxxFromISR()` 的中断，**抢占优先级数值必须 ≥ 5**
> （`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`）。

---

## 五、通信协议

### 5.1 CAN 帧（8 字节）

```
[0]      [1]     [2..3]        [4]      [5..6]   [7]
0xAA头帧  CMD    数据(大端u16) 校验和    保留0    0x55尾帧
校验和 = (CMD + DATA_H + DATA_L) & 0xFF
```

| CMD | 含义 | DATA |
|---|---|---|
| 0x01 | 修改呼吸周期 | 周期毫秒数（如 500 = 0.5 s 一个来回） |
| 0x02 | 熄灭呼吸灯 | — |
| 0x03 | 点亮呼吸灯 | — |

**示例**：把呼吸周期改成 1000 ms →
`AA 01 03 E8 EC 00 00 55`（03E8=1000；校验和=01+03+E8=0xEC）

### 5.2 串口回传格式（任务书硬性要求）

VOFA/串口助手发任意字符串，MCU 回：
```
Receive Data ：（你发的内容）\n
```
（注意是**全角冒号**和**全角括号**，代码里已按此实现）

---

## 六、验收步骤

1. **上电**：串口助手收到
   `Task1 start: CAN1 500k / Breath LED TIM3+TIM7 / UART DMA 115200`
2. **呼吸灯**：PB4 外接 LED 以 2 秒周期呼吸（默认）。
3. **串口回传**：发送 `hello` → 收到 `Receive Data ：（hello）`✅
4. **CAN 改频率**：对端发 `AA 01 03 E8 EC 00 00 55` → 呼吸周期变成 1 秒。
5. **CAN 加速**：发 `AA 01 00 64 65 00 00 55`（100 ms）→ 呼吸明显变快。
6. **CAN 关灯**：发 `AA 02 00 00 02 00 00 55` → 灯灭。
7. **CAN 开灯**：发 `AA 03 00 00 03 00 00 55` → 恢复呼吸。
8. **关键验收点**：**不发任何 CAN 帧时，呼吸灯一直按上次设定的频率呼吸**
   （因为呼吸由 TIM7 中断 + TIM3 PWM 硬件产生，`Task_Breath` 阻塞不影响它）。

---

## 七、故障排查

| 现象 | 排查 |
|---|---|
| CAN 一帧都收不到 | ① 总线上**必须两个节点 + 两端各 120 Ω + 共地**；② 波特率两端都是 500k；③ 收发器 VCC 有电；④ PB8/PB9 没接反 |
| CAN  intermittent / 总线关闭 | 没接终端电阻；或只有一个节点（没人给 ACK，必然失败） |
| 串口收不到回传 | TX/RX 是否交叉（PB6→USB-TTL 的 RX）；GND 共地；COM 口选的是 USB-TTL 那个 |
| 灯不呼吸（常亮/常灭） | 检查 TIM7 是否进了中断；PB4 是否配成 TIM3_CH1；LED 方向 |
| 编译报 `Undefined symbol` | 新加的 `.c` 没进工程：Project 树 Application/User 分组里应有 can.c / dma.c / can_bus.c / uart_vofa.c / breath_led.c |
| 链接报 `Undefined symbol HAL_CAN_xxx` | ⭐ **CAN 的 HAL 驱动源文件没进工程**（CubeMX 没配过 CAN 就不会加它）。确认 Project 树 **Drivers/STM32F4xx_HAL_Driver** 分组里有 `stm32f4xx_hal_can.c`；没有就右键该分组 → **Add Existing Files to Group** → 到 `Drivers\STM32F4xx_HAL_Driver\Src\` 选中它。同理，以后新开一个外设（SPI/I2C/ADC…），都要检查对应的 `stm32f4xx_hal_xxx.c` 在不在工程里 |
| 编译报 CAN 相关未定义 | `stm32f4xx_hal_conf.h` 里 `HAL_CAN_MODULE_ENABLED` 要打开（已开） |
| 一运行就 HardFault | 任务栈太小；或中断优先级 < 5 却调用了 FromISR API |

---

## 八、关于「用 CubeMX 重新生成」

本工程已能直接编译下载。**如果确实要用 CubeMX 打开 `task1.ioc` 并 GENERATE CODE**，注意：

1. CubeMX 会**覆盖** `can.c` / `dma.c` / `tim.c`（生成版本与手写版等价，OK）；
2. 但会在 `stm32f4xx_it.c` 里**再生成一份** `CAN1_RX0_IRQHandler` / `TIM7_IRQHandler` / `DMA2_Stream7_IRQHandler`，
   而我在 `USER CODE BEGIN 1` 区写的同名函数会被保留 → **重复定义，编译报错**。
   → 重新生成后，把 `stm32f4xx_it.c` 里 `USER CODE BEGIN 1` 区中那三个函数**删掉**即可。
3. 重新生成后请重新检查：`stm32f4xx_hal_conf.h` 的 `HAL_CAN_MODULE_ENABLED`、
   `FreeRTOSConfig.h` 的 `configUSE_TASK_NOTIFICATIONS`。

**建议：不改配置就别重新生成。**

---

## 九、已定稿的参数（2026-09-05 确认）

| 项目 | 定稿值 | 改的话动哪里 |
|---|---|---|
| CAN 对端方案 | **方案 A：F103C8T6 + SN65HVD230** | 见 `can对端-F103工程说明.md` |
| CAN 波特率 | **500 kbps**（两端一致） | `can.c` 的 `hcan1.Init.Prescaler` + `task1.ioc` |
| 协议帧 | `AA / CMD / DH / DL / SUM / 00 / 00 / 55` | `Inc/board_config.h` |
| Git | 暂缓 | — |

**配套工程**：
- 任务一主体：`桌面\test\task1\MDK-ARM\task1.uvprojx`
- CAN 对端：`桌面\test\can_sender\can_sender.ioc`（见 `can对端-F103工程说明.md`）
