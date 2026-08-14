# ═══════════════════════════════════════════════════════════════
#  CMakeLists.txt —— Xiangmu 上位机构建配置
#
#  项目简介：
#    串口/WiFi 双模调试工具，基于 Qt6 构建，支持串口通讯和 WiFi
#    AT 指令通讯，可实时监控输入/输出电压、电流、温度、湿度。
#
#  构建要求：
#    - CMake >= 3.19
#    - C++17
#    - Qt6 >= 6.5 (Core, Widgets, SerialPort, Network)
# ═══════════════════════════════════════════════════════════════

# ──────── CMake 最低版本要求 ────────
cmake_minimum_required(VERSION 3.19)

# ──────── 项目定义 ────────
project(xiangmu LANGUAGES CXX)

# ──────── C++ 标准 ────────
set(CMAKE_CXX_STANDARD 17)          # 使用 C++17 标准（支持 if constexpr, string_view 等）
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 编译器必须支持 C++17，否则报错

# ──────── Qt 元对象编译自动处理 ────────
# AUTOMOC: 自动运行 moc 处理 Q_OBJECT 宏
# AUTORCC: 自动编译 .qrc 资源文件
# AUTOUIC: 自动编译 .ui 界面文件
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ──────── 查找 Qt6 依赖 ────────
# Core:       Qt 核心库（事件循环、信号槽、元对象系统）
# Widgets:    图形界面控件库
# SerialPort: 串口通讯库
# Network:    TCP/UDP 网络通讯库（WiFi AT 指令通讯通道）
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets SerialPort Network)

# ──────── Qt 项目标准设置 ────────
# 自动处理平台相关的默认配置
qt_standard_project_setup()

# ──────── 可执行文件 ────────
# WIN32:      Windows 下不显示控制台窗口
# MACOSX_BUNDLE: macOS 下打包为 .app
qt_add_executable(xiangmu
    WIN32 MACOSX_BUNDLE
    main.cpp
    A.cpp          A.h          # 主窗口/控制器
    B.cpp          B.h          # 波形面板
    CommWorker.cpp CommWorker.h # 通讯工作类（子线程）
    SerialCfgPanel.cpp SerialCfgPanel.h  # 串口配置面板
    WiFiCfgPanel.cpp   WiFiCfgPanel.h    # WiFi 配置面板
    DataPanel.cpp      DataPanel.h       # 数据收发面板
    RealtimePanel.cpp  RealtimePanel.h   # 实时数据显示+日志面板
    VisualPanel.cpp    VisualPanel.h     # 可视化面板（LED/仪表）
)

# ──────── 链接库 ────────
# PRIVATE: 链接库仅对当前目标可见，不传递给依赖方
target_link_libraries(xiangmu
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::SerialPort       # 串口通讯支持
        Qt6::Network          # TCP 网络支持（WiFi 通道）
)

# ──────── 安装规则 ────────
# BUNDLE  DESTINATION: macOS .app 安装路径
# RUNTIME DESTINATION: Windows .exe / Linux 可执行文件路径
# LIBRARY DESTINATION: 动态库路径（Linux）
install(TARGETS xiangmu
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ──────── 部署脚本生成 ────────
# 自动生成跨平台部署脚本（windeployqt / macdeployqt）
qt_generate_deploy_app_script(
    TARGET xiangmu
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR # 非主流平台不报错，仅跳过
)
install(SCRIPT ${deploy_script})
