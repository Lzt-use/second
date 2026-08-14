/// @file    SerialCfgPanel.cpp
/// @brief   串口参数配置面板实现 —— 包含自定义卡片容器 CardFrame 与
///          SerialCfgPanel 面板的全部 UI 布局、控件初始化及交互逻辑。
/// @author  自动生成
/// @date    2025

#include "SerialCfgPanel.h"

#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QSerialPortInfo>
#include <QPainter>

// =========================================================================
//  CardFrame —— 带标题线的圆角卡片容器
// =========================================================================

/// @class CardFrame
/// @brief 自定义 QFrame 子类，提供带顶部渐变装饰线和标题文字的圆角卡片外观。
///
/// 在 paintEvent 中自绘背景、边框、顶部渐变线及标题，
/// 配合 QSS 的 background-color / border / border-radius 作为基础样式。
class CardFrame : public QFrame {
public:
    /// @brief 构造卡片容器。
    /// @param title  卡片顶部显示的标题文字。
    /// @param parent 父级控件。
    explicit CardFrame(const QString &title, QWidget *parent = nullptr)
        : QFrame(parent), m_title(title)
    {
        // QSS 设置深色背景、浅色边框及圆角
        setStyleSheet("QFrame { background-color:#1e2232; border:1px solid #2a3050;"
                      " border-radius:8px; }");
    }

protected:
    /// @brief 自绘事件：绘制卡片背景、圆角边框、顶部渐变装饰线及标题文字。
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);  // 抗锯齿，使圆角/渐变线更平滑

        // 1. 填充深色背景
        p.fillRect(rect(), QColor("#1e2232"));

        // 2. 绘制圆角矩形边框（内缩 1px，避免与控件边缘重叠）
        p.setPen(QPen(QColor("#2a3050"), 1));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

        // 3. 顶部渐变装饰线：从蓝色渐变到边框色，宽度留出 12px 边距
        QLinearGradient g(0, 0, width(), 0);      // 水平方向线性渐变
        g.setColorAt(0.0, QColor("#5078d0"));      // 起点：亮蓝
        g.setColorAt(1.0, QColor("#2a3050"));      // 终点：边框色（融入背景）
        p.setPen(QPen(g, 2));                       // 2px 宽的渐变画笔
        p.drawLine(12, 0, width() - 12, 0);         // 顶部水平线，左右各留 12px

        // 4. 标题文字：亮蓝色、微软雅黑 10pt 加粗，位于卡片左上区域
        p.setPen(QColor("#7eb8ff"));
        p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
        p.drawText(16, 22, m_title);                // (16,22) 为文字基线参考点
    }

private:
    QString m_title;  ///< 卡片标题文字。
};

// =========================================================================
//  SerialCfgPanel 实现
// =========================================================================

/// @brief 构造函数：构建完整 UI 布局并初始化各控件。
///
/// 布局层级：
///   SerialCfgPanel (QWidget)
///     └── outer (QVBoxLayout, margins=4)
///           └── card (CardFrame, "串口参数配置")
///                 └── vlay (QVBoxLayout, margins=14,32,14,10)
///                       ├── portRow (QHBoxLayout)
///                       │     ├── 标签 "COM 端口"
///                       │     ├── m_portCombo  (串口下拉框)
///                       │     ├── m_refreshBtn ("⟳ 刷新")
///                       │     └── stretch
///                       ├── paramGrid (QGridLayout)
///                       │     ├── (0,0) 标签 "波特率"     (0,1) m_baudCombo
///                       │     ├── (0,2) 标签 "数据位"     (0,3) m_dataCombo
///                       │     ├── (1,0) 标签 "校验位"     (1,1) m_parityCombo
///                       │     └── (1,2) 标签 "停止位"     (1,3) m_stopCombo
///                       └── actRow (QHBoxLayout)
///                             ├── m_openBtn    ("打开串口")
///                             ├── m_statusLabel("●  串口未连接")
///                             └── stretch
SerialCfgPanel::SerialCfgPanel(QWidget *parent)
    : QWidget(parent)
{
    // ── 创建外层卡片容器 ──
    auto *card = new CardFrame(QStringLiteral("串口参数配置"), this);
    auto *vlay = new QVBoxLayout(card);
    vlay->setSpacing(8);
    vlay->setContentsMargins(14, 32, 14, 10);  // 上边距 32px 给标题留空间

    // ── 端口行：COM 下拉框 + 刷新按钮 ──
    auto *portRow = new QHBoxLayout;

    auto *portLbl = new QLabel(QStringLiteral("COM 端口"));
    portLbl->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;"
                           "min-width:56px;");
    portRow->addWidget(portLbl);

    m_portCombo = new QComboBox;
    m_portCombo->setMinimumWidth(210);
    m_portCombo->setToolTip(QStringLiteral("选择串口号"));
    portRow->addWidget(m_portCombo);

    m_refreshBtn = new QPushButton(QStringLiteral("⟳ 刷新"));
    m_refreshBtn->setFixedWidth(72);
    portRow->addWidget(m_refreshBtn);

    portRow->addStretch();               // 弹性空间：将控件推向左侧
    vlay->addLayout(portRow);

    // ── 参数行：4 组标签+下拉框，使用网格布局 (2行×4列) ──
    auto *paramGrid = new QGridLayout;
    paramGrid->setSpacing(8);
    paramGrid->setColumnMinimumWidth(1, 100);  // 保证下拉框有最小宽度

    // Lambda：创建统一样式的参数标签
    auto mkLabel = [](const QString &t) {
        auto *l = new QLabel(t);
        l->setStyleSheet("background:transparent; color:#8a90a8; font-size:11px; font-weight:bold;");
        return l;
    };

    // 波特率（第0行第0-1列）
    paramGrid->addWidget(mkLabel(QStringLiteral("波特率")), 0, 0);
    m_baudCombo = new QComboBox;
    m_baudCombo->setEditable(true);       // 允许用户手动输入非标波特率
    m_baudCombo->addItems({"9600","14400","19200","38400","56000",
                           "57600","115200","128000","230400","256000",
                           "460800","921600"});
    m_baudCombo->setCurrentText("115200");
    paramGrid->addWidget(m_baudCombo, 0, 1);

    // 数据位（第0行第2-3列）
    paramGrid->addWidget(mkLabel(QStringLiteral("数据位")), 0, 2);
    m_dataCombo = new QComboBox;
    m_dataCombo->addItems({"5","6","7","8"});
    m_dataCombo->setCurrentIndex(3);      // 默认选中 "8"
    paramGrid->addWidget(m_dataCombo, 0, 3);

    // 校验位（第1行第0-1列）
    paramGrid->addWidget(mkLabel(QStringLiteral("校验位")), 1, 0);
    m_parityCombo = new QComboBox;
    m_parityCombo->addItems({QStringLiteral("None"), QStringLiteral("Even"),
                              QStringLiteral("Odd"), QStringLiteral("Mark"),
                              QStringLiteral("Space")});
    paramGrid->addWidget(m_parityCombo, 1, 1);

    // 停止位（第1行第2-3列）
    paramGrid->addWidget(mkLabel(QStringLiteral("停止位")), 1, 2);
    m_stopCombo = new QComboBox;
    m_stopCombo->addItems({"1","1.5","2"});
    paramGrid->addWidget(m_stopCombo, 1, 3);

    vlay->addLayout(paramGrid);

    // ── 底部操作行：打开/关闭按钮 + 连接状态标签 ──
    auto *actRow = new QHBoxLayout;
    actRow->setSpacing(12);

    m_openBtn = new QPushButton(QStringLiteral("打开串口"));
    m_openBtn->setObjectName("successBtn");   // 对象名供外部 QSS 选择器引用
    m_openBtn->setMinimumWidth(110);
    m_openBtn->setMinimumHeight(32);
    m_openBtn->setStyleSheet(
        "QPushButton { background-color:#3a8040; border-radius:6px; color:#fff; font-size:13px;"
        " font-weight:bold; padding:6px 24px; }"
        "QPushButton:hover { background-color:#4aa050; }"
        "QPushButton:pressed { background-color:#2a6030; }"
    );
    actRow->addWidget(m_openBtn);

    // 连接状态指示标签：红色圆点 + 文字，默认"未连接"
    m_statusLabel = new QLabel(QStringLiteral("●  串口未连接"));
    m_statusLabel->setStyleSheet("background:transparent; color:#ff6b6b; font-size:12px; font-weight:bold;");
    actRow->addWidget(m_statusLabel);

    actRow->addStretch();
    vlay->addLayout(actRow);

    // ── 外层布局：将卡片嵌入 SerialCfgPanel 自身 ──
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 0, 4, 0);  // 左右留 4px 微间距
    outer->addWidget(card);

    // ── 信号连接 ──

    // 打开/关闭按钮：根据当前按钮文本判断发射对应信号
    connect(m_openBtn, &QPushButton::clicked, this, [this](){
        if (m_openBtn->text() == QStringLiteral("打开串口")) {
            emit openRequested();            // 当前为"打开"状态 → 请求打开
        } else {
            emit closeRequested();           // 当前为"关闭"状态 → 请求关闭
        }
    });

    // 刷新按钮 → 直接调用刷新槽
    connect(m_refreshBtn, &QPushButton::clicked, this, &SerialCfgPanel::refreshPortList);

    // 构造时立即枚举一次可用串口
    refreshPortList();
}

/// @brief 更新连接状态标签的文本与颜色。
///
/// 使用 QString::arg 将 color 参数注入 QSS 模板，实现运行时颜色切换。
/// @param text  状态文本，如 "●  串口已连接"。
/// @param color CSS 颜色值，如 "#4caf50"（绿色）或 "#ff6b6b"（红色）。
void SerialCfgPanel::setConnectionStatus(const QString &text, const QString &color)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(
        QString("background:transparent; color:%1; font-size:12px; font-weight:bold;").arg(color));
}

/// @brief 刷新串口列表：枚举系统可用串口并填充下拉框。
///
/// 调用 QSerialPortInfo::availablePorts() 获取系统串口列表，
/// 每项显示格式为 "COMx - 描述信息"。
/// 若未检测到任何串口，则显示占位提示"无可用串口"。
void SerialCfgPanel::refreshPortList()
{
    m_portCombo->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto &info : infos)
        m_portCombo->addItem(info.portName() + " - " + info.description());

    // 无串口时的友好占位提示
    if (m_portCombo->count() == 0)
        m_portCombo->addItem(QStringLiteral("无可用串口"));
}

// ── 参数读取接口（内联实现） ──

/// @brief 返回端口下拉框当前文本（含描述信息的完整字符串）。
QString SerialCfgPanel::portName()    const { return m_portCombo->currentText(); }

/// @brief 返回波特率数值：将可编辑下拉框文本转为 int。
/// @note  若文本无法解析为整数，toInt() 返回 0。
int     SerialCfgPanel::baudRate()    const { return m_baudCombo->currentText().toInt(); }

/// @brief 返回数据位下拉框选中索引：0=5, 1=6, 2=7, 3=8。
int     SerialCfgPanel::dataBitsIdx() const { return m_dataCombo->currentIndex(); }

/// @brief 返回校验位下拉框选中索引：0=None, 1=Even, 2=Odd, 3=Mark, 4=Space。
int     SerialCfgPanel::parityIdx()   const { return m_parityCombo->currentIndex(); }

/// @brief 返回停止位下拉框选中索引：0=1, 1=1.5, 2=2。
int     SerialCfgPanel::stopBitsIdx() const { return m_stopCombo->currentIndex(); }

/// @brief 双态切换：启用模式（可编辑参数）与禁用模式（运行中锁定参数）。
///
/// 切换逻辑：
///   enabled = true  → "打开串口"模式：
///     - 所有参数控件启用（可编辑）
///     - 按钮显示"打开串口"，绿色背景
///     - 状态标签显示"●  串口未连接"（红色）
///   enabled = false → "关闭串口"模式：
///     - 所有参数控件禁用（不可编辑，防止运行时误改参数）
///     - 按钮显示"关闭串口"，红色背景
///     - 状态标签显示"●  串口已连接"（绿色）
///
/// @param enabled 是否启用参数编辑控件。
void SerialCfgPanel::setControlsEnabled(bool enabled)
{
    // 1) 统一控制所有参数下拉框及刷新按钮的启用状态
    m_portCombo->setEnabled(enabled);
    m_baudCombo->setEnabled(enabled);
    m_dataCombo->setEnabled(enabled);
    m_parityCombo->setEnabled(enabled);
    m_stopCombo->setEnabled(enabled);
    m_refreshBtn->setEnabled(enabled);

    // 2) 根据模式切换按钮外观与状态标签
    if (enabled) {
        // ── 未连接状态：绿色按钮 + 红色"未连接"标签 ──
        m_openBtn->setText(QStringLiteral("打开串口"));
        m_openBtn->setStyleSheet(
            "QPushButton { background-color:#3a8040; border-radius:6px; color:#fff; font-size:13px;"
            " font-weight:bold; padding:6px 24px; }"
            "QPushButton:hover { background-color:#4aa050; }"
            "QPushButton:pressed { background-color:#2a6030; }"
        );
        m_statusLabel->setText(QStringLiteral("●  串口未连接"));
        m_statusLabel->setStyleSheet("background:transparent; color:#ff6b6b; font-size:12px; font-weight:bold;");
    } else {
        // ── 已连接状态：红色按钮 + 绿色"已连接"标签 ──
        m_openBtn->setText(QStringLiteral("关闭串口"));
        m_openBtn->setStyleSheet(
            "QPushButton { background-color:#a04040; border-radius:6px; color:#fff; font-size:13px;"
            " font-weight:bold; padding:6px 24px; }"
            "QPushButton:hover { background-color:#c05050; }"
            "QPushButton:pressed { background-color:#803030; }"
        );
        m_statusLabel->setText(QStringLiteral("●  串口已连接"));
        m_statusLabel->setStyleSheet("background:transparent; color:#4caf50; font-size:12px; font-weight:bold;");
    }
}
