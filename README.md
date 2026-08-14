# 电力电子监测上位机（Xiangmu）

一款基于 **Qt 6** 的电力电子监测上位机，支持**串口**与 **WiFi** 双模通讯，可实时监控输入/输出电压、输入/输出电流、温度、湿度共 6 路信号，并提供波形观测、数据收发与可视化仪表显示。

> 项目代号：`xiangmu`（GitHub 仓库名 `second`，描述“上位机助手”）

---

## 功能特性

- **双模通讯**
  - 串口：基于 `QSerialPort`，支持波特率、数据位、校验位、停止位配置
  - WiFi：支持 TCP Server / TCP Client / UDP 三种子模式，支持 ESP8266 等模组的 AT 指令
- **6 路实时监测**
  - 输入电压 `Vin`、输出电压 `Vout`
  - 输入电流 `Iin`、输出电流 `Iout`
  - 温度 `Temp`、湿度 `Humi`
- **实时数值面板**：2×3 网格卡片展示 6 项数据，附带原始数据日志窗口
- **波形观测**：2×2 四象限滚动波形（电压双线 / 电流双线 / 温度 / 湿度），支持鼠标拖动滚动、`Ctrl`+滚轮缩放时间窗口
- **可视化面板**：自绘仪表盘 + 3 个 LED 状态指示灯（串口 / 数据收发 / WiFi）
- **数据收发面板**：HEX / 文本双模式显示与发送、时间戳标注、自动换行、定时发送、收发字节计数
- **工程健壮性**
  - 通讯 IO 运行在独立子线程（`CommWorker`），不阻塞 UI
  - 跨线程调用统一使用 `QMetaObject::invokeMethod` + `Qt::QueuedConnection`
  - 死区滤波：解析值与上次之差小于阈值时忽略，避免传感器抖动导致 UI 频繁刷新
- **辅助功能**：保存接收日志、一键清空数据、波形全屏切换、关于对话框

---

## 界面架构

```
┌──────────────────────────────────────────────────────┐
│  A (QMainWindow) — 主窗口 / 应用总控制器               │
│  ├── 顶部模式栏（串口通讯 / WiFi通讯 单选）            │
│  ├── 内容栈 QStackedWidget                            │
│  │   ├── 主界面                                       │
│  │   │   ├── 配置栈（SerialCfgPanel / WiFiCfgPanel）  │
│  │   │   ├── RealtimePanel（实时数值 + 日志）          │
│  │   │   └── QSplitter（DataPanel | VisualPanel）     │
│  │   └── 波形全屏（B，2×2 四象限）                    │
│  ├── 底部状态栏（模式 / 信号 / 日期 / 时间）           │
│  ├── CommWorker（运行在子线程 m_commThread）           │
│  └── m_commThread（Qt 事件循环子线程）                 │
└──────────────────────────────────────────────────────┘
```

**跨线程数据流**

```
面板 → A::槽 → QMetaObject::invokeMethod → CommWorker（子线程）
CommWorker 信号 → A::槽（主线程）→ 面板更新
```

---

## 数据协议

上位机按行解析下位机数据，支持两种格式：

**1. Key-Value 格式（推荐）**

```
Vin:12.05 Vout=5.01 Iin:1.02 Iout:0.51 Temp:25.0 Humi:58.0
```

冒号 `:` 或等号 `=` 均可作为分隔符，字段顺序不限。

**2. 纯数字回退格式**

```
12.05 5.01 1.02 0.51 25.0 58.0
```

6 个数字依次对应：

| 顺序 | 字段 | 含义 | 单位 |
| --- | --- | --- | --- |
| 1 | Vin | 输入电压 | V |
| 2 | Vout | 输出电压 | V |
| 3 | Iin | 输入电流 | A |
| 4 | Iout | 输出电流 | A |
| 5 | Temp | 温度 | °C |
| 6 | Humi | 湿度 | % |

---

## 技术栈

- **框架**：Qt 6.11.0（Core / Widgets / SerialPort / Network / Svg）
- **语言**：C++17
- **构建**：CMake（≥ 3.19）+ Ninja
- **编译器**：MinGW 13.1.0 或 MSVC 2022

---

## 构建

### 环境要求

- Qt 6.5 及以上（本机验证版本 6.11.0）
- CMake ≥ 3.19
- 支持 C++17 的编译器

### MinGW 构建（本机默认）

```bat
set PATH=D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;%PATH%

cmake -S . -B build_release -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=D:/Qt/6.11.0/mingw_64 ^
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe

cmake --build build_release
```

产物为 `build_release/xiangmu.exe`。

### MSVC 构建

项目已提供 `CMakePresets.json`，可用 Visual Studio / Qt Creator 直接选择 `msvc2022_64` 预设，或手动：

```bat
cmake --preset msvc2022_64
cmake --build --preset msvc2022_64
```

---

## 打包发布（Windows）

编译完成后，用 `windeployqt` 收集依赖：

```bat
mkdir release
copy build_release\xiangmu.exe release\

D:\Qt\6.11.0\mingw_64\bin\windeployqt.exe ^
  --release --compiler-runtime release\xiangmu.exe
```

`windeployqt` 会自动收集 Qt DLL、MinGW 运行时、平台插件（`platforms/`、`styles/`、`imageformats/` 等）与翻译文件，得到可直接分发运行的发布目录。

---

## 使用说明

### 串口模式

1. 顶部选择「串口通讯」。
2. 在串口配置面板刷新并选择串口号，配置波特率 / 数据位 / 校验位 / 停止位。
3. 点击「打开串口」，连接状态变绿后即可收发数据。

### WiFi 模式

1. 顶部选择「WiFi 通讯」。
2. 选择工作模式：STA（连接外部 AP）或 AP（本机热点），填写 SSID 与密码，点击「连接 WiFi」。
3. 选择通讯子模式：TCP Server / TCP Client / UDP，填写 IP 与端口。
4. 点击「打开网络」建立数据通道。
5. 需要调试模组时，可在 AT 指令框输入指令（如 `AT`、`AT+CWJAP="ssid","pwd"`）并发送。

---

## 项目结构

```
xiangmu/
├── CMakeLists.txt          # 构建配置
├── CMakePresets.json       # MSVC 预设
├── main.cpp                # 程序入口
├── A.h / A.cpp             # 主窗口 / 应用总控制器
├── B.h / B.cpp             # 2×2 四象限波形面板
├── CommWorker.h / .cpp     # 通讯工作类（串口 / TCP / UDP，子线程）
├── SerialCfgPanel.h / .cpp # 串口配置面板
├── WiFiCfgPanel.h / .cpp   # WiFi 配置面板（STA/AP、TCP/UDP、AT 指令）
├── DataPanel.h / .cpp      # 数据收发面板（HEX/文本、定时发送）
├── RealtimePanel.h / .cpp  # 实时数值 + 日志面板
└── VisualPanel.h / .cpp    # 可视化面板（仪表盘 + LED）
```


