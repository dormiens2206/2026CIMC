# 项目功能说明
工业嵌入式

> MCU：GD32F4xx（GD32F450/470，ARM Cortex-M4）
> 开发环境：Keil MDK（工程名 `CIMC_GD32_Template`）

---

## 1. 项目概述

本项目是一个基于 GD32F4xx 单片机的**工业嵌入式**完整解决方案，采用 **BootLoader + App 双工程架构**：

- **BootLoader 工程**（`BootLoader/`）：负责上电引导、APP 有效性校验与跳转，以及通过 RS485 串口完成固件在线升级（OTA）、版本备份与回退。
- **App 工程**（`App/`）：承担核心业务，包括三通道模拟量采集（两路电压 + 一路电流）、PT100 温度测量、RS485 通信（自定义 ASCII 协议 + Modbus RTU 从站）、OLED 显示、TF 卡数据存储、阈值告警、自动上报、低功耗睡眠等。

两个工程共享同一套 Flash 分区规划与硬件驱动，通过备份寄存器（RTC_BKP1/BKP2）在 BootLoader 与 App 之间传递升级请求标志与波特率信息。

---

## 2. 系统架构与 Flash 分区

### 2.1 Flash 分区规划（`memory.h`）

| 区域 | 起始地址 | 结束地址 | 大小 | 用途 |
|---|---|---|---|---|
| Bootloader 区 | 0x08000000 | 0x0800FFFF | 64 KB | 引导程序与升级逻辑 |
| 参数区 | 0x08010000 | 0x08010FFF | 4 KB | 预留参数区 |
| APP 区 | 0x08011000 | 0x08030FFF | 128 KB | 应用程序固件 |
| APP 备份区 | 0x08031000 | 0x08050FFF | 128 KB | 升级前旧固件备份 |
| 固件暂存区 | 0x08051000 | 0x08070FFF | 128 KB | 升级时接收的固件暂存 |
| SRAM | 0x20000000 | 0x20040000 | 256 KB | 运行内存 |

APP 固件内版本号存放于 **APP 区起始地址 + 0x200**（即 `0x08011200`），当前版本固化为 `20.01.00.00`（字节序 `0x14 0x01 0x00 0x00`）。

### 2.2 启动跳转流程

1. 芯片复位后从 Bootloader 区（`0x08000000`）开始执行。
2. Bootloader 读取备份寄存器 `RTC_BKP1` 判断是否存在升级请求标志（`0xA55A5AA5`）。
3. 无升级请求 → 延时 5 秒 → 校验 APP 合法 → 跳转 APP。
4. 有升级请求 → 进入 10 秒升级等待流程，处理上位机升级命令。
5. APP 运行后通过 `SCB->VTOR = 0x08011000` 重定向中断向量表。

---

## 3. 硬件资源与外设

| 外设 | 引脚/接口 | 说明 |
|---|---|---|
| OLED | I2C 软件模拟，PB8(SCL)/PB9(SDA) | 128×64 显示 |
| LED | PB13(LED1)/PB12(LED2)/PB14(LED3)/PB15(LED4) | 状态指示灯 |
| 按键 | PA7(KEY1)/PA6(KEY2)/PA5(KEY3)/PA4(KEY4) | 模式切换、参数导出 |
| RS485 | USART1，PD5(TX)/PD6(RX)，PE8(DE 方向控制) | 通信总线 |
| 内部 ADC | ADC0，PC0(CH0/通道10)/PC1(CH1/通道11) | 电压/电流采集，基准 3.2979V |
| 外部 ADC | GD30AD3344，SPI 接口，CS=PE10 | 16 位高精度 ADC，用于 PT100 测温 |
| DAC | DAC0(PA4)/DAC1(PA5) | 模拟量输出 |
| RTC | 片上 RTC + 备份寄存器 | 实时时钟、时间戳、唤醒定时 |
| PMU | PA0(EXTI0 唤醒) | 睡眠/深度睡眠低功耗 |
| SPI Flash | SPI0，CS=PA15 | 板载外部 Flash，参数与告警记录持久化 |
| TF 卡 | SDIO 接口 | 采样数据 CSV 与配置导出 |

## 4. BootLoader 工程

### 4.1 功能概述

BootLoader 负责上电引导、APP 有效性校验、固件升级与版本管理。其核心文件为：

- `User/main.c`：启动流程与升级状态机。
- `Function/bootloader.c/h`：Flash 分区操作与跳转逻辑。
- `Protocol/`：复用与 App 相同的 ASCII 帧协议，仅处理升级相关命令。

### 4.2 启动流程（`main.c`）

1. `Boot_HW_Init()`：初始化滴答定时器、LED、备份域（使能 PMU/RTC 时钟）。
2. `OLED_Init()` + 显示队伍号 `2026413929` 与 `Bootloader`。
3. 判断升级标志：
   - **`Boot_Get_Update_Flag() == 0xA55A5AA5`** → 进入 `Boot_UpdateStart()`（升级等待流程）。
   - **否则** → 进入 `Boot_NormalStart()`（正常启动）。

#### 正常启动 `Boot_NormalStart()`
- 延时 5 秒（期间 LED1 闪烁），期间**不做串口输出**（赛题要求）。
- 校验 APP 合法后跳转 APP；若 APP 无效则死循环闪灯。

#### 升级流程 `Boot_UpdateStart()`
- 按备份寄存器 `RTC_BKP2` 保存的波特率初始化 RS485。
- 串口提示等待命令，并进入 10 秒等待窗口。
- 循环处理 `Protocol_Task()`：
  - 收到并校验完固件（0x0502）后，重新给予 0x0503 一个 10 秒执行窗口。
  - 超时或固件就绪超时后退出升级流程，跳转 APP 或闪灯。
  - 执行升级完成（0x0503）后软复位，从干净状态重新启动再跳 APP。

### 4.3 升级协议命令（在 Bootloader 中处理）

| 命令字 | 功能 | 处理逻辑 |
|---|---|---|
| 0x0502 | 固件数据包传输 | 接收原始固件，校验魔术字 `0x5AA5C33C`，写入暂存区 |
| 0x0503 | 执行升级 | 校验暂存固件 → 擦除 APP 区 → 搬运到 APP 区 |
| 0x0504 | 版本回退 | 校验备份区 → 将备份固件回写 APP 区 → 复位 |
| 0x0505 | 查询备份版本 | 读取备份区版本号（`APP_BACKUP + 0x200`） |
| 0x0599 | 退出 Bootloader | 清除升级标志 → 复位进入正常启动 |

### 4.4 Flash 操作接口（`bootloader.c`）

- `Boot_Check_App_Valid()`：校验 APP 合法性（栈指针落在 SRAM 范围内、Reset 向量为 Thumb 且落在 APP 区内）。
- `Boot_Jump_To_Application()`：设置 MSP/PC，跳转到 APP。
- `Boot_Erase_App_Area()` / `Boot_Write_App()`：擦除/写入 APP 区。
- `Boot_Erase_Temp_Area()` / `Boot_Write_Temp()`：操作固件暂存区。
- `Boot_Check_Temp_Firmware()` / `Boot_Copy_Temp_To_App()`：校验并搬运暂存固件。
- `Boot_Backup_App()` / `Boot_Rollback()`：升级前备份与回退。
- `Boot_Get_Backup_Version()`：读取备份版本。
- `Boot_Request_Update()`：供 App 调用，写升级标志后软复位进入 Bootloader。

## 5. App 工程

### 5.1 运行模式

App 支持两种运行模式，通过按键切换：

- **`MODE_ASCII`（ASCII 模式）**：使用自定义 ASCII 十六进制帧协议（`Protocol_Process`）。
- **`MODE_WORK`（Modbus 模式）**：作为 Modbus RTU 从站（FreeModbus，`eMBPoll`）。

切换方式（`Function.c / UsrFunction()`）：
- **KEY1**：切换到 ASCII 模式（禁用 Modbus）。
- **KEY2**：切换到 Modbus 模式（重新初始化并启用 Modbus）。
- **KEY3**：将当前参数导出为 TF 卡上的 `Config.ini`。

### 5.2 系统初始化（`System_Init()`）

1. `System_Hardware_Init()`：滴答定时器、LED、按键、OLED、ADC、DAC、PMU、SDIO 中断。
2. `RTC_Init()`：初始化实时时钟。
3. `spi_flash_init()` + `spi_flash_read_id()`：初始化并检测外部 SPI Flash。
4. `Param_Load()`：从外部 Flash 加载系统参数。
5. `App_ParamInit()`：恢复/校验 APP 参数。
6. `Sample_Init()`：采样初始化。
7. `App_ModbusInit()`：Modbus 从站初始化。
8. `App_Storage_Start()`：启动 TF 卡存储。
9. 恢复 DAC 输出值、按保存波特率重新初始化 RS485、初始化 LED、OLED 显示队伍号与 `IDLE`。
10. 上电/复位后主动发送心跳帧（`Protocol_SendHeartBeat`）。

### 5.3 主循环任务调度（`System_Task()`）

**ASCII 模式：**
- 读取 RS485 数据 → `Protocol_Process()` 协议解析。
- 任务调度：自动上报、睡眠、复位、Bootloader 升级请求、告警检测、LED1 状态灯。

**Modbus 模式：**
- `eMBPoll()` 轮询 Modbus 帧。
- `App_ModbusMap_Apply()` / `App_ModbusReconfigureTask()`：应用/重配 Modbus 配置。
- 按采样周期执行 `Sample_Task()`。
- `App_StorageTask()`：TF 卡存储。
- `App_ModbusMap_Update()`：将采样结果写入 Modbus 寄存器。
- CH2 断线时点亮 LED3。
- 任务调度：自动上报、睡眠、复位、Bootloader 升级请求、告警检测、LED1 状态灯。

### 5.4 任务模块

| 模块 | 文件 | 功能 |
|---|---|---|
| 自动上报 | `app_task.c` | 按 1s/3s/5s 周期上报时间戳 + CH0/CH1/CH2 |
| 睡眠 | `app_task.c` | 收到 0x03AA 后延时 10s 唤醒，醒来发 `instrument wakeup` |
| 复位 | `app_task.c` | 收到 0x0101 后延时复位 |
| 升级请求 | `app_task.c` | 收到 0x0501 后写升级标志并软复位进 Bootloader |
| 告警检测 | `alarm_task.c` | CH0/CH1 超阈值检测、记录与主动上报 |
| 采样 | `sample_task.c` | 三通道 ADC 采集 + PT100 测温 |
| 参数持久化 | `flash_param.c` | 外部 SPI Flash 参数/告警记录读写 |

## 6. 通信协议

### 6.1 自定义 ASCII 帧协议（`uart_protocol.h`）

数据以 **ASCII 十六进制字符串** 传输，内部先经 `Protocol_HexStr2Bin` 转为二进制，再按帧格式解析。

**帧格式：**

| 字段 | 长度 | 说明 |
|---|---|---|
| 帧头 | 2 字节 | 固定 `0xA5B6` |
| 设备 ID | 2 字节 | 广播 ID 为 `0xFFFF` |
| 帧类型 | 1 字节 | CMD=0x01 / REPLY=0x02 / BEAT=0x05 / ERROR=0xFF |
| 命令字 | 2 字节 | 见下方命令表 |
| 数据长度 | 1 字节 | payload 长度 |
| 协议版本 | 1 字节 | `0x02` |
| 数据 | 0~255 字节 | 传输内容 |
| CRC | 2 字节 | CRC16 校验 |
| 帧尾 | 2 字节 | 固定 `0xB6A5` |

最小帧长 13 字节，最大 payload 255 字节。

**命令字定义（部分）：**

| 类别 | 命令字 | 功能 |
|---|---|---|
| 系统管理 | 0x0101 | 设备重启 |
| | 0x0104 | 查询固件版本 |
| | 0x0105 / 0x0106 | 设置 / 查询时间 |
| | 0x01A1 / 0x0111 | 设置 / 查询设备 ID |
| | 0x01A2 / 0x0112 | 设置 / 查询波特率 |
| 数据 | 0x0201 / 0x0202 / 0x0203 | 查询 CH0 / CH1 / CH2 |
| | 0x0241 / 0x0242 / 0x0243 | 设置 CH0 / CH1 / CH2 变比 |
| | 0x0261 | 设置上报间隔 |
| 控制 | 0x0301 | 设置 DAC 输出 |
| | 0x0302 / 0x0303 | 启动 / 停止定时上报 |
| | 0x03AA | 进入睡眠 |
| 阈值 | 0x0400 | 批量读取阈值 |
| | 0x0401~0x0403 | 读取 CH0/CH1/CH2 单通道阈值 |
| | 0x0411~0x0413 | 写入 CH0/CH1/CH2 阈值 |
| 升级 | 0x0501 | 升级请求（App 处理） |
| | 0x0502 | 固件数据包传输（Bootloader 处理） |
| | 0x0503 | 执行升级（Bootloader 处理） |
| | 0x0504 | 版本回退（Bootloader 处理） |
| | 0x0505 | 查询备份版本（Bootloader 处理） |
| | 0x0599 | 退出 Bootloader（Bootloader 处理） |
| 告警日志 | 0x0601 | 询问是否上报告警（设置告警模式） |
| | 0x0602 | 查询告警记录 |
| | 0x0603 | 清除告警记录 |
| | 0x0604 / 0x0605 | 查询 / 清除操作日志 |
| | 0x0606 | 断线检测查询 |
| 特殊 | 0x8888 | 心跳包 |
| | 0xFFFF | 上电通知 / 寻找设备 |
| | 0xEEEE | 错误应答 |

波特率编码：`0x11`=4800、`0x12`=9600、`0x13`=19200、`0x14`=115200。

### 6.2 Modbus RTU 从站（`modbus_register_map.h`）

App 在 `MODE_WORK` 下作为 Modbus RTU 从站，寄存器映射如下：

**输入寄存器（功能码 04，只读，共 12 个使用）：**

| 地址 | 内容 |
|---|---|
| 0x0000~0x0005 | 实时时钟（年/月/日/时/分/秒） |
| 0x0006 | CH0 通道数据 |
| 0x0008 | CH1 通道数据 |
| 0x000A | CH2 通道数据 |

**保持寄存器（功能码 03/06/16，读写，共 60 个使用）：**

| 地址 | 内容 |
|---|---|
| 0x0000~0x0005 | 实时时钟（年/月/日/时/分/秒） |
| 0x0010 | Modbus 从站地址 |
| 0x0011 | RS485 波特率编码 |
| 0x0012 | 数据位 |
| 0x0013 | 停止位 |
| 0x0014 | 校验位 |
| 0x0030 / 0x0032 / 0x0034 | CH0 / CH1 / CH2 变比（float，占 2 寄存器） |
| 0x0036 / 0x0038 / 0x003A | CH0 / CH1 / CH2 阈值（float，占 2 寄存器） |

**线圈（功能码 01/05/15，读写）：**

| 地址 | 内容 |
|---|---|
| 0x0000 | 运行指示灯 |
| 0x0001 | 电流通道断线标志 |

**离散输入（功能码 02，只读）：**

| 地址 | 内容 |
|---|---|
| 0x0000 | 设备故障标志 |
| 0x0001 | CH0 过阈值报警 |
| 0x0002 | CH1 过阈值报警 |
| 0x0003 | CH2 过阈值报警 |

## 7. 数据采集与标定

### 7.1 三通道采集

| 通道 | 物理量 | 采集方式 | 说明 |
|---|---|---|---|
| CH0 | 电压 1 | 内部 ADC0，PC0（通道 10） | 采样值 × CH0 变比 |
| CH1 | 电压 2 | 内部 ADC0，PC1（通道 11） | 采样值 × CH1 变比 |
| CH2 | 电流 | ADC / DAC 相关通道 | 采样值 × CH2 变比 |

- 采样采用多次平均（`SAMPLE_AVG_TIMES = 8`）滤波。
- 电压/电流标定采用线性模型 `y = k·x + b`（`current_k/b`、`voltage1_k/b`、`voltage2_k/b`），K 必须大于 0。
- 电流断线检测：电流低于断线阈值（默认 3.60 mA）判定断线，高于恢复阈值（默认 3.80 mA）恢复，断线状态写入线圈寄存器 `0x0001` 并点亮 LED3。

### 7.2 PT100 温度测量

- 使用外部 16 位高精度 ADC **GD30AD3344**（SPI 接口，单端/差分可配，PGA 增益可调）。
- `PT100_Calculate_Temperature()`：由电阻值查表/换算温度。
- 提供滤波（`PT100_Filter`）与补偿（`PT100_Compensate`）。

## 8. 存储系统

### 8.1 TF 卡存储（FatFS，`app_storage.c`）

- 通过 SDIO 接口访问 TF 卡，挂载 FatFS 文件系统。
- **`DATA.CSV`**：采样数据日志，表头 `current_mA,voltage1_V,voltage2_V,current_break`，按存储周期追加写入。
- **`Config.ini`**：用户定值导出（KEY3 触发），包含 `[info] dID`、`[ratio] CH0/CH1/CH2`、`[limit] CH0/CH1/CH2`。

### 8.2 外部 SPI Flash（`flash_param.c`）

| 地址 | 区域 | 说明 |
|---|---|---|
| 0x000000~0x000FFF | 参数区 | 魔数 `0x50415241`('PARA')，保存设备 ID、波特率、变比、阈值、DAC 值、告警模式、标定系数等 |
| 0x001000~0x001FFF | 告警记录区 | 魔数 `0x414C4D31`('ALM1')，环形保存最多 10 条告警记录 |

参数区带 CRC 校验，`Param_Load()` 加载失败时自动恢复默认值。

## 9. 告警管理（`alarm_task.c`）

- 每 1 秒检测一次 CH0/CH1 是否超过各自阈值。
- 超阈值时记录告警（通道号、阈值、实测值、时间戳）。
- **主动模式**（`ALARM_MODE_ACTIVE`）：立即通过串口主动上报告警记录。
- **被动模式**（`ALARM_MODE_PASSIVE`）：仅记录，等待上位机查询（0x0602）。
- 支持告警清除（0x0603），清除后重新锁存当前状态。

## 10. 目录结构

```
2026413929/
├── App/                        # 应用程序工程
│   ├── Application/            # 应用层（Modbus 映射、参数、采样、存储、版本）
│   ├── Config/                 # 配置（寄存器映射、工程配置）
│   ├── Driver/                 # 外设驱动（ADC/DAC/OLED/RTC/PMU/SPI_FLASH/SD 等）
│   ├── Function/               # 业务逻辑（任务、告警、参数、PT100、采样）
│   ├── HeaderFiles/            # 统一头文件
│   ├── Library/                # 官方标准外设库 / USB / FatFS
│   ├── Middlewares/            # FatFS、FreeModbus
│   ├── Protocol/               # 自定义 ASCII 协议
│   ├── Startup/                # 启动文件
│   └── User/                   # main、中断处理、滴答定时器
│
└── BootLoader/                 # 引导程序工程
    ├── CMSIS/                  # Cortex-M4 内核与 GD32F4xx 头文件
    ├── Driver/                 # 复用外设驱动
    ├── Function/               # bootloader 逻辑、memory 分区定义
    ├── Library/                # 官方标准外设库 / USB / FatFS
    ├── Protocol/               # 升级协议
    ├── Startup/                # 启动文件
    └── User/                   # main、中断处理、滴答定时器
```

---

## 总结

本项目实现了一个完整的 **GD32F4xx 智能仪器仪表**：Bootloader 提供可靠的 OTA 固件升级、版本备份与回退能力；App 具备三通道模拟量采集、PT100 测温、双协议（自定义 ASCII + Modbus RTU）通信、OLED 人机交互、TF 卡数据记录、SPI Flash 参数持久化、阈值告警、自动上报与低功耗睡眠等完整功能，适用于仪器仪表数据采集、监控与远程管理的应用场景。




