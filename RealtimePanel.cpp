// ============================================================================
// RealtimePanel.cpp
// 实时数据面板 — 实现文件
//
// 实现要点：
//   - 构造函数使用 QGridLayout 组装 2×3 卡片矩阵，
//     每个卡片内含名称标签 + 数值标签
//   - 六大 set 接口委托 updateLabel() 进行格式化更新
//   - 日志通过 QPlainTextEdit 的 appendPlainText/clear 操作
// ============================================================================

#include "RealtimePanel.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QFont>

// ---------------------------------------------------------------------------
// 构造函数：搭建 2×3 网格 + 日志区
// ---------------------------------------------------------------------------
RealtimePanel::RealtimePanel(QWidget *parent)
    : QWidget(parent)
{
    // 外层垂直布局：上方网格 + 下方日志
    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(3);
    outer->setContentsMargins(0, 0, 0, 0);

    // ── 2行×3列 数值网格 ──
    // 每格是一张"卡片"（带圆角边框的 QWidget），内部垂直排列名称和数值
    auto *grid = new QGridLayout;
    grid->setSpacing(6);

    // 字段描述结构：名称、标签引用、单位
    struct Field {
        QString name;
        QLabel *&label;
        QString unit;
    };
    Field fields[] = {
        {QStringLiteral("输入电压"), m_vinLabel,  QStringLiteral("V")},
        {QStringLiteral("输出电压"), m_voutLabel, QStringLiteral("V")},
        {QStringLiteral("输入电流"), m_iinLabel,  QStringLiteral("A")},
        {QStringLiteral("输出电流"), m_ioutLabel, QStringLiteral("A")},
        {QStringLiteral("温    度"), m_tempLabel, QStringLiteral("°C")},
        {QStringLiteral("湿    度"), m_humiLabel, QStringLiteral("%")},
    };

    // 遍历 6 个字段，按 row = i/3, col = i%3 填入 2×3 网格
    for (int i = 0; i < 6; ++i) {
        int row = i / 3;   // 行号：0 或 1
        int col = i % 3;   // 列号：0, 1, 2

        // 每格一个卡片容器，设置深色圆角边框样式
        auto *card = new QWidget;
        card->setStyleSheet(
            "QWidget { background-color:#141820; border:1px solid #2a3050; border-radius:6px; }"
        );
        auto *cv = new QVBoxLayout(card);
        cv->setSpacing(2);
        cv->setContentsMargins(10, 6, 10, 6);

        // 卡片顶部：参数名称（小号灰色居中文本）
        auto *nameLabel = new QLabel(fields[i].name);
        nameLabel->setStyleSheet("background:transparent; color:#6a7a9a; font-size:10px;");
        nameLabel->setAlignment(Qt::AlignCenter);
        cv->addWidget(nameLabel);

        // 卡片底部：数值标签（大号青色等宽字体居中），初始显示 "--单位"
        fields[i].label = new QLabel(QStringLiteral("--%1").arg(fields[i].unit));
        fields[i].label->setAlignment(Qt::AlignCenter);
        fields[i].label->setStyleSheet(
            "background:transparent; color:#00e5ff; font-size:20px; font-weight:bold;"
            "font-family:Consolas,'Microsoft YaHei';"
        );
        cv->addWidget(fields[i].label);

        // 将卡片放入网格的 (row, col) 位置
        grid->addWidget(card, row, col);
    }

    // ── 原始数据日志（只读，最多保留 100 行，高度受限以实现"可折叠"效果）──
    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(100);   // 自动丢弃超出 100 行的旧内容
    m_logEdit->setMaximumHeight(48);        // 收缩高度，避免挤占上方网格
    m_logEdit->setPlaceholderText(QStringLiteral("原始数据日志…"));
    m_logEdit->setStyleSheet(
        "QPlainTextEdit { background-color:#0d1117; border:1px solid #2a3050;"
        " border-radius:4px; padding:2px 6px; color:#5a6a90; font-family:Consolas; font-size:11px; }"
    );

    outer->addLayout(grid);
    outer->addWidget(m_logEdit);

    // 初始化为 0.0，确保所有标签显示合法数值
    setInputVoltage(0.0);
    setOutputVoltage(0.0);
    setInputCurrent(0.0);
    setOutputCurrent(0.0);
    setTemperature(0.0);
    setHumidity(0.0);
}

// ---------------------------------------------------------------------------
// 内部工具：格式化数值并更新到指定标签
// ---------------------------------------------------------------------------
void RealtimePanel::updateLabel(QLabel *label, double value, const QString &unit)
{
    // 格式化：保留一位小数，如 "12.3 V"
    label->setText(QString("%1 %2").arg(value, 0, 'f', 1).arg(unit));
}

// ---------------------------------------------------------------------------
// 六大参数设置接口（全部内联委托给 updateLabel）
// ---------------------------------------------------------------------------
void RealtimePanel::setInputVoltage(double v)   { updateLabel(m_vinLabel,  v, "V"); }
void RealtimePanel::setOutputVoltage(double v)  { updateLabel(m_voutLabel, v, "V"); }
void RealtimePanel::setInputCurrent(double a)   { updateLabel(m_iinLabel,  a, "A"); }
void RealtimePanel::setOutputCurrent(double a)  { updateLabel(m_ioutLabel, a, "A"); }
void RealtimePanel::setTemperature(double c)    { updateLabel(m_tempLabel, c, "°C"); }
void RealtimePanel::setHumidity(double pct)     { updateLabel(m_humiLabel, pct, "%"); }

// ---------------------------------------------------------------------------
// 日志操作
// ---------------------------------------------------------------------------

/// 追加一行日志；QPlainTextEdit::appendPlainText 会自动换行
void RealtimePanel::appendLog(const QString &line)
{
    m_logEdit->appendPlainText(line);
}

/// 清空全部日志内容
void RealtimePanel::clearLog()
{
    m_logEdit->clear();
}
