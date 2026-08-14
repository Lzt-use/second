// ============================================================================
// VisualPanel.cpp
// 可视化面板 — 实现文件
//
// 实现要点：
//   1. resizeEvent 将面板纵向划分为仪表盘区（~68%）和 LED 区（下方 3 行）
//   2. drawGauge 绘制 270° 圆弧仪表盘：
//      - 起始角度 135°（左下方），逆时针扫过 270° 到 -135°（右下方）
//      - 数值比率映射为扫过角度：spanAngle = ratio × 270°
//      - 颜色阈值：ratio < 0.5 → 绿, < 0.8 → 橙, ≥ 0.8 → 红
//      - 指针：由当前角度计算 cos/sin 得到末端坐标
//   3. drawLed 使用 QRadialGradient 绘制径向渐变发光圆点 +
//      高光反光点 + 标签文字
// ============================================================================

#include "VisualPanel.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <cmath>

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
VisualPanel::VisualPanel(QWidget *parent)
    : QWidget(parent)
{
    // 设置最小高度，防止布局挤扁
    setMinimumHeight(160);

    // 初始化 3 个 LED 的标签、默认熄灭颜色和初始灭状态
    m_leds[LedSerial] = {QStringLiteral("串口状态"), "#ff6b6b", false};
    m_leds[LedData]   = {QStringLiteral("数据收发"), "#ffaa00", false};
    m_leds[LedWiFi]   = {QStringLiteral("WiFi状态"), "#ff6b6b", false};
}

// ===========================================================================
//  resizeEvent — 动态布局计算
// ===========================================================================
//  每次面板尺寸变化时，重新计算以下矩形的边界：
//    m_gaugeRect    — 仪表盘绘制区域（上方 ~68% 高度）
//    m_ledRects[0]  — 串口状态 LED
//    m_ledRects[1]  — 数据收发 LED
//    m_ledRects[2]  — WiFi 状态 LED
//
//  纵向划分逻辑：
//    ┌─────────────────────────────┐  y=4
//    │                             │
//    │       仪表盘区域             │  gaugeH = h * 0.68
//    │                             │
//    ├─────────────────────────────┤  y = gaugeH + 8
//    │  LED[0] 串口状态             │  ledH = (剩余高度) / 3
//    │  LED[1] 数据收发             │
//    │  LED[2] WiFi状态             │
//    └─────────────────────────────┘  y = h - 4
// ===========================================================================
void VisualPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    int w = width();
    int h = height();

    // 仪表盘占上半部分，高度比例 0.68，左右各留 4px 边距
    int gaugeH = h * 0.68;
    m_gaugeRect = QRect(4, 4, w - 8, gaugeH);

    // LED 区从仪表盘下方 8px 处开始，剩余高度三等分
    int ledY = gaugeH + 8;
    int ledH = (h - ledY - 4) / 3;           // 减去底部 4px 边距后三等分
    for (int i = 0; i < 3; ++i) {
        // 每个 LED 矩形左右各留 8px，上下各留 2px 间距
        m_ledRects[i] = QRect(8, ledY + i * ledH, w - 16, ledH - 4);
    }
}

// ===========================================================================
//  paintEvent — 总绘制入口
// ===========================================================================
void VisualPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);  // 抗锯齿，保证圆弧和圆形平滑

    // 1. 填充深色背景
    p.fillRect(rect(), QColor("#1a1e2c"));

    // 2. 绘制面板外边框（深蓝灰色圆角矩形）
    p.setPen(QPen(QColor("#3a4468"), 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

    // 3. 绘制仪表盘
    drawGauge(p, m_gaugeRect);

    // 4. 依次绘制 3 个 LED 指示灯
    for (int i = 0; i < 3; ++i)
        drawLed(p, i);
}

// ===========================================================================
//  drawGauge — 仪表盘绘制（圆弧 + 颜色分段 + 指针 + 数值）
// ===========================================================================
//
//  坐标系约定（Qt 绘图）：
//    - 角度以 3 点钟方向为 0°，逆时针为正
//    - drawArc 参数：(矩形, 起始角度×16, 扫过角度×16)
//    - 扫过角度为正 → 逆时针，为负 → 顺时针
//
//  本仪表盘设计：
//    - 刻度弧从 135°（左下方，约 7:30 方向）开始
//    - 逆时针扫过 270° 到达 -135°（即 225°，右下方，约 4:30 方向）
//    - 底部（-90° / 6 点钟方向）留空不画弧
//
//  角度示意图：
//                 -90° (顶部)
//                   ↑
//    135° ←─────────+──────────→ -135° (225°)
//        (起始)      |          (结束)
//                   ↓
//                 90° (底部，不画弧)
//
//  颜色分段（基于 ratio = (value - min) / (max - min)）：
//    ratio ∈ [0.0, 0.5) → 绿色 #4caf50  （安全区）
//    ratio ∈ [0.5, 0.8) → 橙色 #ff9800  （警告区）
//    ratio ∈ [0.8, 1.0] → 红色 #f44336  （危险区）
// ===========================================================================
void VisualPanel::drawGauge(QPainter &p, const QRect &r)
{
    p.save();

    // ── 仪表盘背景 ──
    p.fillRect(r, QColor("#141820"));                     // 深色填充
    p.setPen(QPen(QColor("#2a3050"), 1));                 // 边框线
    p.drawRect(r);

    // 计算圆心：水平居中，垂直方向略偏下（+4px）以留出标题空间
    int cx = r.center().x();
    int cy = r.center().y() + 4;
    int radius = qMin(r.width(), r.height()) / 2 - 14;   // 半径：取宽高较小者的一半，再缩 14px

    // 弧线外接正方形
    QRectF arcRect(cx - radius, cy - radius, radius * 2, radius * 2);

    // ── 1. 背景弧（深色，表示整个量程 270°）──
    //    起始角 135°，扫过 270°（逆时针），线宽 8，圆头端点
    p.setPen(QPen(QColor("#2a3050"), 8, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arcRect, 135 * 16, 270 * 16);             // Qt 角度单位：1/16 度

    // ── 2. 值弧（根据当前数值比率计算扫过角度并选择颜色）──
    double ratio = (m_gaugeValue - m_gaugeMin) / (m_gaugeMax - m_gaugeMin);
    ratio = qBound(0.0, ratio, 1.0);                    // 钳制在 [0, 1]
    int spanAngle = static_cast<int>(ratio * 270);      // 实际扫过角度（度）

    // 颜色分段阈值判断
    QColor arcColor;
    if (ratio < 0.5)      arcColor = QColor("#4caf50"); // 绿色：安全区
    else if (ratio < 0.8) arcColor = QColor("#ff9800"); // 橙色：警告区
    else                  arcColor = QColor("#f44336"); // 红色：危险区

    p.setPen(QPen(arcColor, 8, Qt::SolidLine, Qt::RoundCap));
    // 注意：扫过角度取负值（-spanAngle * 16），使绘制方向为顺时针
    // 这样值弧从 135° 开始顺时针生长，与仪表盘视觉习惯一致
    p.drawArc(arcRect, 135 * 16, -spanAngle * 16);

    // ── 3. 指针 ──
    // 角度计算：起始 135°，顺时针旋转 ratio×270°
    //   angle = 135° - ratio × 270°
    //   结果范围：[135° (ratio=0)] → [-135° (ratio=1)]
    double angle = 135 - ratio * 270;
    double rad   = angle * M_PI / 180.0;                 // 转为弧度供 cos/sin 使用
    int ptrLen   = radius - 18;                          // 指针长度略短于半径

    // 指针末端坐标：
    //   Qt 坐标系 y 轴向下，因此 y = cy - sin(rad) * len
    //   cos 从圆心向右为正，sin 向上为负 → y 用减号翻转
    int px = cx + static_cast<int>(ptrLen * cos(rad));
    int py = cy - static_cast<int>(ptrLen * sin(rad));

    // 绘制指针线（橙红色，线宽 2.5）
    p.setPen(QPen(QColor("#ff5722"), 2.5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(cx, cy, px, py);

    // ── 4. 圆心装饰（双层圆）──
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#1a1e2c"));                       // 外圆：面板背景色
    p.drawEllipse(QPoint(cx, cy), 10, 10);
    p.setBrush(QColor("#5078d0"));                       // 内圆：蓝色圆心
    p.drawEllipse(QPoint(cx, cy), 5, 5);

    // ── 5. 数值文本（居中显示当前值，保留一位小数）──
    p.setPen(QColor("#e8eaf0"));
    p.setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    QRect valRect(cx - 50, cy + radius - 30, 100, 24);
    p.drawText(valRect, Qt::AlignHCenter | Qt::AlignVCenter,
               QString::number(m_gaugeValue, 'f', 1));

    // ── 6. 标签文本（仪表盘名称，如"输出电压"）──
    p.setPen(QColor("#8a90a8"));
    p.setFont(QFont("Microsoft YaHei", 9));
    QRect labelRect(cx - 50, cy + radius - 8, 100, 16);
    p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter, m_gaugeLabel);

    // ── 7. 左上角标题"仪表盘"──
    p.setPen(QColor("#7eb8ff"));
    p.drawText(r.left() + 6, r.top() + 16, QStringLiteral("仪表盘"));

    p.restore();
}

// ===========================================================================
//  drawLed — LED 状态指示灯绘制（径向渐变发光效果）
// ===========================================================================
//
//  视觉结构：
//    ┌────────────────────────────────────────────┐
//    │  ◉  串口状态                               │
//    │  ← 径向渐变光晕 + 实心圆 + 高光反光点       │
//    └────────────────────────────────────────────┘
//
//  绘制层次（从外到内）：
//    1. 外发光光晕：QRadialGradient，从中心向外：
//       - 0.0（中心）：color.lighter(150) → 高亮
//       - 0.5（中段）：color → 本色
//       - 1.0（边缘）：透明 → 渐隐融入背景
//    2. 实心圆：LED 本色，半径 = ledR
//    3. 高光反光点：白色半透明小圆，偏左上，模拟玻璃质感
//    4. 标签文字：右侧对齐显示 LED 名称
// ===========================================================================
void VisualPanel::drawLed(QPainter &p, int idx)
{
    if (idx < 0 || idx >= 3) return;                    // 越界保护

    const QRect &r = m_ledRects[idx];
    const LedInfo &led = m_leds[idx];

    p.save();

    // LED 圆心：矩形左侧 14px 处，垂直居中
    int cx = r.left() + 14;
    int cy = r.center().y();
    int ledR = 7;                                       // LED 圆点基础半径

    // ── 1. 径向渐变光晕 ──
    // 根据点亮/熄灭选择颜色：亮时用 LED 本色，灭时用暗灰色
    QColor color(led.on ? led.color : "#3a4468");

    // 渐变范围从圆心延伸到 ledR+4，在边缘完全透明
    QRadialGradient glow(cx, cy, ledR + 4);
    glow.setColorAt(0.0, color.lighter(150));           // 中心：亮度 +150，模拟发光核心
    glow.setColorAt(0.5, color);                        // 中段：LED 本色
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));           // 边缘：全透明，融入背景

    // 绘制光晕（比实心圆大 2px）
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(QPoint(cx, cy), ledR + 2, ledR + 2);

    // ── 2. 实心 LED 圆 ──
    p.setBrush(color);
    p.drawEllipse(QPoint(cx, cy), ledR, ledR);

    // ── 3. 高光反光点 ──
    // 白色半透明小圆，偏移到左上角 (cx-2, cy-2)，半径 3px
    // 模拟光源从左上角照射的玻璃反射效果
    p.setBrush(QColor(255, 255, 255, 80));               // alpha=80，柔和反光
    p.drawEllipse(QPoint(cx - 2, cy - 2), 3, 3);

    // ── 4. 标签文字 ──
    // 从 LED 圆心右侧 14px 处开始绘制，左对齐、垂直居中
    p.setPen(QColor("#b0b8d0"));
    p.setFont(QFont("Microsoft YaHei", 10));
    p.drawText(r.left() + 28, r.top(), r.width() - 30, r.height(),
               Qt::AlignVCenter | Qt::AlignLeft, led.label);

    p.restore();
}

// ===========================================================================
//  公共设置接口
// ===========================================================================

/// 设置仪表盘数值及量程，仅重绘仪表盘区域以减少闪烁
void VisualPanel::setGaugeValue(double value, double minVal, double maxVal)
{
    m_gaugeValue = value;
    m_gaugeMin   = minVal;
    m_gaugeMax   = maxVal;
    update(m_gaugeRect);                                // 局部刷新，仅重绘表盘区
}

/// 设置仪表盘标签文字，仅重绘仪表盘区域
void VisualPanel::setGaugeLabel(const QString &label)
{
    m_gaugeLabel = label;
    update(m_gaugeRect);                                // 局部刷新
}

/// 设置指定 LED 的开关状态和颜色
/// @param id    LED 标识（LedSerial / LedData / LedWiFi）
/// @param on    是否点亮
/// @param color 点亮颜色（为空时回退为默认绿色 #4caf50）
void VisualPanel::setLedState(LedId id, bool on, const QString &color)
{
    if (id < 0 || id >= 3) return;                      // 越界保护
    m_leds[id].on    = on;
    m_leds[id].color = color.isEmpty() ? "#4caf50" : color;
    update(m_ledRects[id]);                             // 只刷新该 LED 所在矩形
}
