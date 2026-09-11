# ZBSAR-C OC 工程说明

## 1. 工程概览

本仓库是面向 ZBSAR-C 硬件平台的 Xilinx Vitis 嵌入式工程工作区，处理器为 `MicroBlaze`。当前平台包含两个软件域：

- `freertos10_xilinx_microblaze_0`：主应用使用的 FreeRTOS 运行环境。
- `standalone_microblaze_0`：Bootloader 使用的 standalone 运行环境。

主应用运行在 FreeRTOS/lwIP 上，负责板级初始化、网络服务、SCPI 控制、DTU 数据通信、BNC 触发以及同步 RS422 收发和测试用例执行。

平台工程由 Vitis 2023.1 创建，平台名称为 `ZBSAR_TOP`。硬件导出文件位于 `ZBSAR_TOP/hw/`，平台导出目录为 `ZBSAR_TOP/export/ZBSAR_TOP/`。

## 2. 顶层目录

```text
oc/
|-- ZBSAR_TOP/          Vitis 平台工程、硬件文件和 MicroBlaze BSP
|-- bootloader/          standalone Bootloader 应用工程
|-- bootloader_system/   Bootloader 对应的 system 工程
|-- test/                FreeRTOS 主应用工程
|-- test_system/         主应用对应的 system 工程
|-- shell/               Shell 组件工程
|-- logc/                日志组件工程
|-- libscpi/             SCPI 协议库工程
|-- lfs/                 littlefs 组件工程
|-- cJson/               cJSON 组件工程
|-- tiny_crypt/          加密和摘要组件工程
|-- doc/                 文档和截图
|-- .metadata/           Vitis/Eclipse 工作区元数据
|-- .vscode/             编辑器配置
`-- readme.md            本说明文档
```

`Debug/` 目录分布在各个 Vitis 工程下，用于保存生成的编译中间文件和输出文件，不是主应用源码目录。

## 3. 平台和构建关系

### `ZBSAR_TOP/`

平台工程，包含：

- `ZBSAR_C.xsa`：硬件平台导出文件。
- `ZBSAR_C.bit`、`ZBSAR_C.mmi`：平台目录下的硬件相关输出文件。
- `export/ZBSAR_TOP/`：导出的 `.xpfm` 平台、硬件文件和软件域/BSP 文件。
- `microblaze_0/`：生成的 FreeRTOS、standalone 及 Xilinx 库文件。

当前 FreeRTOS 域使用 Xilinx `freertos10_xilinx` 和 `lwip213`，并启用 `xilffs`、`xiltimer` 等平台库。lwIP 使用 socket API，平台配置启用了 DHCP。

### `test/` 与 `test_system/`

`test/` 是主应用，运行在 `freertos10_xilinx_microblaze_0` 域。`test_system/` 将主应用和以下组件组织到同一个 system 工程中：

- `test`
- `shell`
- `logc`
- `libscpi`
- `lfs`
- `tiny_crypt`
- `cJson`

Debug 和 Release 配置均声明生成 SD 卡镜像。仓库没有独立的顶层 Makefile 或构建脚本，通常通过 Vitis 导入平台和 system 工程后进行构建、下载和调试。

### `bootloader/` 与 `bootloader_system/`

`bootloader/` 是基于 `standalone_microblaze_0` 的 SREC SPI Bootloader 应用，`bootloader_system/` 是对应的 system 工程。它与 FreeRTOS 主应用是两个独立的应用工程。

## 4. 主应用源码结构

```text
test/src/
|-- main.c                    应用入口和启动流程
|-- driver/
|   |-- platform/             GPIO、SPI、PWM、UART、网络等平台驱动
|   `-- flash/                SPI Flash 访问和 Flash 命令
|-- console/                  UART/Telnet 控制台后端
|-- env/                      环境变量管理
|-- vfs/                      FATFS/littlefs 虚拟文件系统
|-- netutils/
|   |-- telnet/               Telnet 服务
|   |-- tftpd/                TFTP 服务及文件适配
|   |-- sntp/                 SNTP 客户端
|   `-- iperf/                iPerf 性能测试服务
|-- sysmon/                   系统监控和系统命令
|-- nanopb/                   DTU protobuf 定义和编解码代码
|-- scpi_server.c             SCPI TCP 服务
|-- scpi-def.c                SCPI 命令定义和业务映射
|-- dtu.c                     DTU UDP 服务
|-- bnc_channel.c             BNC 通道控制和触发处理
|-- sync_rs422tx_channel.c    同步 RS422 TX 通道
|-- sync_rs422rx_channel.c    同步 RS422 RX 通道
|-- test_case.c               测试用例加载、状态和发送调度
|-- test_case_cmd.c           测试用例命令
`-- test_case.json             测试用例配置示例
```

协议库和通用组件以独立 Vitis 工程存在，主应用通过 `test_system/test_system.sprj` 参与构建，而不是放在 `test/src/` 下的同名目录中。

## 5. 启动流程

`test/src/main.c` 的实际启动过程如下：

```text
main()
  -> 使能 MicroBlaze 指令/数据缓存
  -> 创建 main_thread
  -> 启动 FreeRTOS 调度器

main_thread()
  -> 设置编译时间
  -> 安装 cJSON 内存钩子
  -> 初始化引脚、FATFS、littlefs 和 VFS
  -> 初始化日志和环境变量
  -> 初始化 UART、PWM、风扇和网络交换芯片
  -> 初始化 lwIP 网络
  -> 初始化同步 RS422 TX/RX 和 BNC 通道
  -> 启动系统监控、SNTP、TFTP、SCPI 和 DTU 服务
  -> 注册 Flash、环境、VFS、系统、网络、iPerf 和测试命令
  -> 初始化控制台
  -> 删除 main_thread，后续由各服务任务运行
```

代码中声明了 `uart16550_init()` 和 `hwtimer_init()`，但当前 `main_thread()` 没有调用它们；文档不将它们列为实际启动步骤。

## 6. 网络配置和服务

网络接口由 `test/src/driver/platform/network.c` 初始化。环境变量键名和默认值为：

| 键 | 默认值 |
|---|---|
| `IP` | `192.168.0.10` |
| `mask` | `255.255.255.0` |
| `gateway` | `192.168.0.1` |
| `mac` | `00:0A:35:00:01:02` |
| `ntpserver` | `192.168.1.215` |

当前平台配置启用了 DHCP。DHCP 超时或未启用 DHCP 时，代码使用上述 IP、掩码和网关；MAC 地址仍从 `mac` 环境变量读取。

主应用中定义的服务端口如下：

| 服务 | 协议 | 端口 | 源码位置 |
|---|---|---:|---|
| SCPI 设备接口 | TCP | `5025` | `test/src/scpi_server.c` |
| SCPI 控制接口 | TCP | `5026` | `test/src/scpi_server.c` |
| DTU | UDP | `5000` | `test/src/dtu.c` |
| Telnet 控制台 | TCP | `23` | `test/src/netutils/telnet/telnet.h` |
| TFTP | UDP | `69` | lwIP 标准 TFTP 端口 |
| iPerf TCP 默认端口 | TCP | `5001` | `test/src/netutils/iperf/lwiperf.h` |

SCPI 和 DTU 是业务通信接口；Telnet、TFTP、SNTP 和 iPerf 属于运行时支持或调试服务。

## 7. 业务模块

### SCPI

`scpi_server.c` 创建两个 TCP 监听端口：设备端口 `5025` 和控制端口 `5026`。`scpi-def.c` 定义 SCPI 命令，并将命令映射到通道、BNC、测试用例和系统操作。

### DTU

`dtu.c` 在 UDP `5000` 端口接收 nanopb 编码的消息，根据消息类型分发给 RS422 等业务模块，并提供数据回传接口。protobuf 定义和生成代码位于 `test/src/nanopb/`。

### RS422 和 BNC

- `sync_rs422tx_channel.*`：同步 RS422 TX 通道的打开、配置、数据装载、启动和完成事件处理。
- `sync_rs422rx_channel.*`：同步 RS422 RX 通道的打开、配置、接收和数据上报。
- `bnc_channel.*`：BNC 通道配置、计数、触发和事件回调。
- `driver/platform/spi_pl.*`、`spi_pl_slave.*`、`pwm_tr.*`：为上述通道提供 SPI、SPI Slave 和 PWM 触发能力。

### 测试用例

`test_case.c` 使用 FreeRTOS 任务和事件组管理测试状态，支持开始、暂停、恢复和停止。发送调度由 BNC 配置中的并行模式决定，不能通过 README 中曾描述的 `send_mode` 字段切换。

测试用例支持：

- 文件模式和码片模式，由 `file_mode` 字段决定。
- 串行发送。
- BNC 主/从并行发送。
- 测试循环次数控制，`testcycles` 为 `0` 时表示无限循环。
- 按 `chunk_size` 计算分片，并以 4 个分片为一个发送单元。
- 并行模式下校验有效通道的文件大小、分片大小和分片数量一致。

配置示例位于 `test/src/test_case.json`，主要字段如下：

```json
{
    "file_mode": true,
    "testcycles": 10,
    "SYNCRS422TXchannels": [
        {
            "channel_id": 1,
            "file_name": "file1.bin",
            "file_size": 5321,
            "md5": "d41d8cd98f00b204e9800998ecf8427e",
            "chunk_size": 4260,
            "frame_interval": 0
        }
    ]
}
```

`frame_interval` 可按通道配置，单位为毫秒；`file_size` 单位为字节，`chunk_size` 单位为 bit。实际测试还要求文件能够按当前分片规则完整拆分，并满足最小分片数量和发送单元约束。

## 8. 建议阅读顺序

建议按以下顺序了解主应用：

1. `test/src/main.c`
2. `test/src/driver/platform/network.c`
3. `test/src/scpi_server.c` 和 `test/src/scpi-def.c`
4. `test/src/dtu.c`
5. `test/src/bnc_channel.c`
6. `test/src/sync_rs422tx_channel.c`
7. `test/src/sync_rs422rx_channel.c`
8. `test/src/test_case.c` 和 `test/src/test_case.json`
9. `test/src/driver/`、`test/src/vfs/`、`test/src/netutils/`

这样可以从平台启动、网络和外部接口逐步进入通道驱动与测试调度。

## 9. 当前状态说明

仓库包含 Vitis 生成的工程、BSP 和硬件输出文件。构建、下载和调试依赖对应的 Xilinx/Vitis 工具链以及目标硬件；README 不假设存在可在普通主机上直接运行的构建或测试命令。
