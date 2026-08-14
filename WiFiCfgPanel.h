/// @file    WiFiCfgPanel.h
/// @brief   WiFi 配置面板 —— 提供 STA/AP 工作模式、TCP/UDP 通讯模式以及 AT 指令发送的用户界面。
///
/// 该面板封装了 WiFi 网络参数（SSID、密码、IP、端口）的输入控件和状态反馈，通过 Qt 信号
/// 将用户操作（连接/断开 WiFi、打开/关闭网络、发送 AT 指令）通知上层业务逻辑。
///
/// 典型用法：
/// @code
///   auto *panel = new WiFiCfgPanel(parent);
///   connect(panel, &WiFiCfgPanel::connectWiFiRequested, this, &MyClass::onConnectWiFi);
/// @endcode

#pragma once

#include <QWidget>

class QRadioButton;
class QLineEdit;
class QPushButton;
class QLabel;
class QButtonGroup;

/// @class WiFiCfgPanel
/// @brief WiFi 通讯配置面板。
///
/// 将工作模式选择（STA/AP）、通讯模式选择（TCP Server/TCP Client/UDP）、AT 指令发送
/// 集成在同一个卡片式控件中。对外通过只读 accessor 暴露当前参数，通过信号通知操作请求。
class WiFiCfgPanel : public QWidget
{
    Q_OBJECT

public:
    /// 构造函数 —— 创建 UI 并绑定信号。
    explicit WiFiCfgPanel(QWidget *parent = nullptr);

    // ── 工作模式 ──────────────────────────────────────────────

    /// 当前是否为 STA（Station）模式。
    /// @retval true  STA 模式（客户端，连接外部 AP）
    /// @retval false AP 模式（本机作为热点）
    bool    isStaMode()   const;

    // ── WiFi 参数 ─────────────────────────────────────────────

    /// 获取用户输入的 SSID（WiFi 名称）。
    QString ssid()        const;

    /// 获取用户输入的 WiFi 密码。
    QString password()    const;

    // ── 通讯子模式 ─────────────────────────────────────────────

    /// 获取当前的通讯子模式。
    /// @return 0 = TCP Server（默认），1 = TCP Client，2 = UDP
    int     wifiSubMode() const;

    // ── 网络参数 ──────────────────────────────────────────────

    /// 获取用户输入的 IP 地址字符串。
    QString ip()          const;

    /// 获取用户输入的端口号（0–65535）。
    quint16 port()        const;

    // ── AT 指令 ───────────────────────────────────────────────

    /// 获取 AT 指令输入框中的文本。
    QString atCmd()       const;

    // ── 控件状态 ──────────────────────────────────────────────

    /// 启用/禁用面板中的交互控件。
    /// @param enabled true 则启用所有输入与按钮；false 则禁用。
    void    setControlsEnabled(bool enabled);

    /// 更新底部的连接状态标签文本与颜色。
    /// @param text  状态描述文字（例如 "● 已连接"）
    /// @param color CSS 颜色字符串（例如 "#4caf50"）
    void    setConnectionStatus(const QString &text, const QString &color);

signals:
    /// 用户点击"连接 WiFi"按钮时发射。
    void connectWiFiRequested();

    /// 用户点击"断开 WiFi"按钮时发射。
    void disconnectWiFiRequested();

    /// 用户点击"打开网络"按钮时发射。
    void openNetworkRequested();

    /// 用户点击"关闭网络"按钮时发射。
    void closeNetworkRequested();

    /// 用户点击"发送 AT 指令"按钮时发射，携带指令文本。
    /// @param cmd AT 指令字符串
    void sendAtCmdRequested(const QString &cmd);

private:
    // ═══════════════════════════════════════════════════════════
    //  STA / AP 工作模式区
    // ═══════════════════════════════════════════════════════════

    QRadioButton *m_staRadio = nullptr;  ///< STA（Station）模式单选按钮
    QRadioButton *m_apRadio  = nullptr;  ///< AP 模式单选按钮

    // ═══════════════════════════════════════════════════════════
    //  WiFi 连接参数
    // ═══════════════════════════════════════════════════════════

    QLineEdit    *m_ssidEdit     = nullptr;  ///< SSID 输入框
    QLineEdit    *m_pwdEdit      = nullptr;  ///< WiFi 密码输入框（密文显示）
    QPushButton  *m_wifiConnBtn  = nullptr;  ///< "连接"按钮
    QPushButton  *m_wifiDiscBtn  = nullptr;  ///< "断开"按钮

    // ═══════════════════════════════════════════════════════════
    //  TCP / UDP 通讯模式区
    // ═══════════════════════════════════════════════════════════

    QRadioButton *m_tcpServerRadio = nullptr;  ///< TCP Server 单选按钮
    QRadioButton *m_tcpClientRadio = nullptr;  ///< TCP Client 单选按钮
    QRadioButton *m_udpRadio       = nullptr;  ///< UDP 单选按钮

    // ═══════════════════════════════════════════════════════════
    //  IP & 端口参数
    // ═══════════════════════════════════════════════════════════

    QLineEdit    *m_ipEdit      = nullptr;  ///< IP 地址输入框
    QLineEdit    *m_portEdit    = nullptr;  ///< 端口号输入框（含 1–65535 校验）
    QPushButton  *m_netOpenBtn  = nullptr;  ///< "打开"网络按钮
    QPushButton  *m_netCloseBtn = nullptr;  ///< "关闭"网络按钮

    // ═══════════════════════════════════════════════════════════
    //  AT 指令区
    // ═══════════════════════════════════════════════════════════

    QLineEdit    *m_atCmdEdit   = nullptr;  ///< AT 指令输入框
    QPushButton  *m_atSendBtn   = nullptr;  ///< "发送" AT 指令按钮

    // ═══════════════════════════════════════════════════════════
    //  状态显示
    // ═══════════════════════════════════════════════════════════

    QLabel       *m_statusLabel = nullptr;  ///< 底部状态标签（连接/未连接）
};
