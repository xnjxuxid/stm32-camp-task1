# CAN 对端（信号源）工程：STM32F103C8T6

> 用途：给任务一提供 **CAN 总线的第二个节点**。
> CAN 协议规定发送方必须收到至少一个节点的 ACK 应答，**没有第二个节点，天空星一帧也收不到**。
> 这个工程每 500 ms 发一帧控制报文，每 5 秒换一个呼吸周期，天空星收到后呼吸灯频率会跟着变。

---

## 一、你会看到什么

| 时间 | F103 串口打印 | 天空星表现 |
|---|---|---|
| 0~5 s | `[1] TX ok : ... (breath period = 2000 ms)` | 呼吸灯 2 秒一个来回 |
| 5~10 s | `(breath period = 1000 ms)` | 呼吸变快（1 秒） |
| 10~15 s | `(breath period = 400 ms)` | 呼吸更快（0.4 秒） |
| 15 s 后 | 回到 2000 ms | 循环 |

**这个"自动变化"的设计，是为了让你录验收视频时不用重新烧录程序就能看到频率改变。**

---

## 二、硬件

| 器件 | 数量 | 说明 |
|---|---|---|
| STM32F103C8T6 最小系统板 | 1 | 你手上已有 |
| SN65HVD230 CAN 收发器模块 | 1 | 3.3 V 版本（和天空星那个一样，共买 2 个） |
| ST-Link | 1 | 下载用（你已有） |
| USB-TTL（可选） | 1 | 看 F103 的打印信息，便于排查 |
| 杜邦线 | 若干 | |

---

## 三、接线

### 3.1 F103 ↔ 收发器（模块 A）

| F103C8T6 | SN65HVD230 模块 |
|---|---|
| **PA11** (CAN_RX) | CANRX |
| **PA12** (CAN_TX) | CANTX |
| 3.3V | VCC |
| GND | GND |

### 3.2 天空星 ↔ 收发器（模块 B）

| 天空星 | SN65HVD230 模块 |
|---|---|
| **PB8** (CAN1_RX) | CANRX |
| **PB9** (CAN1_TX) | CANTX |
| 3.3V | VCC |
| GND | GND |

### 3.3 两个收发器互连（CAN 总线）

```
模块 A                                          模块 B
CANH ●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━● CANH
CANL ●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━● CANL
GND  ●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━● GND   ← 必须共地！

两端各 1 个 120 Ω 终端电阻（收发器模块上通常有跳线帽，短接即可）
```

> ⚠️ **三不**：不能只有 1 个节点；不能忘接 120 Ω；不能不共地。
> 这三条任一不满足，CAN 就无法通信（会一直打印 `TX FAIL`）。

### 3.4 F103 串口（可选，看打印）

| USB-TTL | F103C8T6 |
|---|---|
| RX | PA9 (USART1_TX) |
| TX | PA10 (USART1_RX) |
| GND | GND |

115200 / 8 / N / 1

---

## 四、建工程（3 步，5 分钟）

### 第 1 步：CubeMX 生成

1. 双击 `桌面\test\can_sender\can_sender.ioc`（会自动用 CubeMX 打开）
2. 检查右下角/顶部芯片型号是 **STM32F103C8Tx**，不是就手动改
3. **GENERATE CODE**（或 Ctrl+S）→ 用 **MDK-ARM** 打开工程

> 如果 CubeMX 提示缺 F1 固件包：Pack Installer 里装 **STM32Cube FW_F1**（你电脑上已有 1.8.7）

### 第 2 步：覆盖 main.c

把 **`桌面\test\can_sender\_template\main.c`** 复制，覆盖到
CubeMX 生成的 `桌面\test\can_sender\Src\main.c`。

> 说明：CubeMX 生成时一般会保留 `USER CODE` 区，所以生成后先打开 `Src/main.c`
> 看一眼 —— 如果 `printf` 重定向和 while 循环里的发送代码**还在**，就不用覆盖；
> 不在就用 `_template\main.c` 覆盖。
>
> 只动这一个文件，`can.c` / `usart.c` / `gpio.c` 都用 CubeMX 生成的。

### 第 3 步：编译下载

1. Keil 里 **Options for Target → Target** 勾选 **Use MicroLIB**（printf 要用）
2. **Options → Debug** 选你的下载器（ST-Link）→ Settings → Flash Download → Add
   → 选 **STM32F1xx Flash 128k**（C8T6 标称 64k，选 128k 或 64k 都行，算法要存在）
3. Rebuild（F7）→ Download（F8）

---

## 五、CubeMX 配置参数（若 .ioc 打不开，照这个手动配）

| 类别 | 配置项 | 值 |
|---|---|---|
| 芯片 | — | **STM32F103C8Tx**（LQFP48） |
| SYS | Debug | **Serial Wire** |
| RCC | High Speed Clock | **Crystal/Ceramic Resonator**（板载 8 MHz） |
| 时钟 | HSE → PLL ×9 | **SYSCLK 72 MHz**，APB1 = **36 MHz**，APB2 = 72 MHz |
| CAN | Mode | **Master**；引脚 **PA11=CAN_RX，PA12=CAN_TX** |
| CAN 参数 | Prescaler / BS1 / BS2 / SJW | **4 / 13TQ / 4TQ / 1TQ** → 36 MHz ÷ 4 ÷ 18 = **500 kbps** |
| USART1 | Mode | **Asynchronous**，115200，PA9/PA10 |

> ⚠️ 波特率必须和天空星一致（都是 **500 kbps**），不然收不到。

---

## 五·五、单点自检（还没接总线时）的预期现象

> 先只给 F103 下载程序、还没接 CAN 总线时，就能判断这块板软硬件对不对。

### 你会看到（串口 115200，每 500 ms 一行）

```
=== CAN sender (F103C8T6) 500kbps ===
Send a frame every 500ms, change period every 5s
[1] TX -> ACK ERROR (TEC=8) 帧已发出但没人应答：需要 2 个节点 + 两端 120Ω + 共地
[2] TX -> ACK ERROR (TEC=16) 帧已发出但没人应答：需要 2 个节点 + 两端 120Ω + 共地
...
[32] BUS-OFF (TEC=255) -> 总线上没有节点应答，正在重启 CAN...
[33] TX -> ACK ERROR (TEC=8) ...      ← 自动重启后重新开始
```

### ⚠️ 关键点：单点时 **ACK ERROR 是正确的、预期的**

原因：CAN 协议规定发送方必须在 ACK 间隙收到至少一个节点拉低总线；
总线只有 F103 一个节点 → 没人应答 → 每帧报 ACK 错误 → TEC（发送错误计数）累加 →
约 32 帧后进入 **Bus-Off**。

| 现象 | 判断 |
|---|---|
| 打印 **ACK ERROR** | ✅ **正常**！说明程序在跑、PA11/PA12 引脚对、波特率配置生效，只是没有第二个节点 |
| 出现 **BUS-OFF 并自动重启** | ✅ 正常（代码里已做自动恢复，接上节点后会自动退出 Bus-Off） |
| 串口**完全没打印** | ❌ 程序没跑：下载失败 / 晶振 HSE 8 MHz 没配对 / MicroLIB 没勾 / 串口 TX-RX 接反 |
| 打印 **CAN start FAILED** | ❌ `HAL_CAN_Start()` 失败，检查 PA11/PA12 是否被别的外设占用 |
| 一直 **TX OK** | ⚠️ 可疑！说明你在用旧版程序（旧版只看入队结果，会误报成功）。请换成新版 |

> **为什么旧版会"假成功"**：`HAL_CAN_AddTxMessage()` 返回 OK 只代表"报文进了发送邮箱"，
> 且本工程 `AutoRetransmission = DISABLE`（失败不重传，邮箱立刻释放），
> 所以即使无人接收，旧版也会一直打印 `TX ok`。
> 新版改为读取 `CAN->ESR` 的 LEC/BOFF/TEC，**真实反映总线状态**。

### 用万用表（接了收发器的话）

| 状态 | CANH 对 GND | CANL 对 GND |
|---|---|---|
| 空闲（隐性） | ≈ 2.5 V | ≈ 2.5 V |
| 发送（显性） | ≈ 3.5 V | ≈ 1.5 V |

> 万用表反应慢，看到的是平均值（2.5~2.7 V 之间轻微跳动）。
> 有示波器的话，能在 PA12(CAN_TX) 上看到 500 kbps 的方波。

### 单点自检通过的标准

只要串口持续打印（不管是 ACK ERROR 还是 BUS-OFF 循环），
就说明 **F103 的时钟、CAN 外设、引脚、波特率全部正确**——
剩下的只是把总线接起来（两个节点 + 120 Ω ×2 + 共地）。

---

## 六、联合验收流程

1. 两块板都下载好程序，CAN 总线接好（两节点 + 120 Ω ×2 + 共地）
2. 先给 F103 上电，串口应看到 `=== CAN sender (F103C8T6) 500kbps ===`
3. 天空星上电，串口应看到 `Task1 start: ...`，呼吸灯开始 2 秒周期呼吸
4. 观察：
   - F103 打印 `TX ok`（不是 `TX FAIL`）
   - 天空星呼吸灯**每 5 秒变一次频率**
5. 串口助手给天空星发 `hello` → 回 `Receive Data ：（hello）`
6. **关键验收点**：把 F103 断电（CAN 帧停止），天空星呼吸灯**仍按最后一次设定的频率继续呼吸** ✅

---

## 七、故障排查

| 现象 | 原因 / 处理 |
|---|---|
| 一直打印 `TX FAIL` | ① 总线上只有 1 个节点；② 120 Ω 没接；③ GND 没共地；④ 两端波特率不一致；⑤ 收发器没供电 |
| F103 串口无打印 | MicroLIB 没勾；PA9/PA10 接反；COM 口选错；**下载后没按复位**（Keil 默认下载完芯片是停止状态） |
| 串口只打印**半截就停**（如 `=== CAN` 后戛然而止） | ⭐ **`stm32f1xx_it.c` 里缺 `SysTick_Handler`**（本工程生成时 CubeMX 漏了，已手动补在 USER CODE 1 区）。没有它，上电 1 ms 第一次 SysTick 中断就跳进 startup 文件的 WEAK 死循环。用 Keil 调试暂停会看到停在 `startup_stm32f103xb.s` 的 `SysTick_Handler` |
| 程序下载不进去 | Flash 算法没选（Debug → Settings → Flash Download → Add → STM32F1xx Flash） |
| F103 打印 TX ok 但天空星灯不变 | 天空星侧检查：PB8/PB9 接反、CAN 过滤器（代码里是全接收）、帧格式对不对 |
| 天空星收到但频率不变 | 确认发的 CMD 是 `0x01`，校验和正确；看天空星串口有没有异常 |

> ⭐ **通用规律**：调试暂停时如果 PC 停在 startup 汇编文件的 WEAK handler 里
> （`B .` 死循环），说明**对应的中断服务函数没有被正确链接**——要么 `.c` 没进工程，
> 要么像这次一样函数压根没生成。检查中断向量对应的 handler 名字与工程里的定义。

---

## 八、想手动指定周期？

改 `can_sender/Src/main.c` 里这一行：

```c
const uint16_t periodTable[3] = {2000u, 1000u, 400u};  /* 改成你想要的周期(ms) */
```

例如只想固定 500 ms：`{500u, 500u, 500u}`。

协议帧格式见 `任务一，二\task1-接线与验收说明.md` 第五节。
