/// @file    SerialCfgPanel.h
/// @brief   串口参数配置面板 —— 提供串口号选择、波特率/数据位/校验位/停止位配置、
///          打开/关闭串口操作以及串口状态展示的 UI 面板。
/// @author  自动生成
/// @date    2025

#pragma once

#include <QWidget>
#include <QStringList>

class QComboBox;
class QPushButton;
class QLabel;

/// @class SerialCfgPanel
/// @brief 串口参数配置面板。
///
/// 本面板使用"卡片"风格布局（CardFrame 作为外层容器），内部组织为：
/// - 端口行：COM 下拉框 + 刷新按钮
/// - 参数网格：波特率、数据位、校验位、停止位四组标签/下拉框
/// - 操作行：打开/关闭按钮 + 连接状态指示
///
/// 通过信号 openRequested / closeRequested 向外部通知用户操作，
/// 不直接持有 QSerialPort；外部读取参数后自行构造串口对象。
class SerialCfgPanel : public QWidget
{
    Q_OBJECT

public:
    /// @brief 构造函数：初始化 UI 布局、控件样式、信号连接，并刷新端口列表。
    /// @param parent 父级窗口（默认无父级）。
    explicit SerialCfgPanel(QWidget *parent = nullptr);

    // ── 参数读取接口 ──

    /// @brief 获取当前选中的串口名称（如下拉框文本）。
    QString     portName()    const;

    /// @brief 获取当前波特率数值（从可编辑下拉框文本解析整数）。
    int         baudRate()    const;

    /// @brief 获取数据位下拉框当前选中索引（0-3 对应 5/6/7/8）。
    int         dataBitsIdx() const;

    /// @brief 获取校验位下拉框当前选中索引（0-4 对应 None/Even/Odd/Mark/Space）。
    int         parityIdx()   const;

    /// @brief 获取停止位下拉框当前选中索引（0-2 对应 1/1.5/2）。
    int         stopBitsIdx() const;

    // ── 控件状态 / 状态更新 ──

    /// @brief 统一设置所有配置控件的启用/禁用状态，并切换打开/关闭按钮的 UI 模式。
    /// @param enabled  true → 展示"打开串口"模式（绿色按钮，未连接状态）；
    ///                false → 展示"关闭串口"模式（红色按钮，已连接状态）。
    void        setControlsEnabled(bool enabled);

    /// @brief 更新状态标签的文本与颜色。
    /// @param text  展示文本（如 "●  串口已连接"）。
    /// @param color CSS 颜色字面量（如 "#4caf50"）。
    void        setConnectionStatus(const QString &text, const QString &color);

    /// @brief 重新枚举系统可用串口并填充到端口下拉框；无串口时显示占位提示。
    void        refreshPortList();

signals:
    /// @brief 用户点击"打开串口"按钮时发出。
    void openRequested();

    /// @brief 用户点击"关闭串口"按钮时发出。
    void closeRequested();

private:
    /// @brief 串口号选择下拉框（含描述信息）。
    QComboBox  *m_portCombo   = nullptr;

    /// @brief 波特率可编辑下拉框，默认 115200。
    QComboBox  *m_baudCombo   = nullptr;

    /// @brief 数据位下拉框：5 / 6 / 7 / 8，默认 8。
    QComboBox  *m_dataCombo   = nullptr;

    /// @brief 校验位下拉框：None / Even / Odd / Mark / Space。
    QComboBox  *m_parityCombo = nullptr;

    /// @brief 停止位下拉框：1 / 1.5 / 2。
    QComboBox  *m_stopCombo   = nullptr;

    /// @brief 打开/关闭串口按钮 —— 文本与样式随连接状态联动切换。
    QPushButton *m_openBtn    = nullptr;

    /// @brief 刷新端口列表按钮。
    QPushButton *m_refreshBtn = nullptr;

    /// @brief 状态指示标签，默认显示"●  串口未连接"（红色）。
    QLabel     *m_statusLabel = nullptr;
};
