# 任务二 完整工程说明：MPU6050 软件 IIC + DMP + 5ms 绝对周期

> 工程：**`桌面\test\task2\MDK-ARM\task2.uvprojx`**（基于任务一工程复制，继承全部功能）
> 工作分支：`task2-dev`
> 同样**不需要 CubeMX 重新生成**——新外设只是 GPIO，初始化已手写。

---

## 一、与任务一的关系

| 内容 | 状态 |
|---|---|
| CAN 收帧 → 队列(6) → 任务通知 → 改呼吸频率 | ✅ 继承，照常工作 |
| 呼吸灯 TIM7+TIM3 硬件驱动 | ✅ 继承（"任务二运行时呼吸灯照常"继续成立） |
| 串口空闲中断+DMA → 队列(3) → `Receive Data ：（%s）` 回传 | ✅ 继承 |
| **MPU6050 原始六轴，5ms 周期 JustFloat** | ★ 新增 |
| **DMP 欧拉角，5ms 周期 JustFloat** | ★ 新增（需按第 6 节放入 DMP 库） |
| **vTaskDelayUntil 绝对周期** | ★ 新增（任务书核心考点） |

## 二、任务二新增文件

| 文件 | 作用 |
|---|---|
| `Src/soft_i2c.c/h` | 软件 IIC（PB0=SCL / PB1=SDA，开漏+上拉，DWT 微秒延时） |
| `Src/mpu6050.c/h` | MPU6050 驱动：WHO_AM_I 自检、初始化、原始六轴连续读 |
| `Src/vofa_send.c/h` | VOFA **JustFloat** 二进制帧发送 |
| `Src/mpu_dmp.c/h` | DMP 接口层（`DMP_ENABLED` 开关控制，默认关） |
| `Src/app_tasks.c` | 4 个任务：新增 `Task_Mpu`（vTaskDelayUntil 5ms） |

**软件 IIC 选 PB0/PB1 的原因**：任务一的 CAN 占了 PB8/PB9，串口占了 PB6/PB7，
软件 IIC 用任意空闲 GPIO——这正是软件 IIC 的优势之一（详见 soft_i2c.h 头注释，
任务书"为什么用软件 IIC"的答案就写在里面）。

---

## 三、接线

### 3.1 MPU6050（GY-521 模块）—— 新增

| GY-521 | 天空星 | 说明 |
|---|---|---|
| VCC | 3.3V | 模块自带稳压，接 5V 也可以 |
| GND | GND | **共地** |
| **SCL** | **PB0** | 软件 IIC 时钟 |
| **SDA** | **PB1** | 软件 IIC 数据 |
| **AD0** | **GND** | 器件地址 0x68（接 VCC 则变 0x69） |
| XDA / XCL | 悬空 | 外接磁力计才用 |
| INT | 悬空 | 本工程轮询读取，不用中断 |

> 模块板载 4.7kΩ 上拉电阻已焊好，SCL/SDA 不需要额外上拉。
> **上电自检**：初始化若失败，串口每秒打印接线提示（VCC/GND、SCL=PB0、SDA=PB1、AD0=GND）。

### 3.2 继承任务一的接线（不变）

| 功能 | 天空星 | 外设 |
|---|---|---|
| 呼吸灯 PWM | PB4 | LED + 1kΩ → GND |
| CAN（TJA1050，5V） | PB8(RX) / PB9(TX) | 与 F103 对端 CANH/CANL 互连 |
| 串口 | PB6(TX) / PB7(RX) | USB-TTL（RX/TX 交叉，共地） |
| 下载 | PA13/PA14 | 板载 DAP-Link |

---

## 四、VOFA+ 配置（重要，与任务一不同）

任务二的数据是**周期性曲线**，用二进制协议看波形：

1. VOFA+ 新建连接 → 串口 115200 → **协议选 `JustFloat`**（不是 FireWater）
2. 波形窗口会出现通道：
   - **DMP 关闭时（默认）**：ch0~ch5 = 加速度 xyz（g）、陀螺仪 xyz（°/s）
   - **DMP 启用后**：另有 ch0~ch2 = pitch / roll / yaw（度）
3. **时间戳**：打开 VOFA 数据记录/时间戳显示，相邻数据点间隔应**恒定 5ms**——
   这就是 `vTaskDelayUntil` 绝对周期的验收证据

> 提示：JustFloat 模式下串口的文本回传（任务一功能）会干扰波形解析。
> 演示曲线时不要往串口发文本；要演示回传功能时把 VOFA 切回 FireWater/文本模式。

---

## 五、验收步骤

1. **上电**：串口打印
   `Task2 start: task1 features + MPU6050 (soft IIC PB0/PB1) + 5ms vTaskDelayUntil`
   `MPU6050 ready (software IIC, 0x68)`
   `DMP disabled - raw 6-axis only (6 channels)`
2. **原始六轴**：VOFA JustFloat 模式，6 条曲线实时滚动
   - 静止平放：accel z ≈ 1g，x/y ≈ 0
   - 沿桌面平移：加速度计有尖峰，陀螺仪有输出
   - 转动板子：gyro 变化明显
3. **5ms 绝对周期**：VOFA 时间戳相邻间隔恒定 **5ms**（核心验收点）
4. **任务一功能回归**：发串口文本有回传；CAN 发帧呼吸灯频率仍会变
5. **DMP（放好库后）**：7~9 通道出现 pitch/roll/yaw，转一圈 yaw 变化 ≈ 360°

---

## 六、启用 DMP 的三个步骤

默认 `DMP_ENABLED=0`（工程立即可编译，先验收原始数据）。DMP 需要 InvenSense 官方库：

### 步骤 1：获取 eMPL 库文件（6 个）

- 来源 A：**正点原子** HAL 库例程《ATK-MPU6050 六轴传感器（DMP）实验》，取其中的：
  `inv_mpu.c`、`inv_mpu.h`、`inv_mpu_dmp_motion_driver.c`、`inv_mpu_dmp_motion_driver.h`、`dmpKey.h`、`dmpmap.h`
- 来源 B：GitHub 搜 `eMPL` / `MPU6050 DMP`，或问队友要（RoboMaster 车架码盘代码里也常有）

### 步骤 2：放进工程

- 6 个文件放到 `task2/Src/`（.c）和 `task2/Inc/`（.h）
- Keil 里 **Application/User 分组 → Add Existing Files** 把 3 个 .c 加进工程
- Include Paths 已含 `../Inc`，不用改

### 步骤 3：适配（正点原子例程需改 3 处小地方）

1. `inv_mpu.c` 里 `#include "sys.h"` / `#include "delay.h"` —— 删除或替换
   （它用的 `delay_ms()` 改成 `vTaskDelay(pdMS_TO_TICKS(x))`）
2. `inv_mpu.c` 里的 `i2c_write/i2c_read` 已由本工程的 `MPU_Write_Len/MPU_Read_Len`（mpu6050.c）承接
3. `board_config.h` 里把 `DMP_ENABLED` 改成 `1` → Rebuild

> `mpu_dmp.c` 里已写好：DMP 固件加载、FIFO 配置（200Hz）、四元数→欧拉角换算、
> 以及 eMPL 需要的 `get_ms()`。接入后 Task_Mpu 自动多出 3 个角度通道。

---

## 七、排查表

| 现象 | 排查 |
|---|---|
| 串口一直打印 `MPU6050 init failed` | ① VCC/GND；② SCL=PB0、SDA=PB1 没接反；③ AD0 接地；④ 模块坏（换备件） |
| 数据全是 0 或 -1 | 软件 IIC 读失败（多数是接线松动）；或量程系数没用对 |
| 曲线毛刺大 | DLPF 滤波已开（44Hz），仍有毛刺检查是否杜邦线太长/接触不良 |
| 时间戳间隔不是 5ms | 确认用的是 `vTaskDelayUntil`；串口带宽是否被 JustFloat 之外的数据挤占 |
| VOFA 没有曲线 | 协议必须选 **JustFloat**（FireWater 解析不了二进制帧） |
| DMP 初始化失败 | 库文件没加进工程 / `DMP_ENABLED` 没置 1 / FIFO 溢出（先 mpu_reset） |
| 编译报 `inv_mpu.h: No such file` | DMP_ENABLED=1 但库文件还没放——先改回 0，或按第 6 节放好库 |

---

## 八、任务书考点自查（面试口径转述）

1. **为什么软件 IIC 不用硬件 I2C**：引脚灵活（本项目 PB6~PB9 全被占用）；
   总线锁死可软件恢复（9 个时钟+STOP）；时序完全可控。代价是占 CPU（本例 5ms 里约 2ms，可接受）。
2. **vTaskDelay vs vTaskDelayUntil**：前者"干完活再等 5ms"（周期漂移），
   后者"距上次唤醒满 5ms"（绝对周期，VOFA 时间戳可证）。
3. **DMP 是什么**：MPU6050 片内运动处理引擎，片内跑姿态融合输出四元数，
   主机免跑卡尔曼/互补滤波，只需读 FIFO。
4. **JustFloat vs 文本打印**：二进制定长帧，115200 下 9 通道 3.5ms 可靠传完；
   文本一行就要 3~5ms 且长度不定，5ms 周期下不可靠。
