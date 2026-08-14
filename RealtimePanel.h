// ============================================================================
// RealtimePanel.h
// 实时数据面板 — 头文件
//
// 职责：
//   - 以 2×3 网格卡片形式展示 6 项实时传感器数据（电压/电流/温度/湿度）
//   - 提供只读的原始数据日志窗口
//   - 对外暴露设置接口和日志接口，内部封装标签更新逻辑
// ============================================================================

#pragma once

#include <QWidget>
#include <QLabel>
#include <QPlainTextEdit>

class QGridLayout;

/// 实时数据结构化显示：6项参数（2行×3列网格卡片） + 原始数据日志
///
/// 布局示意（垂直方向）：
/// ┌──────────────────────────────┐
/// │  [输入电压] [输出电压] [输入电流]  │  ← 第 0 行
/// │  [输出电流] [温    度] [湿    度]  │  ← 第 1 行
/// ├──────────────────────────────┤
/// │  原始数据日志 (只读, 可折叠)      │  ← QPlainTextEdit
/// └──────────────────────────────┘
class RealtimePanel : public QWidget
{
    Q_OBJECT

public:
    /// 构造函数：初始化 2×3 网格和日志控件，所有数值初始化为 0.0
    explicit RealtimePanel(QWidget *parent = nullptr);

    // ── 六大参数设置接口 ──

    /// 设置输入电压（单位 V），更新 m_vinLabel
    void setInputVoltage(double v);
    /// 设置输出电压（单位 V），更新 m_voutLabel
    void setOutputVoltage(double v);
    /// 设置输入电流（单位 A），更新 m_iinLabel
    void setInputCurrent(double a);
    /// 设置输出电流（单位 A），更新 m_ioutLabel
    void setOutputCurrent(double a);
    /// 设置温度（单位 °C），更新 m_tempLabel
    void setTemperature(double c);
    /// 设置湿度（单位 %），更新 m_humiLabel
    void setHumidity(double pct);

    // ── 日志操作 ──

    /// 追加一行文本到日志窗口
    void appendLog(const QString &line);
    /// 清空日志窗口的全部内容
    void clearLog();

private:
    /// 创建一个统一样式的数值显示标签（深色背景、青色等宽字体）
    QLabel *createValueLabel();

    /// 格式化更新标签文本为 "value unit"（如 "12.3 V"），保留一位小数
    void    updateLabel(QLabel *label, double value, const QString &unit);

    // ── 2×3 网格中 6 个数值标签 ──
    QLabel *m_vinLabel  = nullptr;  ///< 输入电压标签（第 0 行第 0 列）
    QLabel *m_voutLabel = nullptr;  ///< 输出电压标签（第 0 行第 1 列）
    QLabel *m_iinLabel  = nullptr;  ///< 输入电流标签（第 0 行第 2 列）
    QLabel *m_ioutLabel = nullptr;  ///< 输出电流标签（第 1 行第 0 列）
    QLabel *m_tempLabel = nullptr;  ///< 温度标签     （第 1 行第 1 列）
    QLabel *m_humiLabel = nullptr;  ///< 湿度标签     （第 1 行第 2 列）

    QPlainTextEdit *m_logEdit = nullptr;  ///< 只读原始数据日志控件
};
