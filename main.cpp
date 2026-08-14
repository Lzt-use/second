/// @file    main.cpp
/// @brief   应用入口 —— 设置全局样式表、创建并显示主窗口
///
/// 全局样式采用深蓝黑主题（Dark Navy），通过 Qt 样式表 (QSS)
/// 集中定义所有控件的视觉风格，避免在各面板中分散设置。

#include "A.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Xiangmu");
    app.setApplicationVersion("1.0");

    // 全局深蓝黑风格样式表
    app.setStyleSheet(R"(

        // ═══════════════════════════════════════════════════════
        //  通用控件样式 (Universal)
        // ═══════════════════════════════════════════════════════
        * {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 13px;
            color: #d8dce8;
            selection-background-color: #3a6bc5;
        }

        // ═══════════════════════════════════════════════════════
        //  主窗口 / 基础控件样式
        // ═══════════════════════════════════════════════════════
        QMainWindow {
            background-color: #1a1e2c;
        }
        QWidget {
            background-color: #1a1e2c;
        }

        // ═══════════════════════════════════════════════════════
        //  分组框样式 (QGroupBox)
        // ═══════════════════════════════════════════════════════
        QGroupBox {
            background-color: #242838;
            border: 1px solid #3a4468;
            border-radius: 6px;
            margin-top: 12px;
            padding: 14px 10px 10px 10px;
            font-weight: bold;
            font-size: 13px;
            color: #b0b8d0;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 8px;
            color: #7eb8ff;
        }

        // ═══════════════════════════════════════════════════════
        //  单行文本输入框样式 (QLineEdit)
        // ═══════════════════════════════════════════════════════
        QLineEdit {
            background-color: #1e2232;
            border: 1px solid #3a4468;
            border-radius: 4px;
            padding: 4px 8px;
            color: #e8eaf0;
            selection-background-color: #3a6bc5;
        }
        QLineEdit:focus {
            border: 1px solid #5078d0;
        }

        // ═══════════════════════════════════════════════════════
        //  多行文本编辑框样式 (QPlainTextEdit)
        // ═══════════════════════════════════════════════════════
        QPlainTextEdit {
            background-color: #1e2232;
            border: 1px solid #3a4468;
            border-radius: 4px;
            padding: 4px;
            color: #e8eaf0;
            selection-background-color: #3a6bc5;
        }
        QPlainTextEdit:focus {
            border: 1px solid #5078d0;
        }

        // ═══════════════════════════════════════════════════════
        //  组合框样式 (QComboBox)
        // ═══════════════════════════════════════════════════════
        QComboBox {
            background-color: #1e2232;
            border: 1px solid #3a4468;
            border-radius: 4px;
            padding: 4px 8px;
            color: #e8eaf0;
            min-width: 80px;
        }
        QComboBox:hover {
            border: 1px solid #5078d0;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 22px;
            border-left: 1px solid #3a4468;
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
            background-color: #2a3048;
        }
        QComboBox QAbstractItemView {
            background-color: #1e2232;
            border: 1px solid #3a4468;
            selection-background-color: #3a6bc5;
            color: #e8eaf0;
        }

        // ═══════════════════════════════════════════════════════
        //  普通按钮样式 (QPushButton)
        // ═══════════════════════════════════════════════════════
        QPushButton {
            background-color: #3b5998;
            border: none;
            border-radius: 4px;
            padding: 6px 18px;
            color: #ffffff;
            font-weight: bold;
            min-width: 72px;
        }
        QPushButton:hover {
            background-color: #4c6ab0;
        }
        QPushButton:pressed {
            background-color: #2a4488;
        }
        QPushButton:disabled {
            background-color: #2a3050;
            color: #5a6080;
        }

        // ═══════════════════════════════════════════════════════
        //  危险操作按钮样式 (#dangerBtn)
        //  通过 objectName 选择器应用于特定按钮
        // ═══════════════════════════════════════════════════════
        QPushButton#dangerBtn {
            background-color: #a04040;
        }
        QPushButton#dangerBtn:hover {
            background-color: #c05050;
        }
        QPushButton#dangerBtn:pressed {
            background-color: #803030;
        }

        // ═══════════════════════════════════════════════════════
        //  成功/确认按钮样式 (#successBtn)
        // ═══════════════════════════════════════════════════════
        QPushButton#successBtn {
            background-color: #3a8040;
        }
        QPushButton#successBtn:hover {
            background-color: #4aa050;
        }
        QPushButton#successBtn:pressed {
            background-color: #2a6030;
        }

        // ═══════════════════════════════════════════════════════
        //  单选按钮样式 (QRadioButton)
        // ═══════════════════════════════════════════════════════
        QRadioButton {
            background: transparent;
            color: #c8d0e0;
            spacing: 6px;
        }
        QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 8px;
            border: 2px solid #3a4468;
            background-color: #1e2232;
        }
        QRadioButton::indicator:checked {
            background-color: #5078d0;
            border: 2px solid #5078d0;
        }

        // ═══════════════════════════════════════════════════════
        //  复选框样式 (QCheckBox)
        // ═══════════════════════════════════════════════════════
        QCheckBox {
            background: transparent;
            color: #c8d0e0;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 2px solid #3a4468;
            background-color: #1e2232;
        }
        QCheckBox::indicator:checked {
            background-color: #5078d0;
            border: 2px solid #5078d0;
        }

        // ═══════════════════════════════════════════════════════
        //  数字微调框样式 (QSpinBox)
        // ═══════════════════════════════════════════════════════
        QSpinBox {
            background-color: #1e2232;
            border: 1px solid #3a4468;
            border-radius: 4px;
            padding: 4px 6px;
            color: #e8eaf0;
        }
        QSpinBox:focus {
            border: 1px solid #5078d0;
        }

        // ═══════════════════════════════════════════════════════
        //  菜单栏样式 (QMenuBar)
        // ═══════════════════════════════════════════════════════
        QMenuBar {
            background-color: #141724;
            border-bottom: 1px solid #2a3050;
            padding: 2px 0;
            color: #d0d8e8;
        }
        QMenuBar::item {
            padding: 4px 14px;
            background: transparent;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #2a3050;
        }

        // ═══════════════════════════════════════════════════════
        //  弹出菜单样式 (QMenu)
        // ═══════════════════════════════════════════════════════
        QMenu {
            background-color: #1e2232;
            border: 1px solid #3a4468;
            border-radius: 4px;
            padding: 4px 0;
        }
        QMenu::item {
            padding: 6px 28px 6px 16px;
        }
        QMenu::item:selected {
            background-color: #3a6bc5;
        }
        QMenu::separator {
            height: 1px;
            background-color: #3a4468;
            margin: 4px 8px;
        }

        // ═══════════════════════════════════════════════════════
        //  状态栏样式 (QStatusBar)
        // ═══════════════════════════════════════════════════════
        QStatusBar {
            background-color: #141724;
            border-top: 1px solid #2a3050;
            color: #8a90a8;
            font-size: 11px;
        }
        QStatusBar::item {
            border: none;
        }

        // ═══════════════════════════════════════════════════════
        //  垂直滚动条样式 (QScrollBar:vertical)
        // ═══════════════════════════════════════════════════════
        QScrollBar:vertical {
            background-color: #1a1e2c;
            width: 10px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background-color: #3a4468;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #5078d0;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }

        // ═══════════════════════════════════════════════════════
        //  水平滚动条样式 (QScrollBar:horizontal)
        // ═══════════════════════════════════════════════════════
        QScrollBar:horizontal {
            background-color: #1a1e2c;
            height: 10px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal {
            background-color: #3a4468;
            border-radius: 5px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #5078d0;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        // ═══════════════════════════════════════════════════════
        //  分割条样式 (QSplitter::handle)
        // ═══════════════════════════════════════════════════════
        QSplitter::handle {
            background-color: #2a3050;
            width: 2px;
            height: 2px;
        }

        // ═══════════════════════════════════════════════════════
        //  自定义状态 LED 指示灯样式 (#statusLed)
        //  用于 VisualPanel 中显示串口/WiFi/数据状态的圆形指示灯
        // ═══════════════════════════════════════════════════════
        QLabel#statusLed {
            border-radius: 7px;
            min-width: 14px;
            max-width: 14px;
            min-height: 14px;
            max-height: 14px;
        }
    )");

    A w;
    w.show();
    return app.exec();
}
