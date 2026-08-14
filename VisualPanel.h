// ============================================================================
// VisualPanel.h
// 可视化面板 — 头文件
//
// 职责：
//   - 自绘仪表盘（圆弧、颜色分段指针、数值标签）
//   - 3 个 LED 状态指示灯（径向渐变发光效果）
//   - 响应 resizeEvent 动态计算仪表盘和 LED 的布局区域
// ============================================================================

#pragma once

#include <QWidget>

class QLabel;

/// 可视化面板：仪表盘 + 状态指示灯
///
/// 布局示意（垂直方向）：
/// ┌──────────────────────────────┐
/// │         仪表盘区域            │  ← 上半部分 (~68% 高度)，自绘圆弧+指针
/// ├──────────────────────────────┤
/// │  LED 1: 串口状态             │  ← 第 1 行指示灯
/// │  LED 2: 数据收发             │  ← 第 2 行指示灯
/// │  LED 3: WiFi状态             │  ← 第 3 行指示灯
/// └──────────────────────────────┘
class VisualPanel : public QWidget
{
    Q_OBJECT

public:
    /// 构造函数：初始化 LED 状态信息，设置最小高度
    explicit VisualPanel(QWidget *parent = nullptr);

    // ── 仪表盘接口 ──

    /// 设置仪表盘当前值及量程范围，触发表盘区域重绘
    void setGaugeValue(double value, double minVal = 0.0, double maxVal = 100.0);
    /// 设置仪表盘下方显示的标签文字（如"输出电压"），触发重绘
    void setGaugeLabel(const QString &label);

    // ── LED 指示灯接口 ──

    /// LED 标识枚举：串口状态 / 数据收发 / WiFi 状态
    enum LedId { LedSerial, LedData, LedWiFi };
    /// 设置指定 LED 的开/关状态和颜色；颜色为空时使用默认绿色 #4caf50
    void setLedState(LedId id, bool on, const QString &color = "#4caf50");

protected:
    /// 自绘入口：填充背景 → 绘制外边框 → 绘制仪表盘 → 绘制 3 个 LED
    void paintEvent(QPaintEvent *event) override;
    /// 尺寸变化时重新计算仪表盘矩形和 3 个 LED 矩形的位置
    void resizeEvent(QResizeEvent *event) override;

private:
    // ── 绘制子过程 ──

    /// 在矩形 r 内绘制完整仪表盘（弧线、指针、数值、标签）
    void drawGauge(QPainter &p, const QRect &r);
    /// 绘制第 index 个 LED 指示灯（径向渐变发光 + 标签文字）
    void drawLed(QPainter &p, int index);

    // ── 仪表盘数据 ──
    double m_gaugeValue = 0.0;       ///< 当前仪表盘数值
    double m_gaugeMin   = 0.0;       ///< 仪表盘量程下限
    double m_gaugeMax   = 100.0;     ///< 仪表盘量程上限
    QString m_gaugeLabel = QStringLiteral("数值");  ///< 仪表盘底部标签文字

    // ── LED 指示灯数据 ──
    struct LedInfo {
        QString label;               ///< LED 旁显示的标签文字
        QString color = "#4caf50";   ///< 点亮时的颜色（默认绿色）
        bool    on    = false;       ///< 当前点亮状态
    };
    LedInfo m_leds[3];               ///< 3 个 LED：串口 / 数据 / WiFi

    // ── 布局矩形（由 resizeEvent 计算）──
    QRect m_gaugeRect;               ///< 仪表盘绘制区域
    QRect m_ledRects[3];             ///< 三个 LED 各自的绘制区域
};
