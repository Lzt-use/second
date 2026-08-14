// =============================================================================
/// @file    DataPanel.cpp
/// @brief   数据面板实现 — 详见 DataPanel.h 类注释
// =============================================================================

#include "DataPanel.h"

#include <QPlainTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>

// =============================================================================
//  构造函数 — 构建完整的接收/发送双面板布局
// =============================================================================

DataPanel::DataPanel(QWidget *parent)
    : QWidget(parent)
{
    // 使用 QSplitter 实现上下分区，支持用户拖拽调整比例
    auto *splitter = new QSplitter(Qt::Vertical, this);

    // ══════════════════════════════════════════════
    //  接收子面板 — 用于展示从串口/网络接收到的数据
    // ══════════════════════════════════════════════
    auto *recvBox = new QGroupBox(QStringLiteral("数据接收"));
    auto *recvV   = new QVBoxLayout(recvBox);
    recvV->setSpacing(4);
    recvV->setContentsMargins(8, 4, 8, 4);

    // 接收文本框：只读，最多保留5000行以防止内存无限增长
    m_recvEdit = new QPlainTextEdit;
    m_recvEdit->setReadOnly(true);
    m_recvEdit->setMaximumBlockCount(5000);
    m_recvEdit->setTabChangesFocus(true);  // Tab键切换焦点而非插入制表符
    recvV->addWidget(m_recvEdit);

    // 接收区底部工具栏
    auto *recvBtnRow = new QHBoxLayout;
    m_hexRecvCheck  = new QCheckBox(QStringLiteral("HEX显示"));
    m_autoNewlineCheck = new QCheckBox(QStringLiteral("自动换行"));
    m_autoNewlineCheck->setChecked(true);  // 默认开启自动换行
    m_timestampCheck  = new QCheckBox(QStringLiteral("时间戳"));

    recvBtnRow->addWidget(m_hexRecvCheck);
    recvBtnRow->addWidget(m_autoNewlineCheck);
    recvBtnRow->addWidget(m_timestampCheck);
    recvBtnRow->addStretch();

    m_clearRecvBtn = new QPushButton(QStringLiteral("清除接收"));
    m_clearRecvBtn->setObjectName("dangerBtn");  // 通过 QSS 标记为危险操作样式
    recvBtnRow->addWidget(m_clearRecvBtn);
    recvV->addLayout(recvBtnRow);

    // ── 自动换行切换：勾选时按窗口宽度换行，取消时禁用换行 ──
    connect(m_autoNewlineCheck, &QCheckBox::toggled, this, [this](bool on){
        m_recvEdit->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth
                                       : QPlainTextEdit::NoWrap);
    });
    connect(m_clearRecvBtn, &QPushButton::clicked, this, &DataPanel::clearRecv);

    splitter->addWidget(recvBox);

    // ══════════════════════════════════════════════
    //  发送子面板 — 用户编辑并发送数据
    // ══════════════════════════════════════════════
    auto *sendBox = new QGroupBox(QStringLiteral("数据发送"));
    auto *sendV   = new QVBoxLayout(sendBox);
    sendV->setSpacing(4);
    sendV->setContentsMargins(8, 4, 8, 4);

    // 发送文本框：可编辑，最多保留1000行
    m_sendEdit = new QPlainTextEdit;
    m_sendEdit->setMaximumBlockCount(1000);
    m_sendEdit->setTabChangesFocus(true);
    sendV->addWidget(m_sendEdit);

    // 发送区底部工具栏
    auto *sendBtnRow = new QHBoxLayout;
    m_hexSendCheck  = new QCheckBox(QStringLiteral("HEX发送"));
    m_timedSendCheck = new QCheckBox(QStringLiteral("定时发送"));

    sendBtnRow->addWidget(m_hexSendCheck);
    sendBtnRow->addWidget(m_timedSendCheck);

    sendBtnRow->addWidget(new QLabel(QStringLiteral("间隔(ms):")));
    // 设置间隔标签背景透明，避免与父容器样式冲突
    auto *lbl = qobject_cast<QLabel*>(sendBtnRow->itemAt(sendBtnRow->count()-1)->widget());
    if (lbl) lbl->setStyleSheet("background:transparent;");

    // 定时发送间隔输入框，默认1000ms，限制最小50ms
    m_timedIntervalEdit = new QLineEdit;
    m_timedIntervalEdit->setText("1000");
    m_timedIntervalEdit->setMaximumWidth(68);
    m_timedIntervalEdit->setPlaceholderText("ms");
    m_timedIntervalEdit->setAlignment(Qt::AlignCenter);
    sendBtnRow->addWidget(m_timedIntervalEdit);

    sendBtnRow->addStretch();

    m_sendBtn = new QPushButton(QStringLiteral("发送"));
    m_sendBtn->setMinimumWidth(80);
    sendBtnRow->addWidget(m_sendBtn);

    m_clearSendBtn = new QPushButton(QStringLiteral("清除发送"));
    m_clearSendBtn->setObjectName("dangerBtn");
    sendBtnRow->addWidget(m_clearSendBtn);
    sendV->addLayout(sendBtnRow);

    splitter->addWidget(sendBox);
    // 设置上下比例：接收区占3份，发送区占2份（接收区略大）
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    // 外层布局 — 将 splitter 嵌入到 DataPanel 自身的 QWidget 中
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 0, 4, 0);
    outer->addWidget(splitter);

    // ── 发送区信号连接 ──
    connect(m_sendBtn, &QPushButton::clicked, this, &DataPanel::onSendClicked);
    connect(m_clearSendBtn, &QPushButton::clicked, this, &DataPanel::clearSend);
    connect(m_timedSendCheck, &QCheckBox::toggled, this, &DataPanel::onTimedSendToggled);

    // 定时发送定时器 — 超时时自动触发一次发送
    m_timedTimer = new QTimer(this);
    connect(m_timedTimer, &QTimer::timeout, this, &DataPanel::onSendClicked);
}

// =============================================================================
//  槽函数 — 发送按钮点击 / 定时器超时触发
// =============================================================================

void DataPanel::onSendClicked()
{
    // 获取发送区原始文本
    QString text = m_sendEdit->toPlainText();
    if (text.isEmpty()) return;

    QByteArray data;

    // ── HEX 发送解析逻辑 ──────────────────────────────────
    // 当 "HEX发送" 复选框勾选时，将文本视为十六进制字符串：
    //   1. 去掉所有空格和换行符，例如 "48 65 6C 6C 6F" → "48656C6C6F"
    //   2. 调用 QByteArray::fromHex() 将每两个字符解析为一个字节
    // 否则直接按 UTF-8 编码发送文本
    if (m_hexSendCheck->isChecked()) {
        text.remove(' ');
        text.remove('\n');
        data = QByteArray::fromHex(text.toUtf8());
    } else {
        data = text.toUtf8();
    }

    // 解析结果非空时才发射信号，避免发送空数据
    if (!data.isEmpty()) {
        emit sendRequested(data);
        m_txBytes += data.size();  // 累加发送字节计数
    }
}

// =============================================================================
//  槽函数 — 定时发送开关切换
// =============================================================================

void DataPanel::onTimedSendToggled(bool checked)
{
    // ── 定时发送逻辑 ─────────────────────────────────────
    // 开启定时发送：
    //   1. 从间隔输入框读取毫秒数，最小限制为50ms（防止过于频繁）
    //   2. 启动 QTimer，按指定间隔周期性触发 onSendClicked()
    //   3. 禁用发送编辑框，防止用户在定时发送期间手动修改内容
    //
    // 关闭定时发送：
    //   1. 停止 QTimer
    //   2. 恢复发送编辑框的可编辑状态
    if (checked) {
        int ms = m_timedIntervalEdit->text().toInt();
        if (ms < 50) ms = 100;          // 保护下限：不低于50ms（实际设为100ms）
        m_timedTimer->start(ms);
        m_sendEdit->setEnabled(false);  // 定时发送期间禁止编辑
    } else {
        m_timedTimer->stop();
        m_sendEdit->setEnabled(true);
    }
}

// =============================================================================
//  公共方法 — 接收区追加数据
// =============================================================================

void DataPanel::appendReceived(const QByteArray &data)
{
    m_rxBytes += data.size();  // 累加接收字节计数

    QString text;

    // 若开启时间戳，在数据前附加当前时间，格式： [hh:mm:ss.zzz]
    if (m_timestampCheck->isChecked()) {
        text += "[" + QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + "] ";
    }

    // 根据 HEX 显示开关决定显示格式
    if (m_hexRecvCheck->isChecked()) {
        text += formatHex(data);          // 十六进制大写，空格分隔
    } else {
        text += QString::fromUtf8(data);  // 原始 UTF-8 文本
    }

    // 移动到文本末尾后插入，并再次移动光标以确保新内容可见
    m_recvEdit->moveCursor(QTextCursor::End);
    m_recvEdit->insertPlainText(text);
    m_recvEdit->moveCursor(QTextCursor::End);
}

// =============================================================================
//  公共方法 — 发送区追加已发送数据（预留，当前为空实现）
// =============================================================================

void DataPanel::appendSent(const QByteArray &data)
{
    Q_UNUSED(data);  // 未使用参数，消除编译器警告
    // TODO: 可在接收区回显已发送的数据，或单独维护发送历史
}

// =============================================================================
//  公共方法 — 清空接收区
// =============================================================================

void DataPanel::clearRecv()
{
    m_recvEdit->clear();
    m_rxBytes = 0;  // 同步重置字节计数
}

// =============================================================================
//  公共方法 — 清空发送区
// =============================================================================

void DataPanel::clearSend()
{
    m_sendEdit->clear();
}

// =============================================================================
//  公共方法 — 获取接收区文本
// =============================================================================

QString DataPanel::recvText() const
{
    return m_recvEdit->toPlainText();
}

// =============================================================================
//  公共方法 — 设置连接状态
// =============================================================================

void DataPanel::setConnected(bool connected)
{
    // 连接状态控制发送按钮和编辑框的可用性
    // 未连接时禁止发送，防止无效操作
    m_sendBtn->setEnabled(connected);
    m_sendEdit->setEnabled(connected);
}

// =============================================================================
//  私有工具方法 — 字节数组 → 十六进制字符串
// =============================================================================

QString DataPanel::formatHex(const QByteArray &data) const
{
    // QByteArray::toHex(' ') 将每个字节转换为两个十六进制字符，
    // 字节之间用空格分隔，如: 0x48 0x65 → "48 65"
    // toUpper() 统一转为大写
    // 末尾追加一个空格以便连续显示时数据块之间有间隔
    return data.toHex(' ').toUpper() + " ";
}
