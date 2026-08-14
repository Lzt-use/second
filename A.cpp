/// @file    A.cpp
/// @brief   主窗口类实现 —— 全部 UI 构建、跨线程协调、协议解析、数据分发
///
/// 核心设计：
///   1. CommWorker 通过 moveToThread 移入子线程，
///      所有 IO 操作通过 QMetaObject::invokeMethod + Qt::QueuedConnection 跨线程调用
///   2. 面板信号 → A 槽函数 → invokeMethod → CommWorker（子线程执行）
///      CommWorker 信号 → A 槽函数 → 更新面板（主线程 UI）
///   3. m_connected 标志统一管理连接状态，驱动全部 UI 控件可用性
///   4. parseAndUpdateRealtime 先按 Key-Value 格式解析，失败则回退到纯数字格式
///   5. 死区滤波 (deadband) 抑制传感器微小抖动，避免 UI 频繁刷新

#include "A.h"
#include "SerialCfgPanel.h"
#include "WiFiCfgPanel.h"
#include "DataPanel.h"
#include "VisualPanel.h"
#include "RealtimePanel.h"
#include "B.h"
#include "CommWorker.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QRadioButton>
#include <QLabel>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QtMath>
#include <functional>

// ═══════════════════════════════════════════════════════════════
//  构造函数 —— 初始化 UI、通讯线程、时钟
// ═══════════════════════════════════════════════════════════════
A::A(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Xiangmu - 串口/WiFi 上位机"));
    resize(1050, 780);
    setMinimumSize(920, 680);

    // 创建通讯工作对象和专用线程（尚未启动）
    m_commWorker = new CommWorker;
    m_commThread = new QThread(this);

    setupUi();          // 构建主界面布局
    setupMenuBar();     // 构建菜单栏
    setupBottomBar();   // 构建底部状态栏
    setupConnections(); // 连接所有信号槽（包括跨线程连接）

    // ═══════════════════════════════════════════════
    // 跨线程架构核心：
    //   将 CommWorker 移入子线程 m_commThread，
    //   此后所有 IO 操作都在子线程事件循环中执行，
    //   不会阻塞主线程 UI。
    // ═══════════════════════════════════════════════
    m_commWorker->moveToThread(m_commThread);
    m_commThread->start();

    // 每秒刷新底部栏日期/时间
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &A::updateClock);
    m_clockTimer->start(1000);
    updateClock();

    m_visualPanel->setGaugeLabel(QStringLiteral("速率(Hz)"));
    m_visualPanel->setGaugeValue(0.0, 0.0, 50.0);
}

// ═══════════════════════════════════════════════════════════════
//  析构函数 —— 优雅关闭通讯线程
// ═══════════════════════════════════════════════════════════════
A::~A()
{
    m_clockTimer->stop();           // 停止时钟
    m_commThread->quit();           // 请求子线程退出
    m_commThread->wait(2000);       // 最多等待 2 秒，防止死锁
}

// ═══════════════════════════════════════════════════════════════
//  setupUi —— 构建主界面布局
//
//  布局结构（垂直）：
//  ┌─────────────────────────────────────┐
//  │ m_topBar  模式切换栏                │
//  ├─────────────────────────────────────┤
//  │ m_contentStack (QStackedWidget)     │
//  │  ├─ [0] m_mainContent (主界面)      │
//  │  │    ├─ m_configStack (配置面板)   │
//  │  │    ├─ RealtimePanel              │
//  │  │    └─ QSplitter (Data | Visual)  │
//  │  └─ [1] m_wavePanel (波形全屏)      │
//  └─────────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════
void A::setupUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setSpacing(3);
    root->setContentsMargins(6, 3, 6, 3);

    // ── 模式切换栏 ──
    m_topBar = new QWidget;
    auto *modeBar = new QHBoxLayout(m_topBar);
    modeBar->setContentsMargins(0, 0, 0, 0);
    modeBar->setSpacing(8);
    auto *modeLbl = new QLabel(QStringLiteral("通讯模式:"));
    modeLbl->setStyleSheet("background:transparent; font-weight:bold; color:#b0b8d0;");
    modeBar->addWidget(modeLbl);
    m_serialModeRadio = new QRadioButton(QStringLiteral("串口通讯"));
    m_wifiModeRadio   = new QRadioButton(QStringLiteral("WiFi通讯"));
    m_serialModeRadio->setChecked(true);            // 默认选中串口模式
    auto *mg = new QButtonGroup(this);              // 互斥组 —— 同一时间只能选一个
    mg->addButton(m_serialModeRadio); mg->addButton(m_wifiModeRadio);
    modeBar->addWidget(m_serialModeRadio);
    modeBar->addWidget(m_wifiModeRadio);
    modeBar->addSpacing(24);
    m_modeStatusLabel = new QLabel(QStringLiteral("● 未连接"));
    m_modeStatusLabel->setStyleSheet("background:transparent; font-weight:bold; color:#ff6b6b; font-size:14px;");
    modeBar->addWidget(m_modeStatusLabel);
    modeBar->addStretch();
    root->addWidget(m_topBar);

    // ═══════════════════════════════════════════
    //  内容切换 QStackedWidget（主界面 / 波形）
    // ═══════════════════════════════════════════
    m_contentStack = new QStackedWidget(this);

    // ── 页面0：主界面包装 ──
    m_mainContent = new QWidget;
    auto *mainLay = new QVBoxLayout(m_mainContent);
    mainLay->setSpacing(3);
    mainLay->setContentsMargins(0, 0, 0, 0);

    // 配置面板栈 —— 根据当前模式显示串口或WiFi面板
    m_configStack = new QStackedWidget;
    m_serialPanel = new SerialCfgPanel;
    m_wifiPanel   = new WiFiCfgPanel;
    m_configStack->addWidget(m_serialPanel);
    m_configStack->addWidget(m_wifiPanel);
    m_configStack->setCurrentIndex(0);              // 默认显示串口面板
    mainLay->addWidget(m_configStack);

    m_realtimePanel = new RealtimePanel;
    mainLay->addWidget(m_realtimePanel);

    // 数据面板与可视化面板水平分割
    auto *contentSplit = new QSplitter(Qt::Horizontal);
    m_dataPanel   = new DataPanel(contentSplit);
    m_visualPanel = new VisualPanel(contentSplit);
    contentSplit->addWidget(m_dataPanel);
    contentSplit->addWidget(m_visualPanel);
    contentSplit->setStretchFactor(0, 6);           // 数据面板占 6/8
    contentSplit->setStretchFactor(1, 2);           // 可视化面板占 2/8
    contentSplit->setSizes({700, 240});
    mainLay->addWidget(contentSplit, 1);            // stretch=1 允许扩展

    m_contentStack->addWidget(m_mainContent);       // index 0

    // ── 页面1：全屏波形 ──
    m_wavePanel = new B;
    m_contentStack->addWidget(m_wavePanel);         // index 1

    m_contentStack->setCurrentIndex(0);             // 默认显示主界面
    root->addWidget(m_contentStack, 1);
}

// ═══════════════════════════════════════════════════════════════
//  setupBottomBar —— 底部状态栏
//  显示：通讯模式 | 信号强度 | (弹簧) | 日期 | 时间
// ═══════════════════════════════════════════════════════════════
void A::setupBottomBar()
{
    m_botBar = new QWidget;
    auto *bar = new QHBoxLayout(m_botBar);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(16);
    bar->setContentsMargins(8, 2, 8, 2);
    m_bottomMode   = new QLabel(QStringLiteral("模式: 串口通讯"));
    m_bottomSignal = new QLabel(QStringLiteral("信号: ---"));
    m_bottomDate   = new QLabel;
    m_bottomTime   = new QLabel;
    QString s("background:transparent; color:#8a90a8; font-size:11px;");
    m_bottomMode->setStyleSheet(s);
    m_bottomSignal->setStyleSheet(s);
    m_bottomDate->setStyleSheet(s);
    m_bottomTime->setStyleSheet("background:transparent; color:#b0b8d0; font-size:11px; font-weight:bold;");
    bar->addWidget(m_bottomMode);
    bar->addWidget(m_bottomSignal);
    bar->addStretch();                              // 弹簧：把日期/时间推到右侧
    bar->addWidget(m_bottomDate);
    bar->addWidget(m_bottomTime);
    auto *rt = qobject_cast<QVBoxLayout *>(centralWidget()->layout());
    if (rt) rt->addWidget(m_botBar);
}

// ═══════════════════════════════════════════════════════════════
//  setupMenuBar —— 菜单栏
//  文件(F): 保存接收记录 | 退出
//  视图(V): 波形观测 (Ctrl+B, 勾选切换)
//  设置(S): 清空所有数据
//  帮助(H): 关于
// ═══════════════════════════════════════════════════════════════
void A::setupMenuBar()
{
    // ── 文件菜单 ──
    auto *fm = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    m_saveAct = new QAction(QStringLiteral("保存接收记录..."), this); fm->addAction(m_saveAct);
    fm->addSeparator();
    auto *ex = new QAction(QStringLiteral("退出(&X)"), this);
    ex->setShortcut(QKeySequence("Alt+F4"));
    connect(ex, &QAction::triggered, this, &QWidget::close);
    fm->addAction(ex);

    // ── 视图菜单 ──
    auto *vm = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    m_waveAct = new QAction(QStringLiteral("波形观测  (&B)"), this);
    m_waveAct->setShortcut(QKeySequence("Ctrl+B"));
    m_waveAct->setCheckable(true);                  // 勾选状态与波形视图同步
    vm->addAction(m_waveAct);

    // ── 设置菜单 ──
    auto *em = menuBar()->addMenu(QStringLiteral("设置(&S)"));
    m_clearAct = new QAction(QStringLiteral("清空所有数据"), this); em->addAction(m_clearAct);

    // ── 帮助菜单 ──
    auto *hm = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    m_aboutAct = new QAction(QStringLiteral("关于..."), this); hm->addAction(m_aboutAct);
}

// ═══════════════════════════════════════════════════════════════
//  setupConnections —— 信号槽连接中枢
//
//  数据流全景：
//  ┌────────────┐    信号     ┌─────┐  invokeMethod   ┌─────────────┐
//  │ 各配置面板  │ ──────────→ │  A  │ ──────────────→ │ CommWorker  │
//  │ DataPanel  │             │槽函数│ (QueuedConnection)│ (子线程)    │
//  └────────────┘             └─────┘                  └──────┬──────┘
//       ▲                         ▲                          │
//       │                         │ 信号 (connected/         │
//       │  直接调用更新UI           │  disconnected/           │
//       └─────────────────────────┘  dataReceived/error)     │
//                                   跨线程自动排队到主线程 ───┘
// ═══════════════════════════════════════════════════════════════
void A::setupConnections()
{
    // ─── 模式切换 ───
    connect(m_serialModeRadio, &QRadioButton::toggled, this, [this](bool o){ if(o) switchToSerialMode(); });
    connect(m_wifiModeRadio,   &QRadioButton::toggled, this, [this](bool o){ if(o) switchToWiFiMode(); });

    // ─── 串口面板 → A ───
    connect(m_serialPanel, &SerialCfgPanel::openRequested,  this, &A::onSerialOpenRequested);
    connect(m_serialPanel, &SerialCfgPanel::closeRequested, this, &A::onSerialCloseRequested);

    // ─── WiFi面板 → A ───
    connect(m_wifiPanel, &WiFiCfgPanel::connectWiFiRequested,    this, &A::onWiFiConnectRequested);
    connect(m_wifiPanel, &WiFiCfgPanel::disconnectWiFiRequested, this, &A::onWiFiDisconnectRequested);
    connect(m_wifiPanel, &WiFiCfgPanel::openNetworkRequested,    this, &A::onWiFiNetworkOpenRequested);
    connect(m_wifiPanel, &WiFiCfgPanel::closeNetworkRequested,   this, &A::onWiFiNetworkCloseRequested);
    connect(m_wifiPanel, &WiFiCfgPanel::sendAtCmdRequested,      this, &A::onWiFiAtCmdRequested);

    // ─── CommWorker → A（跨线程信号，Qt自动排队到主线程）───
    connect(m_commWorker, &CommWorker::connected,    this, &A::onCommConnected);
    connect(m_commWorker, &CommWorker::disconnected, this, &A::onCommDisconnected);
    connect(m_commWorker, &CommWorker::dataReceived, this, &A::onCommDataReceived);
    connect(m_commWorker, &CommWorker::errorOccurred,this, &A::onCommError);

    // ─── DataPanel → A → CommWorker ───
    connect(m_dataPanel, &DataPanel::sendRequested, this, &A::onSendRequested);

    // ─── 菜单动作 ───
    connect(m_waveAct,  &QAction::triggered, this, &A::toggleWaveView);
    connect(m_saveAct,  &QAction::triggered, this, &A::onSaveLog);
    connect(m_clearAct, &QAction::triggered, this, &A::onClearAll);
    connect(m_aboutAct, &QAction::triggered, this, &A::onAbout);
}

// ═══════════════════════════════════════════════════════════════
//  onModeChanged —— 模式切换回调（预留，当前由Lambda直接处理）
// ═══════════════════════════════════════════════════════════════
void A::onModeChanged() {}

// ═══════════════════════════════════════════════════════════════
//  switchToSerialMode —— 切换到串口模式
//  若当前WiFi已连接则先关闭WiFi连接
// ═══════════════════════════════════════════════════════════════
void A::switchToSerialMode()
{
    // 如果在WiFi模式下已连接，先优雅关闭WiFi
    if (m_connected && m_currentMode == 1)
        QMetaObject::invokeMethod(m_commWorker, "closeWiFi", Qt::QueuedConnection);
    m_currentMode = 0;
    m_configStack->setCurrentIndex(0);              // 显示串口配置面板
    m_bottomMode->setText(QStringLiteral("模式: 串口通讯"));
    m_bottomSignal->setText(QStringLiteral("信号: ---"));
}

// ═══════════════════════════════════════════════════════════════
//  switchToWiFiMode —— 切换到WiFi模式
//  若当前串口已连接则先关闭串口
// ═══════════════════════════════════════════════════════════════
void A::switchToWiFiMode()
{
    // 如果在串口模式下已连接，先优雅关闭串口
    if (m_connected && m_currentMode == 0)
        QMetaObject::invokeMethod(m_commWorker, "closeSerial", Qt::QueuedConnection);
    m_currentMode = 1;
    m_configStack->setCurrentIndex(1);              // 显示WiFi配置面板
    m_bottomMode->setText(QStringLiteral("模式: WiFi通讯"));
    m_bottomSignal->setText(QStringLiteral("信号: ---"));
}

// ═══════════════════════════════════════════════════════════════
//  onSerialOpenRequested —— 串口面板 → 跨线程打开串口
//
//  跨线程调用链（关键架构模式）：
//    串口面板信号 (主线程)
//    → A::onSerialOpenRequested (主线程)
//    → QMetaObject::invokeMethod(m_commWorker, "openSerial", Qt::QueuedConnection)
//    → CommWorker::openSerial (子线程) —— 实际执行 QSerialPort::open
//    → CommWorker 发出 connected/disconnected 信号
//    → Qt 自动将信号排队回 A 槽 (主线程) —— 更新 UI
// ═══════════════════════════════════════════════════════════════
void A::onSerialOpenRequested()
{
    // 确保当前处于串口模式
    if (m_currentMode != 0) m_serialModeRadio->setChecked(true);
    // 跨线程调用：将串口参数打包传递给子线程中的 CommWorker
    QMetaObject::invokeMethod(m_commWorker, "openSerial", Qt::QueuedConnection,
        Q_ARG(QString, m_serialPanel->portName()), Q_ARG(int, m_serialPanel->baudRate()),
        Q_ARG(int, m_serialPanel->dataBitsIdx()),  Q_ARG(int, m_serialPanel->parityIdx()),
        Q_ARG(int, m_serialPanel->stopBitsIdx()));
}

void A::onSerialCloseRequested()
{ QMetaObject::invokeMethod(m_commWorker, "closeSerial", Qt::QueuedConnection); }

// ═══════════════════════════════════════════════════════════════
//  onWiFiConnectRequested —— WiFi面板 → 发送AT连接指令
//
//  流程：
//   1. 根据 STA/AP 模式发送 AT+CWMODE=1 或 AT+CWMODE=2
//   2. 发送 AT+CWJAP="SSID","PASSWORD" 连接WiFi热点
//  所有 AT 指令都通过跨线程 invokeMethod 发送到子线程
// ═══════════════════════════════════════════════════════════════
void A::onWiFiConnectRequested()
{
    QString m = m_wifiPanel->isStaMode() ? "1" : "2";
    QMetaObject::invokeMethod(m_commWorker, "sendWiFiAtCmd", Qt::QueuedConnection,
        Q_ARG(QString, QString("AT+CWMODE=%1").arg(m)));
    QString s = m_wifiPanel->ssid();
    if (!s.isEmpty())
        QMetaObject::invokeMethod(m_commWorker, "sendWiFiAtCmd", Qt::QueuedConnection,
            Q_ARG(QString, QString("AT+CWJAP=\"%1\",\"%2\"").arg(s, m_wifiPanel->password())));
}

void A::onWiFiDisconnectRequested()
{ QMetaObject::invokeMethod(m_commWorker, "sendWiFiAtCmd", Qt::QueuedConnection,
    Q_ARG(QString, QStringLiteral("AT+CWQAP"))); }

void A::onWiFiNetworkOpenRequested()
{
    if (m_currentMode != 1) m_wifiModeRadio->setChecked(true);
    QMetaObject::invokeMethod(m_commWorker, "openWiFi", Qt::QueuedConnection,
        Q_ARG(WiFiSubMode, static_cast<WiFiSubMode>(m_wifiPanel->wifiSubMode())),
        Q_ARG(QString, m_wifiPanel->ip()), Q_ARG(quint16, m_wifiPanel->port()));
}

void A::onWiFiNetworkCloseRequested()
{ QMetaObject::invokeMethod(m_commWorker, "closeWiFi", Qt::QueuedConnection); }

void A::onWiFiAtCmdRequested(const QString &c)
{ QMetaObject::invokeMethod(m_commWorker, "sendWiFiAtCmd", Qt::QueuedConnection, Q_ARG(QString, c)); }

// ═══════════════════════════════════════════════════════════════
//  onCommConnected —— 连接结果回调（来自 CommWorker 子线程的信号）
//
//  m_connected 状态管理：
//   - true: 更新状态标签为"已连接"（绿色），禁用配置面板控件，
//           点亮串口/WiFi LED，启用数据面板发送功能
//   - false: 更新状态标签为"连接失败"（红色），在串口/WiFi面板
//            显示失败状态信息
// ═══════════════════════════════════════════════════════════════
void A::onCommConnected(bool ok, const QString &msg)
{
    m_connected = ok;                               // 核心状态标志位
    if (ok) {
        m_modeStatusLabel->setText(QStringLiteral("● 已连接"));
        m_modeStatusLabel->setStyleSheet("background:transparent; font-weight:bold; color:#4caf50; font-size:14px;");
        m_bottomSignal->setText(m_currentMode == 0
            ? QStringLiteral("信号: ▂▄▆█ 100%")
            : QStringLiteral("信号: ▂▄▆█ 85%"));
        // 连接成功后锁定配置面板，防止误操作
        if (m_currentMode == 0) m_serialPanel->setControlsEnabled(false);
        else m_wifiPanel->setControlsEnabled(false);
        m_dataPanel->setConnected(true);
        m_visualPanel->setLedState(VisualPanel::LedSerial, true, "#4caf50");
        m_visualPanel->setLedState(VisualPanel::LedWiFi, true, "#4caf50");
    } else {
        m_modeStatusLabel->setText(QStringLiteral("● 连接失败"));
        m_modeStatusLabel->setStyleSheet("background:transparent; font-weight:bold; color:#ff6b6b; font-size:14px;");
        if (m_currentMode == 0)
            m_serialPanel->setConnectionStatus(QStringLiteral("●  连接失败"), "#ff6b6b");
        else
            m_wifiPanel->setConnectionStatus(QStringLiteral("●  连接失败"), "#ff6b6b");
    }
}

// ═══════════════════════════════════════════════════════════════
//  onCommDisconnected —— 断开回调
//
//  恢复所有 UI 控件到"未连接"状态：
//  - m_connected = false
//  - 解锁配置面板控件
//  - 熄灭所有 LED
//  - 数据面板设为未连接状态（禁用发送按钮等）
// ═══════════════════════════════════════════════════════════════
void A::onCommDisconnected()
{
    m_connected = false;
    m_modeStatusLabel->setText(QStringLiteral("● 未连接"));
    m_modeStatusLabel->setStyleSheet("background:transparent; font-weight:bold; color:#ff6b6b; font-size:14px;");
    m_bottomSignal->setText(QStringLiteral("信号: ---"));
    m_serialPanel->setControlsEnabled(true);        // 解锁串口面板
    m_wifiPanel->setControlsEnabled(true);          // 解锁WiFi面板
    m_dataPanel->setConnected(false);
    m_visualPanel->setLedState(VisualPanel::LedSerial, false);
    m_visualPanel->setLedState(VisualPanel::LedWiFi, false);
    m_visualPanel->setLedState(VisualPanel::LedData, false);
    m_lastFrameTimer.invalidate();
    m_visualPanel->setGaugeValue(0.0, 0.0, 50.0);
}

// ═══════════════════════════════════════════════════════════════
//  onCommDataReceived —— 数据接收处理（核心数据处理管线）
//
//  处理步骤：
//   1. 累加接收字节数，追加原始数据到 DataPanel
//   2. 数据追加到行缓冲区 m_rxBuffer
//   3. 按 \n 分割完整行，逐行解析（防止分片导致字段错乱）
//   4. 调用 parseAndUpdateRealtime 解析 → 仅死区通过时推送波形
//   5. 首字节灰度值映射到仪表盘
//   6. 闪烁数据 LED 指示灯
// ═══════════════════════════════════════════════════════════════
void A::onCommDataReceived(const QByteArray &data)
{
    m_rxBytes += data.size();
    m_dataPanel->appendReceived(data);

    // ── 行缓冲：累积，支持 \r\n / \r / \n 作为行尾 ──
    m_rxBuffer.append(data);
    bool hasNewData = false;

    while (true) {
        // 同时查找 \r 和 \n，取最先出现的
        int idxR = m_rxBuffer.indexOf('\r');
        int idxN = m_rxBuffer.indexOf('\n');
        int idx  = -1;
        int skip = 1;

        if (idxR >= 0 && idxN >= 0) {
            idx = qMin(idxR, idxN);
        } else if (idxR >= 0) {
            idx = idxR;
        } else if (idxN >= 0) {
            idx = idxN;
        } else {
            break;                                  // 没有行尾，等待下一片
        }

        // 检测 \r\n：跳过两个字符
        if (m_rxBuffer.size() > idx + 1 &&
            m_rxBuffer.at(idx) == '\r' && m_rxBuffer.at(idx + 1) == '\n') {
            skip = 2;
        }

        QByteArray line = m_rxBuffer.left(idx).trimmed();
        m_rxBuffer.remove(0, idx + skip);

        if (line.isEmpty()) continue;

        // 计算发送速率 (Hz) = 1000 / 帧间隔(ms)
        if (m_lastFrameTimer.isValid()) {
            double ms = m_lastFrameTimer.elapsed();
            if (ms > 0)
                m_visualPanel->setGaugeValue(1000.0 / ms, 0.0, 50.0);
        }
        m_lastFrameTimer.start();

        QString text = QString::fromUtf8(line);
        m_realtimePanel->appendLog(
            QString("[%1] %2").arg(QTime::currentTime().toString("hh:mm:ss.zzz"), text));

        if (parseAndUpdateRealtime(text))
            hasNewData = true;
    }

    // 仅当完整行解析通过死区时才推送波形（避免分片产生的不完整数据点）
    if (hasNewData) {
        m_wavePanel->addDataBatch(m_curVin, m_curVout, m_curIin, m_curIout,
                                  m_curTemp, m_curHumi);
    }

    // 数据LED闪烁指示
    m_visualPanel->setLedState(VisualPanel::LedData, true, "#ffaa00");
    QTimer::singleShot(500, this, [this](){
        if (m_connected) m_visualPanel->setLedState(VisualPanel::LedData, false);
    });
}

// ═══════════════════════════════════════════════════════════════
//  onCommError —— 通讯错误日志
// ═══════════════════════════════════════════════════════════════
void A::onCommError(const QString &e)
{ m_realtimePanel->appendLog(QString("[ERROR] %1").arg(e)); }

// ═══════════════════════════════════════════════════════════════
//  parseAndUpdateRealtime —— 协议解析引擎（核心算法）
//
//  设计思路：
//    下位机可能以两种格式之一上报数据，本函数依次尝试：
//
//  【路径1】Key-Value 格式（优先）
//    正则:  (Vin|Vout|Iin|Iout|T|H)\s*[:=]\s*([\d.]+)
//    匹配示例:
//      "Vin:12.05 Vout=5.01 Iin:1.02 Iout=0.51 T:25.0 H:58.0"
//    优点：键值明确，顺序无关，可部分匹配（只上报有变化的字段）
//
//  【路径2】纯数字回退
//    正则:  \b(\d+\.?\d*)\b
//    从文本中提取所有数字，要求至少6个，
//    按固定顺序映射: [Vin, Vout, Iin, Iout, Temp, Humi]
//    适用场景：下位机以空格分隔的纯数字流上报
//    例如: "12.05 5.01 1.02 0.51 25.0 58.0"
//
//  【死区滤波 (Deadband)】
//    applyDeadband 闭包:
//    - 若 |新值 - 上一次值| < kDeadband (0.01)，忽略本次更新
//    - 否则更新 cur、last，并调用对应面板 setter
//    目的：
//      传感器在稳态时会有微小噪声（如 12.051→12.049→12.052），
//      死区滤波过滤掉这些无意义波动，大幅减少 UI 刷新开销。
//
//  返回值：
//    任一字段通过死区滤波成功更新即返回 true，
//    用于上游判断是否推送波形（避免噪声污染曲线）。
// ═══════════════════════════════════════════════════════════════
bool A::parseAndUpdateRealtime(const QString &text)
{
    static constexpr double kDeadband = 0.01;
    bool parseSuccess = false;

    // 局部 lambda：更新硬件状态，仅当超出“死区”时才刷 UI 控件
    auto updateField = [](double &cur, double &last, double newVal,
                          const std::function<void(double)>& uiSetter) {
        cur = newVal; // 无论如何都保存最新的值，用于波形图绘制
        if (qAbs(newVal - last) >= kDeadband) {
            last = newVal;
            uiSetter(newVal); // 超出死区才触发耗时的 UI 重绘
        }
    };

    // ──────── 路径1：Key-Value 格式解析 ────────
    static QRegularExpression kvRx(R"((Vin|Vout|Iin|Iout|T|H)\s*[:=]\s*([\d.]+))",
                                   QRegularExpression::CaseInsensitiveOption);
    auto it = kvRx.globalMatch(text);
    bool kvMatched = false;
    while (it.hasNext()) {
        auto m = it.next();
        QString key = m.captured(1).toLower();
        double val  = m.captured(2).toDouble();
        kvMatched = true;
        parseSuccess = true;

        if (key == "vin")       updateField(m_curVin,  m_lastVin,  val, [this](double v){ m_realtimePanel->setInputVoltage(v); });
        else if (key == "vout") updateField(m_curVout, m_lastVout, val, [this](double v){ m_realtimePanel->setOutputVoltage(v); });
        else if (key == "iin")  updateField(m_curIin,  m_lastIin,  val, [this](double v){ m_realtimePanel->setInputCurrent(v); });
        else if (key == "iout") updateField(m_curIout, m_lastIout, val, [this](double v){ m_realtimePanel->setOutputCurrent(v); });
        else if (key == "t")    updateField(m_curTemp, m_lastTemp, val, [this](double v){ m_realtimePanel->setTemperature(v); });
        else if (key == "h")    updateField(m_curHumi, m_lastHumi, val, [this](double v){ m_realtimePanel->setHumidity(v); });
    }

    if (kvMatched) return parseSuccess;

    // ──────── 路径2：纯数字回退解析 ────────
    static QRegularExpression numRx(R"(\b(-?\d+\.?\d*)\b)"); // 支持负数解析
    QList<double> nums;
    auto it2 = numRx.globalMatch(text);
    while (it2.hasNext()) nums << it2.next().captured(1).toDouble();

    if (nums.size() >= 6) {
        parseSuccess = true;
        updateField(m_curVin,  m_lastVin,  nums[0], [this](double v){ m_realtimePanel->setInputVoltage(v); });
        updateField(m_curVout, m_lastVout, nums[1], [this](double v){ m_realtimePanel->setOutputVoltage(v); });
        updateField(m_curIin,  m_lastIin,  nums[2], [this](double v){ m_realtimePanel->setInputCurrent(v); });
        updateField(m_curIout, m_lastIout, nums[3], [this](double v){ m_realtimePanel->setOutputCurrent(v); });
        updateField(m_curTemp, m_lastTemp, nums[4], [this](double v){ m_realtimePanel->setTemperature(v); });
        updateField(m_curHumi, m_lastHumi, nums[5], [this](double v){ m_realtimePanel->setHumidity(v); });
    }

    return parseSuccess;                                // 未解析到任何有效数据
}

// ═══════════════════════════════════════════════════════════════
//  onSendRequested —— 数据面板 → 跨线程发送数据
//
//  流程：累加发送字节数 → invokeMethod 到子线程 → 闪烁发送LED
// ═══════════════════════════════════════════════════════════════
void A::onSendRequested(const QByteArray &data)
{
    m_txBytes += data.size();
    QMetaObject::invokeMethod(m_commWorker, "sendData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    m_visualPanel->setLedState(VisualPanel::LedData, true, "#ffaa00");
    QTimer::singleShot(300, this, [this](){ if (m_connected) m_visualPanel->setLedState(VisualPanel::LedData, false); });
}

// ═══════════════════════════════════════════════════════════════
//  toggleWaveView —— 切换波形全屏视图
//
//  QStackedWidget 页面切换：
//   index 0 = 主界面, index 1 = 波形全屏
//  全屏时隐藏模式栏和底部栏，最大化波形可视面积
// ═══════════════════════════════════════════════════════════════
void A::toggleWaveView()
{
    bool showWave = (m_contentStack->currentIndex() == 0);
    m_contentStack->setCurrentIndex(showWave ? 1 : 0);
    m_waveAct->setChecked(showWave);                // 同步菜单勾选状态
    if (m_topBar) m_topBar->setVisible(!showWave);  // 全屏时隐藏模式栏
    if (m_botBar) m_botBar->setVisible(!showWave);  // 全屏时隐藏底部栏
    if (showWave) m_wavePanel->setFocus();          // 全屏时聚焦波形面板
}

// ═══════════════════════════════════════════════════════════════
//  updateClock —— 每秒更新底部栏日期和时间
// ═══════════════════════════════════════════════════════════════
void A::updateClock()
{
    auto n = QDateTime::currentDateTime();
    m_bottomDate->setText(n.toString("yyyy-MM-dd"));
    m_bottomTime->setText(n.toString("HH:mm:ss"));
}

// ═══════════════════════════════════════════════════════════════
//  onSaveLog —— 保存接收记录到文件
// ═══════════════════════════════════════════════════════════════
void A::onSaveLog()
{
    QString f = QFileDialog::getSaveFileName(this, QStringLiteral("保存接收记录"),
        QStringLiteral("recv_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        QStringLiteral("文本文件 (*.txt)"));
    if (f.isEmpty()) return;
    QFile file(f);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << "=== Xiangmu ===\n"
            << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n"
            << QStringLiteral("模式: ") << (m_currentMode == 0 ? "串口" : "WiFi") << "\n\n"
            << QStringLiteral("── 接收数据 ──\n")
            << m_dataPanel->recvText() << "\n";
        file.close();
    }
}

// ═══════════════════════════════════════════════════════════════
//  onClearAll —— 清空所有面板数据与波形
// ═══════════════════════════════════════════════════════════════
void A::onClearAll()
{
    m_dataPanel->clearRecv();
    m_dataPanel->clearSend();
    m_realtimePanel->clearLog();
    m_wavePanel->clearAll();
    m_rxBytes = m_txBytes = 0;
    m_rxBuffer.clear();
}

// ═══════════════════════════════════════════════════════════════
//  onAbout —— 关于对话框
// ═══════════════════════════════════════════════════════════════
void A::onAbout()
{
    QMessageBox::about(this, QStringLiteral("关于"),
        QStringLiteral("<h3>Xiangmu 上位机 v1.0</h3><p>串口/WiFi 双模调试工具</p>"
            "<p>输入电压·输出电压·输入电流·输出电流·温度·湿度</p>"
            "<p style='color:#8a90a8;'>Qt6 C++ | 深蓝黑主题</p>"));
}