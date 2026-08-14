// ============================================================================
/// @file    B.cpp
/// @brief   实时波形面板 B 类的实现。
///
/// 所属项目：电力电子监测上位机
///
/// 实现要点：
///   - 定时器驱动脏标记，数据到达与重绘解耦。
///   - 2×2 等面积布局，自适应 resize。
///   - 每个 Zone 独立管理滚动窗口 (viewOffset / viewPoints)。
///   - 绘制流程：背景 → 标题/图例 → 网格 → 曲线 → 轴标签 → 位置条。
///   - 鼠标拖动：按像素位移反算 viewOffset 增量。
///   - Ctrl+滚轮：四区同步缩放 viewPoints (20~800)。
// ============================================================================

#include "B.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QShowEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QtMath>

// ============================================================================
// 常量定义
// ============================================================================

/// @brief 左右 Y 轴刻度标签区域的水平边距（像素）。
///
/// 左 Y 轴区域从 Zone 左边界向右预留 MG 像素用于绘制刻度数字；
/// 双线模式右侧同理。单线模式右侧仅留 4 px。
static const int MG = 28;

// ============================================================================
// 构造 / 析构
// ============================================================================

B::B(QWidget *parent) : QWidget(parent)
{
    // 启用鼠标追踪，即使不按键也能收到 move 事件（用于 hover 效果）
    setMouseTracking(true);
    // 允许接收键盘焦点（支持 Ctrl+滚轮缩放）
    setFocusPolicy(Qt::StrongFocus);
    // 告知 Qt 本控件会完整绘制背景，避免不必要的系统清屏
    setAttribute(Qt::WA_OpaquePaintEvent);
    // 启用 hover 事件（当鼠标进入/离开时触发）
    setAttribute(Qt::WA_Hover);

    // 创建刷新定时器
    m_refreshTimer = new QTimer(this);
    // 使用精确计时类型，减少累积漂移
    m_refreshTimer->setTimerType(Qt::PreciseTimer);
    // 30 ms 周期 ≈ 33 fps，兼顾流畅度与 CPU 开销
    m_refreshTimer->setInterval(30);
    connect(m_refreshTimer, &QTimer::timeout, this, &B::onRefreshTick);
    m_refreshTimer->start();
}

// ============================================================================
// 事件处理 ── 显示 / 布局
// ============================================================================

void B::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    // 确保首次显示时布局已计算（可能在构造函数时 size 尚未确定）
    recalcRects();
}

// ============================================================================
/// @brief 重新计算四个 Zone 的像素矩形。
///
/// 布局策略：
///   - 控件宽度减去 12 px（左右边框各 4 px + 中间间隔 4 px）后均分两份。
///   - 控件高度减去 12 px 同理。
///   - 得到 2×2 等面积网格。
///
/// 示意（数字为间距像素）：
/// @code
///   ┌────4────┬─w─┬─4─┬─w─┬4┐
///   │         │←─Zone 0──→│←─Zone 1──→│
///   │    4    ├───┼───┼───┤           │
///   │         │               │               │
///   ├─h─┼───┼─Zone 2─┤─Zone 3─┤
///   │    4    │               │               │
///   └────────┴───┴───┴───┘
/// @endcode
// ============================================================================
void B::recalcRects()
{
    // 控件总宽度/高度减去 12 px 后均分两份
    int w = (width()  - 12) / 2;
    int h = (height() - 12) / 2;
    if (w <= 0 || h <= 0) return;       // 尺寸过小不计算

    for (int i = 0; i < 4; ++i) {
        int c = i % 2;  // 列号：0=左列, 1=右列
        int r = i / 2;  // 行号：0=上行, 1=下行
        // 左上角 X：4(border) + c*(w+4)  其中 4 为列间间距
        // 左上角 Y：4(border) + r*(h+4)  其中 4 为行间间距
        m_z[i].rect = QRect(4 + c * (w + 4), 4 + r * (h + 4), w, h);
    }
}

// ============================================================================
// 绘制
// ============================================================================

void B::paintEvent(QPaintEvent *)
{
    // 每次重绘前重新计算布局（防 resize 漏算）
    recalcRects();

    QPainter p(this);
    // 开启抗锯齿，曲线更平滑
    p.setRenderHint(QPainter::Antialiasing);
    // 全局深色背景
    p.fillRect(rect(), QColor("#1a1e2c"));

    int total = m_vin.size();  // 总数据点数（以 Vin 为准，所有通道同步）

    // ── 辅助 lambda：安全截取 QVector 子区间 ──
    /// @brief 从 src 中截取 [start, end) 区间的数据。
    /// @param start 起始索引。
    /// @param end   结束索引（不含）。
    /// @return 截取到的数据副本；若 start 超出 src 大小则返回空。
    auto sl = [&](const QVector<double> &src, int start, int end) {
        if (src.size() <= start) return QVector<double>();
        return src.mid(start, qMin(end, src.size()) - start);
    };

    // 遍历四个子区域
    for (int z = 0; z < 4; ++z) {
        auto &zone = m_z[z];

        // 根据当前 viewOffset 计算数据窗口 [start, end)
        int start = qBound(0, zone.viewOffset, qMax(0, total - zone.viewPoints));
        int end   = start + zone.viewPoints;

        // ── 按区域准备数据与参数 ──
        QVector<double> d1, d2;
        QColor c1, c2;
        QString t1, t2, title;
        double yMin1 = 0, yMax1 = 30;
        double yMin2 = 0, yMax2 = 30;

        switch (z) {
        case 0: // ── 左上：Vin + Vout，独立 Y 轴（Vin: 0-20V, Vout: 0-10V）
            d1 = sl(m_vin,  start, end);
            d2 = sl(m_vout, start, end);
            c1 = QColor("#448aff");   // 亮蓝 ── Vin
            c2 = QColor("#ff0000");   // 红色 ── Vout
            t1 = "Vin";
            t2 = "Vout";
            title = "输入/输出电压";
            yMin1 = 0;   yMax1 = 30;   // Vin 量程 0~30 V
            yMin2 = 0;   yMax2 = 30;   // Vout 量程 0~30 V
            break;
        case 1: // ── 右上：Iin + Iout，独立 Y 轴（Iin: 0-3A, Iout: 0-2A）
            d1 = sl(m_iin,  start, end);
            d2 = sl(m_iout, start, end);
            c1 = QColor("#ff9100");   // 橙色 ── Iin
            c2 = QColor("#00e5ff");   // 青色 ── Iout
            t1 = "Iin";
            t2 = "Iout";
            title = "输入/输出电流";
            yMin1 = 0;   yMax1 = 5;    // Iin 量程 0~5 A
            yMin2 = 0;   yMax2 = 5;    // Iout 量程 0~5 A
            break;
        case 2: // ── 左下：Temp 单线
            d1 = sl(m_temp, start, end);
            d2 = {};                    // 空 → 单线模式
            c1 = QColor("#e040fb");    // 紫色 ── Temp
            c2 = {};
            t1 = "Temp";
            t2 = {};
            title = "温度";
            yMin1 = 0;   yMax1 = 60;    // 温度量程 0~60 °C
            break;
        case 3: // ── 右下：Humi 单线
            d1 = sl(m_humi, start, end);
            d2 = {};
            c1 = QColor("#76ff03");    // 亮绿 ── Humi
            c2 = {};
            t1 = "Humidity";
            t2 = {};
            title = "湿度";
            yMin1 = 0;   yMax1 = 100;   // 湿度量程 0~100 %
            break;
        }
        // 将准备好的参数送入统一绘制函数
        drawZone(p, zone, d1, d2, c1, c2, t1, t2, title, yMin1, yMax1, yMin2, yMax2);
    }
}

// ============================================================================
/// @brief 绘制单个子区域的全部图形元素。
///
/// @param p      QPainter 实例（已绑定控件）。
/// @param z      该 Zone 的矩形与滚动参数。
/// @param d1     曲线1 的数据片段（已按窗口截取）。
/// @param d2     曲线2 的数据片段（空=单线模式）。
/// @param c1,c2  曲线颜色。
/// @param t1,t2  图例标签。
/// @param title  区域标题。
/// @param yMin1, yMax1  曲线1 Y 轴范围。
/// @param yMin2, yMax2  曲线2 Y 轴范围（双线模式有效）。
///
/// 绘制顺序（由上到下分层）：
///   1. 背景填充 + 外框线
///   2. 左上角标题
///   3. 右上角图例（先画曲线2，再画曲线1，保证曲线1 在最右侧）
///   4. 网格（8 横 × 12 纵虚线）
///   5. 曲线1 实线、曲线2（独立 Y 轴映射）
///   6. 左 Y 轴刻度（曲线1）、右 Y 轴刻度（曲线2，仅双线）
///   7. X 轴刻度（每 5 点标一次，自适应密度）
///   8. 底部滚动位置指示条（灰色底色 + 蓝色滑块）
// ============================================================================
void B::drawZone(QPainter &p, const Zone &z,
                 const QVector<double> &d1, const QVector<double> &d2,
                 const QColor &c1, const QColor &c2,
                 const QString &t1, const QString &t2,
                 const QString &title,
                 double yMin1, double yMax1,
                 double yMin2, double yMax2)
{
    // 判断是否为双线模式（d2 非空即为双线）
    const bool dual = !d2.isEmpty();
    const QRect &r = z.rect;

    // ── 1. 背景 + 外框 ──
    p.save();
    p.setClipRect(r);                           // 限制绘制不越界
    p.fillRect(r, QColor("#0d1117"));           // 深色背景
    p.setPen(QPen(QColor("#1e2840"), 1));       // 边框：深蓝灰 1 px
    p.drawRect(r);

    // ── 2. 标题（左上角） ──
    p.setPen(QColor("#7eb8ff"));                // 浅蓝色
    p.setFont(QFont("Microsoft YaHei", 7, QFont::Bold));  // 7 pt 粗体
    p.drawText(r.left() + 3, r.top() + 10, title);

    // ── 3. 图例（右上角，从右向左排列） ──
    int lx = r.right() - 4;                     // 图例起始 X（右边界留 4 px）
    p.setFont(QFont("Microsoft YaHei", 6, QFont::Bold));  // 6 pt 粗体

    /// @brief 局部 lambda：绘制一条图例色块 + 标签。
    ///        双线模式时标签格式为 "Name [Ymin-Ymax]"。
    auto drawLegend = [&](const QString &t, const QColor &c, double yMin, double yMax) {
        if (t.isEmpty()) return;
        QString label = dual
            ? QString("%1 [%2-%3]").arg(t).arg(yMin, 0, 'f', 0).arg(yMax, 0, 'f', 0)
            : t;
        QFontMetrics fm(p.font());
        int tw = fm.horizontalAdvance(label) + 14;  // 14 = 色块宽度(8) + 间距(6)
        lx -= tw;                                    // 从右向左推进
        p.setPen(Qt::NoPen);
        p.setBrush(c);                               // 用曲线颜色填充色块
        p.drawRoundedRect(lx, r.top() + 1, 8, 8, 2, 2);  // 8×8 px 圆角色块
        p.setPen(c);
        p.drawText(lx + 10, r.top() + 9, label);    // 标签文字，色块右侧 10 px
        lx -= 2;                                     // 图例间距 2 px
    };
    // 先画曲线2 再画曲线1，保证曲线1（主曲线）在图例最右侧
    drawLegend(t2, c2, yMin2, yMax2);
    drawLegend(t1, c1, yMin1, yMax1);

    // ⚠ 重要：绘制完图例后必须清空画刷，否则后续 QPainterPath 会被填充
    p.setBrush(Qt::NoBrush);

    // ── 4. 计算绘图区参数 ──
    // 右侧边距：双线模式需要 MG(28) 像素给第二 Y 轴，单线仅留 4 px
    int rightMG = dual ? MG : 4;
    int px = r.left() + MG;                  // 绘图区左边界（预留左 Y 轴）
    int py = r.top()  + 14;                  // 绘图区上边界（标题占 14 px）
    int pw = r.width() - MG - rightMG;       // 绘图区宽度
    int ph = r.height() - 28;                // 绘图区高度（底部留 28 px 给 X 轴和位置条）

    // ⚠ 重要：精确剪裁绘图区，防止曲线画出网格边界
    p.save();
    p.setClipRect(QRect(px, py, pw, ph), Qt::IntersectClip);

    // 实际数据窗口起止索引（用于 X 轴标签计算）
    int total = m_vin.size();
    int sIdx  = z.viewOffset;
    int eIdx  = total > 0 ? qMin(sIdx + z.viewPoints, total) : z.viewPoints;

    // ── 5. 网格线 ──
    p.setPen(QPen(QColor("#131b26"), 1, Qt::DotLine));  // 深蓝灰虚线 1 px
    // 水平网格：8 条，对应 Y 轴 9 个刻度点（含上下边界）
    for (int i = 0; i <= 8; ++i) {
        int y = py + ph * i / 8;
        p.drawLine(px, y, px + pw, y);
    }
    // 垂直网格：12 条，提供适中的时间轴参考线密度
    for (int i = 0; i <= 12; ++i) {
        int x = px + pw * i / 12;
        p.drawLine(x, py, x, py + ph);
    }

    // ── 6. 曲线绘制 ──
    // 曲线1 的 Y 轴映射参数
    double yR1c = yMax1 - yMin1;           // Y 轴跨度
    if (yR1c <= 0) yR1c = 1;               // 防止除零
    double baseY = py + ph - 1;            // 绘图区底部 Y 坐标（-1 留 1 px 余量防越界）

    // ── 辅助 lambda：画单点（实心小圆）──
    auto drawDot = [&](const QColor &c, double x, double y) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(x, y), 3.0, 3.0);
    };

    // 曲线1（实线/单点圆）
    if (d1.size() >= 1) {
        int n1 = d1.size();
        if (n1 == 1) {
            drawDot(c1, px + pw / 2.0,
                    baseY - (d1[0] - yMin1) / yR1c * (ph - 2));
        } else {
            QPainterPath path1;
            double xs = (double)pw / (n1 - 1);
            // Y = baseY - (value - yMin) / range * (ph - 2)
            //   值越大 ⇨ Y 越小（屏幕上方），值越小 ⇨ Y 越大（屏幕下方）
            path1.moveTo(px, baseY - (d1[0] - yMin1) / yR1c * (ph - 2));
            for (int i = 1; i < n1; ++i)
                path1.lineTo(px + i * xs,
                             baseY - (d1[i] - yMin1) / yR1c * (ph - 2));
            p.setPen(QPen(c1, 1.5));           // 线宽 1.5 px
            p.drawPath(path1);
        }
    }

    // 曲线2：使用独立的 Y 轴映射（与曲线1 可不同量程）
    if (dual && d2.size() >= 1) {
        double yR2c = yMax2 - yMin2;
        if (yR2c <= 0) yR2c = 1;               // 防除零
        int n2 = d2.size();
        if (n2 == 1) {
            drawDot(c2, px + pw / 2.0,
                    baseY - (d2[0] - yMin2) / yR2c * (ph - 2));
        } else {
            QPainterPath path2;
            double xs = (double)pw / (n2 - 1);
            path2.moveTo(px, baseY - (d2[0] - yMin2) / yR2c * (ph - 2));
            for (int i = 1; i < n2; ++i)
                path2.lineTo(px + i * xs,
                             baseY - (d2[i] - yMin2) / yR2c * (ph - 2));
            QPen pen2(c2, 1.5);
            p.setPen(pen2);
            p.drawPath(path2);
        }
    }

    // 恢复精确绘图区剪裁 —— 接下来绘制标签，不受绘图区限制
    p.restore();

    // ── 7. Y 轴刻度标签 ──
    p.setFont(QFont("Consolas", 6));         // 等宽字体 6 pt
    p.setPen(QColor("#6a6a7a"));             // 灰白色
    double yR1 = yMax1 - yMin1;
    if (yR1 <= 0) yR1 = 1;
    // 左 Y 轴：从上到下 9 个刻度 (i=0..8)
    for (int i = 0; i <= 8; ++i) {
        int y = py + ph * i / 8;
        double val = yMax1 - yR1 * i / 8.0;  // 从上到下递减
        QString s = QString::number(val, 'f', 1);  // 保留 1 位小数
        QFontMetrics fm(p.font());
        p.drawText(px - fm.horizontalAdvance(s) - 3, y + 3, s);  // 右对齐于左边界
    }

    // 右 Y 轴：仅双线模式绘制
    if (dual) {
        double yR2 = yMax2 - yMin2;
        if (yR2 <= 0) yR2 = 1;
        int rx = px + pw + 3;                // 右 Y 轴标签起始 X
        for (int i = 0; i <= 8; ++i) {
            int y = py + ph * i / 8;
            double val = yMax2 - yR2 * i / 8.0;
            QString s = QString::number(val, 'f', 1);
            p.drawText(rx, y + 3, s);        // 左对齐于右边界
        }
    }

    // ── 8. X 轴刻度标签 ──
    p.setPen(QColor("#4a5a7a"));             // 蓝灰色
    p.setFont(QFont("Consolas", 6));
    // 实际可显示的数据点数（用于计算标签密度）
    int actualN = qMin(z.viewPoints, total - sIdx);
    if (actualN <= 0) actualN = z.viewPoints;

    // 自适应 X 轴标签步长：初始 5 点一标，若标签数超过 20 则增大步长
    int xStep = 5;
    while (actualN / xStep > 20) xStep += 5;

    // 从 sIdx 向上取整到 xStep 的整数倍开始标注
    for (int labelIdx = (sIdx / xStep) * xStep; labelIdx <= eIdx; labelIdx += xStep) {
        if (labelIdx < sIdx) continue;       // 跳过窗口之前的整数倍点
        // X 轴位置 = 左边界 + 绘图宽度 * (当前下标在窗口中的比例)
        double frac = (double)(labelIdx - sIdx) / qMax(1, actualN);
        int x = px + (int)(pw * frac);
        QString text = QString::number(labelIdx);
        QFontMetrics fm(p.font());
        p.drawText(x - fm.horizontalAdvance(text) / 2, py + ph + 9, text);  // 居中
    }

    // ── 9. 底部滚动位置指示条 ──
    if (total > 0 && z.scrollMax > 0) {
        // 当前位置占总可滚动范围的比例
        double frac = (double)z.viewOffset / z.scrollMax;
        int barY = py + ph + 14;             // 指示条 Y 坐标
        // 灰色轨道
        p.fillRect(px, barY, pw, 2, QColor("#1e2840"));
        // 蓝色滑块（宽度至少 4 px，或绘图区宽度的 1/20）
        int posX = px + (int)(frac * pw * 0.8);
        p.fillRect(posX, barY, qMax(4, pw / 20), 2, QColor("#5078d0"));
    }

    // 恢复最外层 clip
    p.restore();
}

// ============================================================================
// 数据管理
// ============================================================================

// ============================================================================
/// @brief 添加一批（6 路）采样数据，并维护滚动状态。
///
/// 内部使用 lambda `pu` 将数据追加到 QVector 末尾；当 QVector 大小
/// 超过 #kMaxBuf（8000 点）时，从头部丢弃最早的数据，形成环形缓冲效果。
///
/// 滚动行为：若当前视图已在末尾（viewOffset >= scrollMax - 2），
/// 则自动跟随到最新数据位置。否则保持用户当前查看位置不变。
// ============================================================================
void B::addDataBatch(double vin, double vout, double iin, double iout,
                      double temp, double humi)
{
    // 辅助 lambda：追加数据并限制缓冲区大小
    /// @brief push-update：追加一个数据点，超过 kMaxBuf 时丢弃最旧的。
    auto pu = [](QVector<double> &v, double d) {
        v.append(d);
        while (v.size() > kMaxBuf) v.removeFirst();  // 环形缓冲：丢弃头部旧数据
    };
    pu(m_vin,  vin);
    pu(m_vout, vout);
    pu(m_iin,  iin);
    pu(m_iout, iout);
    pu(m_temp, temp);
    pu(m_humi, humi);

    // 更新各 Zone 的滚动参数
    int total = m_vin.size();
    for (int i = 0; i < 4; ++i) {
        // scrollMax：最大可滚动偏移量 = 总点数 - 窗口点数
        m_z[i].scrollMax = qMax(0, total - m_z[i].viewPoints);
        // 判断当前是否在末尾（容差 2 点，避免微小抖动导致不跟随）
        bool atEnd = (m_z[i].viewOffset >= m_z[i].scrollMax - 2);
        if (atEnd || m_z[i].scrollMax == 0)
            m_z[i].viewOffset = m_z[i].scrollMax;  // 自动跟至末尾
    }
    // 设置脏标记，等待定时器触发重绘（避免高频数据涌入时每次 update）
    m_dirty = true;
}

// ============================================================================
/// @brief 清空全部历史数据，并将各 Zone 的滚动状态复位。
// ============================================================================
void B::clearAll()
{
    m_vin.clear();
    m_vout.clear();
    m_iin.clear();
    m_iout.clear();
    m_temp.clear();
    m_humi.clear();
    for (int i = 0; i < 4; ++i) {
        m_z[i].viewOffset = 0;
        m_z[i].scrollMax  = 0;
    }
    m_dirty = true;  // 触发重绘
}

// ============================================================================
// 定时刷新
// ============================================================================

void B::onRefreshTick()
{
    // 仅当有脏数据时才触发重绘
    if (m_dirty) {
        m_dirty = false;
        update();    // 请求 Qt 在下一事件循环中调用 paintEvent()
    }
}

// ============================================================================
// 滚轮缩放
// ============================================================================

// ============================================================================
/// @brief Ctrl+滚轮缩放时间窗口。
///
/// 缩放逻辑：
///   - Ctrl 按下时，四个 Zone 同步调整 viewPoints。
///   - viewPoints 范围：[20, 800]。
///     - 20  ≈ 0.6 秒窗口（@33 fps），适合观察细节。
///     - 800 ≈ 24 秒窗口，适合宏观趋势。
///   - 鼠标滚轮步长：angleDelta().y() / 40，每个刻度调整约 1 点。
///   - 缩放后若 viewOffset 超出新的 scrollMax，则钳位到 scrollMax。
/// ============================================================================
void B::wheelEvent(QWheelEvent *e)
{
    // 仅在 Ctrl 按下时响应缩放
    if (e->modifiers() & Qt::ControlModifier) {
        // QWheelEvent::angleDelta().y() 典型值为 ±120（一个滚轮刻度）
        // 除以 40 得到 ≈ ±3 点的步长，手感适中
        int d = e->angleDelta().y() / 40;
        int total = m_vin.size();

        // 四个区域同步缩放
        for (int i = 0; i < 4; ++i) {
            // viewPoints 范围限制：最小 20 点，最大 800 点
            m_z[i].viewPoints = qBound(20, m_z[i].viewPoints - d, 800);
            // 重新计算最大滚动范围
            m_z[i].scrollMax = qMax(0, total - m_z[i].viewPoints);
            // 若当前偏移量超出新范围则钳位
            if (m_z[i].viewOffset > m_z[i].scrollMax)
                m_z[i].viewOffset = m_z[i].scrollMax;
        }
        update();      // 立即重绘
        e->accept();   // 标记事件已处理
    } else {
        // 非 Ctrl 修饰时交还父类处理（可被外部 scroll area 捕获）
        QWidget::wheelEvent(e);
    }
}

// ============================================================================
// 鼠标拖动
// ============================================================================

// ============================================================================
/// @brief 鼠标按下：检测命中的 Zone 并记录拖动起点。
///
/// 算法：
///   1. 仅响应左键。
///   2. 遍历四个 Zone 的矩形，用 contains() 检测命中。
///   3. 命中后记录 m_active（Zone 索引）、m_dragX（鼠标 X）、
///      m_dragOff（当前 viewOffset）。
///   4. 光标切换为 ClosedHandCursor。
// ============================================================================
void B::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;  // 仅左键拖动

    for (int i = 0; i < 4; ++i) {
        if (m_z[i].rect.contains(e->pos())) {
            m_active  = i;                       // 标记活动 Zone
            m_dragX   = e->pos().x();            // 记录起始鼠标 X 坐标
            m_dragOff = m_z[i].viewOffset;       // 记录起始偏移量
            setCursor(Qt::ClosedHandCursor);     // 抓手光标
            e->accept();
            return;
        }
    }
}

// ============================================================================
/// @brief 鼠标移动：根据水平位移同步滚动波形。
///
/// 核心算法：
///   1. 计算水平像素位移 dx = 起始X - 当前X（往左拖 ⇨ dx>0 ⇨ viewOffset 增大）。
///   2. 将像素位移转换为数据偏移量：
///        scale = viewPoints / 绘图区宽度(px)
///        newOff = 起始偏移 + round(dx * scale)
///   3. 将 newOff 钳位到 [0, scrollMax]。
///   4. 直接 update() 重绘（不走脏标记，保证拖动跟手）。
///
/// @note 绘图区宽度计算必须与 drawZone() 保持一致（双线 zone 右侧
///       预留 MG(28) 给第二 Y 轴）。
// ============================================================================
void B::mouseMoveEvent(QMouseEvent *e)
{
    if (m_active < 0) return;  // 未激活拖动则忽略

    // 像素位移：鼠标向左拖 → dx > 0
    int dx = m_dragX - e->pos().x();
    auto &z = m_z[m_active];

    // 与 drawZone 保持一致的绘图区宽度计算
    bool dual = (m_active == 0 || m_active == 1);  // zone 0,1 为双线模式
    int rightMG = dual ? MG : 4;
    int pw = z.rect.width() - MG - rightMG;
    if (pw <= 0) return;

    // 像素 → 数据点：比例 = 窗口数据点数 / 绘图区像素宽度
    double scale = (double)z.viewPoints / pw;
    // 新偏移量 = 起始偏移 + 像素位移 × 比例（四舍五入）
    int newOff = m_dragOff + qRound(dx * scale);
    // 钳位到合法范围
    newOff = qBound(0, newOff, z.scrollMax);
    z.viewOffset = newOff;

    update();       // 立即重绘，保证拖动流畅
    e->accept();
}

// ============================================================================
/// @brief 鼠标释放：结束拖动，恢复光标。
// ============================================================================
void B::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_active >= 0) {
        m_active = -1;                       // 清除活动标记
        setCursor(Qt::ArrowCursor);          // 恢复默认箭头光标
        e->accept();
    }
}
