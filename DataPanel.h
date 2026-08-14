// =============================================================================
/// @file    DataPanel.h
/// @brief   数据面板 — 串口/网络通信助手的数据收发UI组件
///
/// DataPanel 提供上下分屏的接收区与发送区，支持：
///   - HEX/文本双模式显示与发送
///   - 时间戳标注
///   - 自动换行切换
///   - 定时发送
///   - 接收/发送字节计数
// =============================================================================

#pragma once

#include <QWidget>
#include <QByteArray>

// 前置声明 — 减少头文件依赖，加速编译
class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QLineEdit;
class QTimer;

// =============================================================================
/// @class DataPanel
/// @brief 数据收发面板
///
/// 布局概览：
/// ┌──────────────────────────────┐
/// │  [接收区]  QGroupBox          │
/// │  ┌────────────────────────┐  │
/// │  │ QPlainTextEdit（只读）   │  │
/// │  └────────────────────────┘  │
/// │  [HEX显示] [自动换行] [时间戳] │
/// │  [清除接收]                   │
/// ├──────────────────────────────┤
/// │  [发送区]  QGroupBox          │
/// │  ┌────────────────────────┐  │
/// │  │ QPlainTextEdit（可编辑） │  │
/// │  └────────────────────────┘  │
/// │  [HEX发送] [定时发送] [间隔]  │
/// │  [发送] [清除发送]            │
/// └──────────────────────────────┘
///
/// 信号：
///   - sendRequested(QByteArray): 当用户点击发送或定时器触发时发射
// =============================================================================
class DataPanel : public QWidget
{
    Q_OBJECT

public:
    /// @brief 构造函数，构建接收/发送双面板布局
    /// @param parent 父窗口指针（通常为主窗口或串口/网络配置面板）
    explicit DataPanel(QWidget *parent = nullptr);

    // ── 接收区操作 ──────────────────────────────────────────

    /// @brief 追加数据到接收显示区（来自串口/网络的数据到达时调用）
    /// @param data 接收到的原始字节
    /// @note 根据 m_hexRecvCheck / m_timestampCheck 决定显示格式
    void appendReceived(const QByteArray &data);

    /// @brief 追加已发送数据到发送区（预留接口，当前为空实现）
    /// @param data 已发送的原始字节
    void appendSent(const QByteArray &data);

    /// @brief 清空接收区文本并重置接收字节计数
    void clearRecv();

    /// @brief 清空发送区文本
    void clearSend();

    /// @brief 获取接收区当前纯文本内容
    /// @return 接收区 QPlainTextEdit 中的全部文本
    QString recvText() const;

    /// @brief 根据连接状态启用/禁用发送相关控件
    /// @param connected true 表示已连接（启用发送），false 表示未连接（禁用发送）
    void setConnected(bool connected);

signals:
    /// @brief 用户请求发送数据
    /// @param data 待发送的字节数组（已根据 HEX 选项解析）
    /// @note 由 MainWindow 或通信层连接此信号以执行实际发送
    void sendRequested(const QByteArray &data);

private slots:
    /// @brief 发送按钮点击槽 — 解析文本并发射 sendRequested 信号
    void onSendClicked();

    /// @brief 定时发送复选框切换槽 — 启动/停止定时器
    /// @param checked true=开启定时发送，false=关闭
    void onTimedSendToggled(bool checked);

private:
    /// @brief 将字节数组格式化为大写十六进制字符串（每字节间空格分隔）
    /// @param data 原始字节数组
    /// @return 格式化后的 HEX 字符串，如 "48 65 6C 6C 6F "
    QString formatHex(const QByteArray &data) const;

    // ═══════════════════════════════════════════════════════
    //  接收区控件
    // ═══════════════════════════════════════════════════════

    QPlainTextEdit *m_recvEdit    = nullptr;   ///< 接收显示框（只读，最多保留5000行）
    QCheckBox      *m_hexRecvCheck  = nullptr; ///< "HEX显示" — 勾选后以十六进制格式显示接收数据
    QCheckBox      *m_autoNewlineCheck = nullptr; ///< "自动换行" — 勾选后接收区按窗口宽度自动换行
    QCheckBox      *m_timestampCheck  = nullptr; ///< "时间戳" — 勾选后每条接收数据前追加 hh:mm:ss.zzz 时间戳
    QPushButton    *m_clearRecvBtn = nullptr;   ///< "清除接收" 按钮

    // ═══════════════════════════════════════════════════════
    //  发送区控件
    // ═══════════════════════════════════════════════════════

    QPlainTextEdit *m_sendEdit    = nullptr;   ///< 发送编辑框（用户在此输入待发送文本）
    QCheckBox      *m_hexSendCheck  = nullptr; ///< "HEX发送" — 勾选后将发送区文本按十六进制解析后发送
    QCheckBox      *m_timedSendCheck = nullptr; ///< "定时发送" — 勾选后按指定间隔自动重复发送
    QLineEdit      *m_timedIntervalEdit = nullptr; ///< 定时发送间隔输入框（单位：毫秒，默认1000）
    QPushButton    *m_sendBtn     = nullptr;   ///< "发送" 按钮 — 单击发送一次
    QPushButton    *m_clearSendBtn = nullptr;  ///< "清除发送" 按钮

    // ═══════════════════════════════════════════════════════
    //  定时器
    // ═══════════════════════════════════════════════════════

    QTimer         *m_timedTimer  = nullptr;   ///< 定时发送定时器 — 超时即触发 onSendClicked()

    // ═══════════════════════════════════════════════════════
    //  统计
    // ═══════════════════════════════════════════════════════

    quint64         m_rxBytes = 0;             ///< 接收字节累计计数
    quint64         m_txBytes = 0;             ///< 发送字节累计计数
};
