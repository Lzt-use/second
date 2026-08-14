/// @file    A.h
/// @brief   主窗口类声明 —— 应用总控制器，协调所有子面板与通讯线程
///
/// 架构概览：
/// ┌──────────────────────────────────────────────────────┐
/// │  A (QMainWindow) — 主线程                            │
/// │  ├── m_topBar      模式切换栏 (串口/WiFi 单选)       │
/// │  ├── m_contentStack (QStackedWidget)                  │
/// │  │   ├── m_mainContent (主界面)                       │
/// │  │   │   ├── m_configStack (QStackedWidget)           │
/// │  │   │   │   ├── SerialCfgPanel / WiFiCfgPanel        │
/// │  │   │   ├── RealtimePanel                            │
/// │  │   │   └── QSplitter (DataPanel | VisualPanel)      │
/// │  │   └── m_wavePanel (B, 波形全屏)                    │
/// │  ├── m_botBar      底部状态栏                          │
/// │  ├── m_commWorker  CommWorker (位于 m_commThread)     │
/// │  └── m_commThread  子线程 (Qt事件循环)                │
/// └──────────────────────────────────────────────────────┘
///
/// 数据流（跨线程信号槽）：
///   面板 → A::槽 → QMetaObject::invokeMethod → CommWorker (子线程)
///   CommWorker 信号 → A::槽 (主线程) → 面板更新
///
/// 死区滤波：
///   解析到的新值与上一次值之差 < kDeadband 时忽略，
///   避免传感器微小抖动造成 UI 频繁刷新。

#pragma once

#include <QMainWindow>
#include <QElapsedTimer>

// 前向声明 —— 各子面板及通讯工作类
class SerialCfgPanel;   ///< 串口配置面板
class WiFiCfgPanel;     ///< WiFi配置面板
class DataPanel;        ///< 数据收发面板
class VisualPanel;      ///< 可视化面板（LED、仪表等）
class RealtimePanel;    ///< 实时数据显示 + 日志面板
class CommWorker;       ///< 通讯工作类，运行在子线程
class B;                ///< 波形面板（B 为缩写/代号）
class QStackedWidget;
class QThread;
class QRadioButton;
class QLabel;
class QAction;
class QTimer;




/// @class A
/// @brief 主窗口 / 应用总控制器
///
/// 职责：
/// - 构建 UI 布局（模式栏、配置面板、数据显示、可视化、底部状态栏）
/// - 管理串口 / WiFi 两种通讯模式切换
/// - 作为 CommWorker（子线程）与各面板之间的协调中枢
/// - 接收并解析下位机数据，经过死区滤波后分发到实时面板和波形面板
/// - 管理连接状态 m_connected，驱动 UI 控件的启用/禁用
class A : public QMainWindow
{
    Q_OBJECT

public:
    /// @brief 构造函数 —— 创建 UI、菜单栏、底部栏、通讯线程，并加载模拟数据
    explicit A(QWidget *parent = nullptr);

    /// @brief 析构函数 —— 停止时钟，退出并等待通讯线程
    ~A() override;

private slots:
    // ──────────────── 模式切换 ────────────────
    /// @brief 串口/WiFi 模式单选按钮切换时触发（当前未使用线内Lambda替代）
    void onModeChanged();

    // ──────────────── 串口操作 ────────────────
    /// @brief 串口面板请求打开串口 → 跨线程调用 CommWorker::openSerial
    void onSerialOpenRequested();
    /// @brief 串口面板请求关闭串口 → 跨线程调用 CommWorker::closeSerial
    void onSerialCloseRequested();

    // ──────────────── WiFi 操作 ────────────────
    /// @brief WiFi面板请求连接 → 发送 AT+CWMODE 及 AT+CWJAP 指令
    void onWiFiConnectRequested();
    /// @brief WiFi面板请求断开 → 发送 AT+CWQAP 指令
    void onWiFiDisconnectRequested();
    /// @brief WiFi面板请求打开网络 → 跨线程调用 CommWorker::openWiFi
    void onWiFiNetworkOpenRequested();
    /// @brief WiFi面板请求关闭网络 → 跨线程调用 CommWorker::closeWiFi
    void onWiFiNetworkCloseRequested();
    /// @brief WiFi面板发送自定义AT指令 → 跨线程调用 CommWorker::sendWiFiAtCmd
    void onWiFiAtCmdRequested(const QString &cmd);

    // ──────────────── 通讯回调 ────────────────
    /// @brief 通讯连接结果回调 (CommWorker::connected 信号)
    /// @param success true=连接成功
    /// @param msg     附加消息
    void onCommConnected(bool success, const QString &msg);
    /// @brief 通讯断开回调 (CommWorker::disconnected 信号)
    void onCommDisconnected();
    /// @brief 接收到数据回调 (CommWorker::dataReceived 信号)
    ///        进行协议解析、死区滤波、波形推送、日志记录
    void onCommDataReceived(const QByteArray &data);
    /// @brief 通讯错误回调 (CommWorker::errorOccurred 信号)
    void onCommError(const QString &error);

    // ──────────────── 数据发送 ────────────────
    /// @brief 数据面板请求发送数据 → 跨线程调用 CommWorker::sendData
    void onSendRequested(const QByteArray &data);

    // ──────────────── 视图 & 时钟 ────────────────
    /// @brief 切换波形全屏视图（普通视图 ↔ 波形全屏）
    void toggleWaveView();
    /// @brief 每秒更新底部栏日期时间
    void updateClock();

    // ──────────────── 菜单操作 ────────────────
    /// @brief 保存接收记录到文本文件
    void onSaveLog();
    /// @brief 清空所有数据（接收/发送/日志/波形）
    void onClearAll();
    /// @brief 显示"关于"对话框
    void onAbout();

private:
    // ──────────────── 初始化 ────────────────
    void setupUi();           ///< 构建主界面布局
    void setupMenuBar();      ///< 构建菜单栏（文件/视图/设置/帮助）
    void setupBottomBar();    ///< 构建底部状态栏（模式/信号/日期/时间）
    void setupConnections();  ///< 连接所有信号槽（含跨线程连接）
    void switchToSerialMode();///< 切换到串口模式
    void switchToWiFiMode();  ///< 切换到WiFi模式

    /// @brief 解析下位机协议文本并更新实时面板
    /// @param text 接收到的文本行
    /// @return 是否成功解析到有效数据
    /// @details 支持两种协议格式：
    ///   1. Key-Value格式: "Vin:12.05 Vout=5.01 Iin:1.02 ..."
    ///   2. 纯数字回退:   "12.05 5.01 1.02 0.51 25.0 58.0"
    bool parseAndUpdateRealtime(const QString &text);

    // ──────────────── 内容切换 ────────────────
    QStackedWidget *m_contentStack  = nullptr;  ///< 内容栈：主界面 ↔ 波形全屏
    QWidget        *m_mainContent   = nullptr;  ///< 主界面包装容器
    B              *m_wavePanel     = nullptr;  ///< 波形面板（嵌入在 m_contentStack 页面1）

    // ──────────────── 配置面板切换 ────────────────
    QStackedWidget *m_configStack   = nullptr;  ///< 配置栈：串口面板 ↔ WiFi面板
    SerialCfgPanel *m_serialPanel   = nullptr;  ///< 串口配置面板
    WiFiCfgPanel   *m_wifiPanel     = nullptr;  ///< WiFi配置面板

    // ──────────────── 数据显示面板 ────────────────
    RealtimePanel  *m_realtimePanel = nullptr;  ///< 实时数值 + 日志面板
    DataPanel      *m_dataPanel     = nullptr;  ///< 数据收发面板
    VisualPanel    *m_visualPanel   = nullptr;  ///< 可视化面板（LED、仪表盘）

    // ──────────────── 通讯层 ────────────────
    /// @brief 通讯工作对象（实际IO在子线程执行）
    CommWorker     *m_commWorker    = nullptr;
    /// @brief 通讯专用子线程，CommWorker 通过 moveToThread 移入
    /// @note 跨线程调用统一使用 QMetaObject::invokeMethod + Qt::QueuedConnection
    QThread        *m_commThread    = nullptr;

    // ──────────────── 模式栏控件 ────────────────
    QRadioButton   *m_serialModeRadio = nullptr;  ///< "串口通讯" 单选按钮
    QRadioButton   *m_wifiModeRadio   = nullptr;  ///< "WiFi通讯" 单选按钮
    QLabel         *m_modeStatusLabel = nullptr;  ///< 连接状态标签（●已连接/●未连接）

    QWidget        *m_topBar       = nullptr;  ///< 模式栏容器
    QWidget        *m_botBar       = nullptr;  ///< 底部状态栏容器

    // ──────────────── 底部状态栏控件 ────────────────
    QLabel         *m_bottomMode   = nullptr;  ///< 当前通讯模式
    QLabel         *m_bottomSignal = nullptr;  ///< 信号强度
    QLabel         *m_bottomDate   = nullptr;  ///< 日期显示
    QLabel         *m_bottomTime   = nullptr;  ///< 时间显示（每秒刷新）
    QTimer         *m_clockTimer   = nullptr;  ///< 时钟定时器（1s周期）

    // ──────────────── 菜单动作 ────────────────
    QAction        *m_waveAct  = nullptr;  ///< "波形观测"菜单项（勾选切换）
    QAction        *m_saveAct  = nullptr;  ///< "保存接收记录"菜单项
    QAction        *m_clearAct = nullptr;  ///< "清空所有数据"菜单项
    QAction        *m_aboutAct = nullptr;  ///< "关于"菜单项

    // ──────────────── 数据缓存 & 死区滤波 ────────────────
    /// @name 当前解析值 —— 最新一次成功解析的传感器数据
    /// @{
    double          m_curVin=0, m_curVout=0, m_curIin=0, m_curIout=0;
    double          m_curTemp=0, m_curHumi=0;
    /// @}

    /// @name 死区滤波上一次值 —— 用于判断变化是否超过阈值
    /// @{
    double          m_lastVin=0, m_lastVout=0, m_lastIin=0, m_lastIout=0;
    double          m_lastTemp=0, m_lastHumi=0;
    /// @}

    // ──────────────── 状态标记 ────────────────
    int             m_currentMode = 0;   ///< 当前模式: 0=串口, 1=WiFi
    bool            m_connected   = false; ///< 当前连接状态（驱动 UI 控件启用/禁用）
    quint64         m_rxBytes = 0;       ///< 累计接收字节数
    quint64         m_txBytes = 0;       ///< 累计发送字节数
    QByteArray      m_rxBuffer;             ///< 接收行缓冲：累积分片数据，按 \n 分割完整行
    QElapsedTimer   m_lastFrameTimer;       ///< 上次收到完整帧的时间，用于计算发送速率
};
