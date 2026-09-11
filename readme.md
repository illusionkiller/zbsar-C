# ZBSAR-B OC 工程说明

## 1. 仓库定位

本仓库是一个基于 Xilinx Vitis 工程体系组织的嵌入式固件工作区，整体软件栈运行在：

- `MicroBlaze`
- `FreeRTOS`
- `lwIP`

仓库中同时包含：

- 硬件平台工程
- Bootloader 工程
- 主应用工程
- Vitis 生成的 system/platform 配套工程

主应用承担的核心职责包括：

- 板级初始化与运行时环境启动
- 网络服务启动
- SCPI 控制接口提供
- DTU UDP 数据接口提供
- 同步 RS422 发送/接收通道管理
- 基于文件与 JSON 配置的测试流程编排

## 2. 平台环境

### 2.1 软件环境

- 工程体系：Xilinx Vitis / Eclipse CDT 风格工程
- 处理器架构：`MicroBlaze`
- 实时操作系统：`FreeRTOS`
- 网络协议栈：`lwIP`
- BSP 与底层库：Xilinx BSP
- 文件系统支持：文件管理封装 + `littlefs` 适配
- 协议支持：
  - `SCPI` over TCP
  - `protobuf` via `nanopb`
  - 本地 Shell/Console

### 2.2 硬件与平台工程

- `ZBSAR_TOP.xsa`
  - 硬件导出文件
- `ZBSAR_TOP/`
  - Vitis 平台工程
  - 包含 BSP、平台元数据、硬件导出物
- `ZBSAR_TOP/microblaze_0/`
  - 与 MicroBlaze 相关的 BSP 和库

从当前平台目录可见，工程依赖的基础软件组件主要包括：

- `freertos10_xilinx_*`
- `lwip213_*`
- `standalone_*`

### 2.3 网络运行环境

主应用启动后会初始化 lwIP，并在其上继续启动多个网络服务。网络参数支持通过环境变量配置，例如：

- `IP`
- `mask`
- `gateway`
- `mac`

如果 BSP/lwIP 配置中启用了 DHCP，设备也可以动态获取 IP 地址。

## 3. 顶层目录结构

```text
oc/
|-- ZBSAR_TOP/             硬件平台工程及 BSP 输出
|-- ZBSAR_TOP.xsa          硬件导出文件
|-- bootloader/            Bootloader 应用工程
|-- bootloader_system/     Bootloader 对应的 Vitis system 工程
|-- test/                  主应用工程
|-- test_system/           主应用对应的 Vitis system 工程
|-- .metadata/             Vitis/Eclipse 工作区元数据
|-- .vscode/               编辑器配置
|-- rules.json             本地规则/配置文件
`-- readme.md              本说明文档
```

## 4. 顶层各层级作用

### `ZBSAR_TOP/`

平台层工程，主要承载：

- 硬件平台定义
- MicroBlaze BSP
- FreeRTOS/lwIP/standalone 集成
- 应用工程依赖的平台元数据

这一层大部分内容由工具生成或维护。

### `bootloader/`

Bootloader 应用工程，负责设备上电后的早期启动与引导逻辑。

### `bootloader_system/`

Bootloader 的 system 工程，主要用于 Vitis 中的构建、下载、调试和工程组织。

### `test/`

主应用工程，是当前业务代码和固件逻辑最集中的目录，日常开发主要围绕这里展开。

### `test_system/`

主应用对应的 system 工程，用于配合 `test/` 完成构建、下载与调试。

### `.metadata/`

Vitis/Eclipse 工作区元数据目录，保存索引、调试配置、工程状态等信息，不属于固件运行时逻辑。

## 5. 主应用目录结构

`test/` 是整个仓库最核心的应用工程。

```text
test/
|-- src/
|   |-- driver/            板级驱动与外设驱动
|   |-- net_manager/       网络初始化与网络服务
|   |-- env/               环境变量与运行时参数
|   |-- log/               日志子系统
|   |-- SHELL/             本地控制台与命令行
|   |-- cJSON/             JSON 解析库
|   |-- nanopb/            Protobuf 支持
|   |-- libscpi/           SCPI 协议库
|   |-- lfs/               littlefs 适配层
|   |-- tiny_crypt/        加密/摘要辅助库
|   |-- main.c             应用入口与启动流程
|   |-- scpi_server.c      SCPI TCP 服务
|   |-- scpi-def.c         SCPI 命令与业务动作映射
|   |-- dtu.c              DTU UDP 服务
|   |-- sync_rs422tx_*     同步 RS422 TX 通道逻辑
|   |-- sync_rs422rx_*     同步 RS422 RX 通道逻辑
|   |-- test_case.*        测试用例状态机与流程编排
|   |-- test_case_cmd.c    测试命令注册
|   |-- file_manager.*     文件访问抽象
|   `-- system_cmd.c       系统级命令
|-- Debug/                 编译输出目录
|-- _ide/                  Vitis 工程辅助内容
|-- .project/.cproject     Eclipse/Vitis 工程描述文件
`-- test.prj               工程配置文件
```

## 6. `test/src` 各层级职责

### 6.1 入口层

- `main.c`

主要职责：

- 创建主运行线程
- 初始化板级服务和中间件
- 启动网络、SCPI、DTU、RS422 等模块
- 注册 Shell 和系统命令
- 维持系统进入稳定运行状态

### 6.2 驱动层

- `driver/`

主要职责：

- 封装硬件访问
- 提供 SPI、SPI Slave、PWM、GPIO、Flash、交换芯片等能力
- 对上层隐藏寄存器级实现细节

这一层可以理解为硬件抽象层。

### 6.3 系统服务层

- `net_manager/`
- `env/`
- `log/`
- `SHELL/`
- `file_manager.*`
- `lfs/`

主要职责：

- `net_manager/`
  - 网络接口初始化
  - IP 配置
  - SNTP、TFTP 等网络服务启动
- `env/`
  - 运行时参数与环境变量管理
- `log/`
  - 统一日志接口
- `SHELL/`
  - 本地命令行入口
- `file_manager.*`
  - 文件读写与文件工具接口
- `lfs/`
  - Flash 文件系统适配

这一层是运行时基础设施层。

### 6.4 协议与接口层

- `scpi_server.c`
- `scpi-def.c`
- `dtu.c`
- `nanopb/`
- `libscpi/`
- `cJSON/`

主要职责：

- `scpi_server.c`
  - 提供 SCPI TCP 服务
- `scpi-def.c`
  - 将 SCPI 命令映射到具体业务动作
- `dtu.c`
  - 提供基于 UDP 的 protobuf 通信接口
- `nanopb/`
  - protobuf 编解码
- `libscpi/`
  - SCPI 协议解析与标准能力
- `cJSON/`
  - JSON 配置解析

这一层是外部通信与协议适配层。

### 6.5 业务逻辑层

- `sync_rs422tx_channel.c`
- `sync_rs422rx_channel.c`
- `test_case.c`
- `test_case_cmd.c`
- `system_cmd.c`

主要职责：

- RS422 TX 通道控制与事件处理
- RS422 RX 通道控制、数据回环与上报
- 测试用例加载与执行编排
- 测试状态机管理
- 调试与运维命令提供

这一层最接近实际业务流程。

## 7. 启动时序

固件的启动顺序大致如下：

```text
main()
  -> 使能缓存
  -> 创建 main_thread
  -> 启动 FreeRTOS 调度器

main_thread()
  -> 设置系统时间
  -> 初始化堆统计
  -> 初始化引脚、文件系统、交换芯片、Flash、PWM
  -> 打开风扇
  -> 初始化日志和环境参数
  -> 初始化 RS422 TX/RX 模块
  -> 初始化网络
  -> 启动 SNTP
  -> 启动 TFTP
  -> 启动 SCPI Server
  -> 启动 DTU UDP Server
  -> 注册 Shell / System / Test 命令
  -> 进入常驻运行循环
```

整体上遵循典型的嵌入式分层启动顺序：

- 先平台
- 再中间件和网络
- 再外部接口
- 最后业务服务

## 8. 业务逻辑框架

### 8.1 总体框架

```text
外部客户端
  |- SCPI 客户端（TCP）
  |- DTU 客户端（UDP）
  |- 本地 Shell/Console

协议接入层
  |- scpi_server.c
  |- scpi-def.c
  |- dtu.c

业务编排层
  |- sync_rs422tx_channel.c
  |- sync_rs422rx_channel.c
  |- test_case.c

基础设施层
  |- driver/
  |- net_manager/
  |- env/
  |- log/
  |- file_manager.*
```

### 8.2 两条主要外部交互路径

#### 控制路径

SCPI 主要用于：

- 查询状态
- 打开/关闭通道
- 配置测试参数
- 启动/暂停/恢复/停止测试

这一条路径属于命令与控制面。

#### 数据路径

DTU UDP 主要用于：

- 接收上位机发送的 protobuf 数据
- 回传 RS422 TX/RX 数据帧
- 上报异步事件或数据结果

这一条路径属于数据面。

## 9. RS422 相关业务关系

### `sync_rs422tx_channel.*`

该模块负责：

- TX 通道打开与关闭
- 与 SPI/PWM 触发链路协同
- 发送完成事件处理
- 数据打包和上报

它是同步 RS422 发送侧的执行模块。

### `sync_rs422rx_channel.*`

该模块负责：

- RX 通道打开与关闭
- 从 SPI Slave 侧接收数据
- 按通道规则做回环/填充处理
- 对接收结果进行打包和上报

它是同步 RS422 接收侧的执行模块。

### `test_case.*`

该模块是业务编排中心，负责把 JSON 配置和输入文件转换成可执行的测试流程，包括：

- 读取测试配置
- 校验输入数据 MD5
- 数据分片/分块
- 串行或并行发送模式控制
- 测试状态机管理：
  - `IDLE`
  - `RUNNING`
  - `PAUSED`
  - `STOPPED`

可以简单理解为：

- `test_case.c` 负责编排
- `sync_rs422tx_channel.c` 与 `sync_rs422rx_channel.c` 负责执行

### 9.1 自动化测试用例执行方案

自动化测试用例以 `test_case.c` 为调度中心，核心目标是把“配置文件 + 文件数据 + 通道启动”编排成可重复执行的测试流程。

#### 测试启动顺序

测试任务启动时，流程如下：

1. 读取 JSON 测试配置文件。
2. 按配置加载所有测试通道的文件、MD5 和分片参数。
3. 在真正开始测试前先打开 BNC 通道。
4. 打开所有有效的 RS422 TX 测试通道。
5. 根据 `send_mode` 进入串行或并行发送流程。
6. 测试结束后，按相反顺序关闭 RS422 TX 通道和 BNC 通道。

#### 配置字段

当前测试配置主要包含以下字段：

- `send_mode`
  - `sequential`：串行发送
  - `parallel`：并行发送
- `testcycles`
  - 测试循环次数，`0` 表示无限循环
- `SYNCRS422TXchannels`
  - 每个元素描述一个 RS422 TX 测试通道
  - 常用字段包括：
    - `channel_id`
    - `file_name`
    - `md5`
    - `chunk_size`
    - `frame_interval`

#### 串行发送流程

串行模式下，同一时刻只推进一个通道，通道之间严格等待：

1. 取出当前通道的文件数据。
2. 按 `chunk_size` 计算分片大小，4 片为一个发送单元。
3. 先调用 `sync_rs422tx_channel_write()` 完成数据装载。
4. 再调用 `sync_rs422tx_channel_start_transfer()` 启动传输。
5. 通过 `SPI_PL_BUS_EVENT_TRANSFER_DONE` 对应的事件等待本次分片完成。
6. 如果配置了 `frame_interval`，则在下一次分片前延时。
7. 当前通道文件发送完成后，再切换到下一个通道。

串行模式的超时规则统一为：

`chunk_size(bits) * 1000 / baudrate + 1 ms`

其中 `baudrate` 取自当前通道的 SPI 配置，超时判断按单通道分片完成事件进行。

#### 并行发送流程

并行模式下，多个通道同时启动，通道之间不等待彼此：

1. 测试用例加载阶段先锁定所有有效通道，只遍历这些通道。
2. 在真正开始测试前，先打开 BNC 通道。
3. 如果是并行模式，先做用例合法性校验。
   - 所有有效通道都必须成功加载文件。
   - 所有有效通道的 `file_size`、`chunk_size`、`chunk_count` 必须一致。
   - 只要有一个有效通道不满足条件，就直接判定用例非法，不启动测试。
4. 首帧数据装载完成后，统一调用 BNC 启动入口 `bnc_channel_start()`。
5. 并行批次的完成信号不再依赖各通道独立完成事件，而是等待 BNC 提供的统一触发事件 `BNC_CHANNEL_NOTIFY_TRIGGER_TR`。
6. BNC 事件到来后，再统一推进所有有效通道的分片进度。
7. 如果需要继续下一轮，则重新装载下一批分片并再次调用 BNC 启动。

并行模式的超时规则与串行一致，仍按：

`chunk_size(bits) * 1000 / baudrate + 1 ms`

不过并行模式下按当前批次所有有效通道里的最大单通道超时作为整轮等待上限，避免某个慢通道拖住整体流程。

#### 事件与状态

测试用例内部使用事件组做控制和同步，主要包括：

- `TEST_EVENT_START`
- `TEST_EVENT_PAUSE`
- `TEST_EVENT_RESUME`
- `TEST_EVENT_STOP`
- `TEST_EVENT_BNC_TRIGGER`

串行模式仍然使用 `SPI_PL_BUS_EVENT_TRANSFER_DONE` 作为单通道分片完成依据，由通道事件回调置位对应的完成事件。
并行模式则由 BNC 事件回调把 `BNC_CHANNEL_NOTIFY_TRIGGER_TR` 转成测试事件组里的 `TEST_EVENT_BNC_TRIGGER`，作为整批分片完成的统一依据。

#### 设计原则

这套方案的目标是尽量复用公共代码：

- 通道准备、数据装载、超时计算、关闭清理都尽量复用同一套函数。
- 串行和并行的差异主要保留在“调度策略”和“完成事件来源”上。
- BNC 只负责并行模式的统一启动/统一完成触发，不参与 RS422 分片数据装载逻辑。

## 10. 对外服务端口

从当前代码可以看到，主要开放的端口包括：

- SCPI Device Port：`5025`
- SCPI Control Port：`5026`
- DTU UDP Port：`5000`

这些端口是联调和上位机接入时最重要的入口。

## 11. 建议阅读顺序

如果第一次接手这个仓库，建议按下面顺序阅读代码：

1. `test/src/main.c`
2. `test/src/net_manager/net_manager.c`
3. `test/src/scpi_server.c`
4. `test/src/scpi-def.c`
5. `test/src/dtu.c`
6. `test/src/sync_rs422tx_channel.c`
7. `test/src/sync_rs422rx_channel.c`
8. `test/src/test_case.c`
9. `test/src/driver/`

这样可以从：

- 系统启动
- 到外部接口
- 到业务执行
- 再到底层硬件集成

逐层建立对工程的理解。

## 12. 总结

这个工作区本质上是一个运行在 Xilinx MicroBlaze 平台上的测试与通信固件工程，整体架构可概括为：

- 平台层
  - Xilinx 硬件平台与 BSP
- 系统层
  - FreeRTOS、lwIP、存储、日志、环境参数
- 接口层
  - SCPI、DTU、Shell
- 业务层
  - 同步 RS422 TX/RX 与测试流程编排

## 13. 后续建议补充的文档

接下来最值得继续补充的两份文档是：

1. `test_case.json` 示例及字段说明
2. SCPI 命令清单与 DTU protobuf 消息说明
