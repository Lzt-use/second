/// @file    WiFiCfgPanel.cpp
/// @brief   WiFiCfgPanel 实现 —— 以及内部使用的 WiFiCardFrame 卡片控件。

#include "WiFiCfgPanel.h"

#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>
#include <QIntValidator>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QFrame>
#include <QPainter>

// ═══════════════════════════════════════════════════════════════
//  WiFiCardFrame —— 带标题描边的深色卡片容器
// ═══════════════════════════════════════════════════════════════

/// @class WiFiCardFrame
/// @brief 内部使用的卡片式容器控件，继承自 QFrame。
///
/// 自带深色圆角背景、顶部渐变装饰线和标题文字的自绘逻辑。
/// 用于包裹 WiFiCfgPanel 的整个 UI，使其呈现统一的卡片风格。
class WiFiCardFrame : public QFrame {
public:
    /// 构造卡片，指定标题文字。
    /// @param title  顶部标题（例如 "WiFi 通讯配置"）
    /// @param parent 父控件
    explicit WiFiCardFrame(const QString &title, QWidget *parent = nullptr)
        : QFrame(parent), m_title(title) {
        // 深色背景 + 圆角边框的基础样式
        setStyleSheet("QFrame { background-color:#1e2232; border:1px solid #2a3050;"
                      " border-radius:8px; }");
    }

protected:
    /// 自绘事件：绘制背景、圆角边框、顶部渐变线和标题文字。
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 深色填充背景
        p.fillRect(rect(), QColor("#1e2232"));

        // 圆角描边（内缩 1px 避免圆角被裁切）
        p.setPen(QPen(QColor("#2a3050"), 1));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

        // 顶部渐变装饰线（蓝色 → 边框色）
        QLinearGradient g(0, 0, width(), 0);
        g.setColorAt(0, QColor("#5078d0"));
        g.setColorAt(1, QColor("#2a3050"));
        p.setPen(QPen(g, 2));
        p.drawLine(12, 0, width() - 12, 0);

        // 标题文字
        p.setPen(QColor("#7eb8ff"));
        p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
        p.drawText(16, 22, m_title);
    }

private:
    QString m_title;  ///< 卡片标题文字
};

// ═══════════════════════════════════════════════════════════════
//  WiFiCfgPanel 构造 —— 构建完整的卡片式配置 UI
// ═══════════════════════════════════════════════════════════════

/// 构造 WiFi 配置面板，分为三个功能行和底部状态区：
///   1. STA/AP 工作模式 + SSID + 密码 + 连接/断开
///   2. TCP/UDP 通讯模式 + IP + 端口 + 打开/关闭
///   3. AT 指令输入 + 发送
///   4. 底部状态标签
WiFiCfgPanel::WiFiCfgPanel(QWidget *parent)
    : QWidget(parent)
{
    // ── 创建外层卡片 ──
    auto *card = new WiFiCardFrame(QStringLiteral("WiFi 通讯配置"), this);
    auto *vlay = new QVBoxLayout(card);
    vlay->setSpacing(10);
    vlay->setContentsMargins(14, 32, 14, 10);

    // ═══════════════════════════════════════════════════════════
    //  行1: STA/AP 工作模式区 —— 工作模式 + SSID + 密码 + 连接/断开按钮
    // ═══════════════════════════════════════════════════════════
    auto *row1 = new QHBoxLayout;
    row1->setSpacing(8);

    auto *modeLbl = new QLabel(QStringLiteral("工作模式"));
    modeLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
    row1->addWidget(modeLbl);

    m_staRadio = new QRadioButton(QStringLiteral("STA"));
    m_apRadio  = new QRadioButton(QStringLiteral("AP"));
    m_staRadio->setChecked(true);  // 默认 STA 模式
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_staRadio);
    modeGroup->addButton(m_apRadio);
    row1->addWidget(m_staRadio);
    row1->addWidget(m_apRadio);

    row1->addSpacing(10);
    auto *ssidLbl = new QLabel(QStringLiteral("SSID"));
    ssidLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
    row1->addWidget(ssidLbl);
    m_ssidEdit = new QLineEdit;
    m_ssidEdit->setMaximumWidth(140);
    m_ssidEdit->setPlaceholderText(QStringLiteral("WiFi名称"));
    row1->addWidget(m_ssidEdit);

    auto *pwdLbl = new QLabel(QStringLiteral("密码"));
    pwdLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
    row1->addWidget(pwdLbl);
    m_pwdEdit = new QLineEdit;
    m_pwdEdit->setEchoMode(QLineEdit::Password);  // 密码掩码显示
    m_pwdEdit->setMaximumWidth(120);
    m_pwdEdit->setPlaceholderText(QStringLiteral("WiFi密码"));
    row1->addWidget(m_pwdEdit);

    // "连接"按钮 —— 绿色
    m_wifiConnBtn = new QPushButton(QStringLiteral("连接"));
    m_wifiConnBtn->setStyleSheet(
        "QPushButton { background-color:#3a8040; border-radius:4px; color:#fff;"
        " font-size:12px; font-weight:bold; padding:5px 14px; }"
        "QPushButton:hover { background-color:#4aa050; }"
    );
    // "断开"按钮 —— 红色
    m_wifiDiscBtn = new QPushButton(QStringLiteral("断开"));
    m_wifiDiscBtn->setStyleSheet(
        "QPushButton { background-color:#a04040; border-radius:4px; color:#fff;"
        " font-size:12px; font-weight:bold; padding:5px 14px; }"
        "QPushButton:hover { background-color:#c05050; }"
    );
    row1->addWidget(m_wifiConnBtn);
    row1->addWidget(m_wifiDiscBtn);
    row1->addStretch();
    vlay->addLayout(row1);

    // ── 分隔线 ──
    auto *sep1 = new QFrame;
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("QFrame { color:#2a3050; max-height:1px; }");
    vlay->addWidget(sep1);

    // ═══════════════════════════════════════════════════════════
    //  行2: TCP/UDP 通讯模式区 —— 通讯模式 + IP + 端口 + 打开/关闭按钮
    // ═══════════════════════════════════════════════════════════
    auto *row2 = new QHBoxLayout;
    row2->setSpacing(8);

    auto *netLbl = new QLabel(QStringLiteral("通讯模式"));
    netLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
    row2->addWidget(netLbl);

    m_tcpServerRadio = new QRadioButton(QStringLiteral("TCP Server"));
    m_tcpClientRadio = new QRadioButton(QStringLiteral("TCP Client"));
    m_udpRadio       = new QRadioButton(QStringLiteral("UDP"));
    m_tcpServerRadio->setChecked(true);  // 默认 TCP Server
    auto *netGroup = new QButtonGroup(this);
    netGroup->addButton(m_tcpServerRadio);
    netGroup->addButton(m_tcpClientRadio);
    netGroup->addButton(m_udpRadio);
    row2->addWidget(m_tcpServerRadio);
    row2->addWidget(m_tcpClientRadio);
    row2->addWidget(m_udpRadio);

    row2->addSpacing(10);
    auto *ipLbl = new QLabel(QStringLiteral("IP 地址"));
    ipLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
    row2->addWidget(ipLbl);
    m_ipEdit = new QLineEdit;
    m_ipEdit->setMaximumWidth(130);
    m_ipEdit->setPlaceholderText(QStringLiteral("192.168.1.100"));
    m_ipEdit->setStyleSheet("font-family:Consolas;");  // 等宽字体便于输入 IP
    row2->addWidget(m_ipEdit);

    auto *portLbl = new QLabel(QStringLiteral("端口"));
    portLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
    row2->addWidget(portLbl);
    m_portEdit = new QLineEdit;
    m_portEdit->setText("8080");  // 默认端口 8080
    m_portEdit->setValidator(new QIntValidator(1, 65535, this));  // 限制 1–65535 整数
    m_portEdit->setMaximumWidth(68);
    m_portEdit->setAlignment(Qt::AlignCenter);
    m_portEdit->setStyleSheet("font-family:Consolas;");
    row2->addWidget(m_portEdit);

    // "打开"按钮 —— 绿色
    m_netOpenBtn  = new QPushButton(QStringLiteral("打开"));
    m_netOpenBtn->setStyleSheet(
        "QPushButton { background-color:#3a8040; border-radius:4px; color:#fff;"
        " font-size:12px; font-weight:bold; padding:5px 14px; }"
        "QPushButton:hover { background-color:#4aa050; }"
    );
    // "关闭"按钮 —— 红色
    m_netCloseBtn = new QPushButton(QStringLiteral("关闭"));
    m_netCloseBtn->setStyleSheet(
        "QPushButton { background-color:#a04040; border-radius:4px; color:#fff;"
        " font-size:12px; font-weight:bold; padding:5px 14px; }"
        "QPushButton:hover { background-color:#c05050; }"
    );
    row2->addWidget(m_netOpenBtn);
    row2->addWidget(m_netCloseBtn);
    row2->addStretch();
    vlay->addLayout(row2);

    // ── 分隔线 ──
    auto *sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("QFrame { color:#2a3050; max-height:1px; }");
    vlay->addWidget(sep2);

    // ═══════════════════════════════════════════════════════════
    //  行3: AT 指令区 —— 指令输入 + 发送按钮
    // ═══════════════════════════════════════════════════════════
    auto *row3 = new QHBoxLayout;
    row3->setSpacing(8);
    auto *atLbl = new QLabel(QStringLiteral("AT 指令"));
    atLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
    row3->addWidget(atLbl);
    m_atCmdEdit = new QLineEdit;
    m_atCmdEdit->setPlaceholderText(QStringLiteral("例如: AT+CWMODE=1"));
    m_atCmdEdit->setStyleSheet("font-family:Consolas;");
    row3->addWidget(m_atCmdEdit);

    m_atSendBtn = new QPushButton(QStringLiteral("发送"));
    m_atSendBtn->setFixedWidth(64);
    m_atSendBtn->setStyleSheet(
        "QPushButton { background-color:#3b5998; border-radius:4px; color:#fff;"
        " font-size:12px; font-weight:bold; padding:5px 0; }"
        "QPushButton:hover { background-color:#4c6ab0; }"
    );
    row3->addWidget(m_atSendBtn);
    row3->addStretch();
    vlay->addLayout(row3);

    // ═══════════════════════════════════════════════════════════
    //  底部: 连接状态标签
    // ═══════════════════════════════════════════════════════════
    m_statusLabel = new QLabel(QStringLiteral("●  未连接"));
    m_statusLabel->setStyleSheet("background:transparent; color:#ff6b6b; font-size:12px; font-weight:bold;");
    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();
    vlay->addLayout(statusRow);

    // ── 将卡片放入外层布局 ──
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 0, 4, 0);
    outer->addWidget(card);

    // ── 信号绑定：按钮点击 → 面板信号 ──
    connect(m_wifiConnBtn, &QPushButton::clicked, this, &WiFiCfgPanel::connectWiFiRequested);
    connect(m_wifiDiscBtn, &QPushButton::clicked, this, &WiFiCfgPanel::disconnectWiFiRequested);
    connect(m_netOpenBtn,  &QPushButton::clicked, this, &WiFiCfgPanel::openNetworkRequested);
    connect(m_netCloseBtn, &QPushButton::clicked, this, &WiFiCfgPanel::closeNetworkRequested);
    // AT 指令发送：读取输入框内容并通过信号发出，随后清空输入框
    connect(m_atSendBtn, &QPushButton::clicked, this, [this](){
        emit sendAtCmdRequested(m_atCmdEdit->text());
        m_atCmdEdit->clear();
    });
}

// ═══════════════════════════════════════════════════════════════
//  Accessor —— 只读属性访问
// ═══════════════════════════════════════════════════════════════

/// 判断当前是否为 STA 模式。
/// @return true 表示 STA（客户端），false 表示 AP（热点）。
bool    WiFiCfgPanel::isStaMode()  const { return m_staRadio->isChecked(); }

/// @return 用户输入的 SSID 文本。
QString WiFiCfgPanel::ssid()       const { return m_ssidEdit->text(); }

/// @return 用户输入的 WiFi 密码文本。
QString WiFiCfgPanel::password()   const { return m_pwdEdit->text(); }

/// 获取通讯子模式。
/// 检查顺序: TCP Client → UDP → 默认 TCP Server。
/// @return 0 = TCP Server, 1 = TCP Client, 2 = UDP
int WiFiCfgPanel::wifiSubMode() const {
    if (m_tcpClientRadio->isChecked()) return 1;  // TCP Client
    if (m_udpRadio->isChecked())       return 2;  // UDP
    return 0;  // TCP Server（默认值）
}

/// @return IP 地址输入框文本。
QString WiFiCfgPanel::ip()     const { return m_ipEdit->text(); }

/// @return 端口号（0–65535 范围内的 quint16）。
quint16 WiFiCfgPanel::port()   const { return static_cast<quint16>(m_portEdit->text().toUInt()); }

/// @return AT 指令输入框文本。
QString WiFiCfgPanel::atCmd()  const { return m_atCmdEdit->text(); }

// ═══════════════════════════════════════════════════════════════
//  控件状态管理
// ═══════════════════════════════════════════════════════════════

/// 更新底部连接状态标签。
/// @param text  状态文字（例如 "● 已连接"）
/// @param color CSS 颜色值（例如 "#4caf50" 绿色 / "#ff6b6b" 红色）
void WiFiCfgPanel::setConnectionStatus(const QString &text, const QString &color)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(
        QString("background:transparent; color:%1; font-size:12px; font-weight:bold;").arg(color));
}

/// 批量启用或禁用面板中的主要交互控件。
/// 当 enabled 为 true 时复位状态为"未连接"（红色）；
/// 当 enabled 为 false 时显示"已连接"（绿色，用于表示处于活跃连接中）。
/// @param enabled true 启用控件，false 禁用控件
void WiFiCfgPanel::setControlsEnabled(bool enabled)
{
    m_ssidEdit->setEnabled(enabled);
    m_pwdEdit->setEnabled(enabled);
    m_wifiConnBtn->setEnabled(enabled);
    m_ipEdit->setEnabled(enabled);
    m_portEdit->setEnabled(enabled);
    m_netOpenBtn->setEnabled(enabled);
    m_atCmdEdit->setEnabled(enabled);
    m_atSendBtn->setEnabled(enabled);

    if (enabled)
        setConnectionStatus(QStringLiteral("●  未连接"), "#ff6b6b");
    else
        setConnectionStatus(QStringLiteral("●  已连接"), "#4caf50");
}
