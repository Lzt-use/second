// =============================================================================
// 文件: CommWorker.h
// 描述: 跨线程统一通讯层 —— 在子线程中运行串口/WiFi（TCP Server/Client/UDP）
//       的收发工作，通过 Qt 信号槽跨线程向主线程/业务层投递数据与状态事件。
//       CommWorker 本身是一个 QObject，被外部通过 moveToThread() 移动到
//       专用的 QThread 中执行，所有公有槽通过排队的跨线程信号连接由调用方线程触发。
// =============================================================================

#pragma once

#include <QObject>
#include <QByteArray>

class QSerialPort;
class QTcpServer;
class QTcpSocket;
class QUdpSocket;
class QTimer;

// -----------------------------------------------------------------
// 通讯模式枚举 —— 决定当前工作在线程中使用串口还是 WiFi 对象
// -----------------------------------------------------------------

/// 顶层通讯模式
enum class CommMode {
    Serial, ///< 串口通讯
    WiFi    ///< WiFi / 网络通讯（TCP Server、TCP Client 或 UDP）
};

/// WiFi 子模式，仅在 CommMode::WiFi 下生效
enum class WiFiSubMode {
    TcpServer, ///< TCP 服务器模式 —— 监听端口，接受一个客户端连接
    TcpClient, ///< TCP 客户端模式 —— 主动连接远端服务器
    Udp        ///< UDP 无连接模式 —— 绑定本地端口收发数据报
};

// =============================================================================
// CommWorker —— 跨线程通讯工作对象
// =============================================================================

/// 统一通讯工作线程（运行在子线程）
///
/// ## 设计意图（moveToThread 模式）
/// Qt 推荐在此场景下将 QObject 子类实例通过 moveToThread() 移动到
/// QThread 中，而非重写 QThread::run()。CommWorker 被创建在主线程，
/// 然后移动到一个后台 QThread，其所有公有槽的调用将通过 Qt::QueuedConnection
/// 排队到子线程事件循环中串行执行，天然线程安全且无需手动加锁。
///
/// ## 信号数据流向
/// 本类发出的所有信号均为跨线程信号：
///   - 调用方（主线程）连接槽时默认使用 Qt::AutoConnection，Qt 检测到
///     信号发送者与接收者在不同线程时会自动升级为 QueuedConnection，
///     信号参数被深拷贝后投递到目标线程的事件队列。
///   - 因此多个底层 socket 回调（readyRead 等）内部直接 emit 信号是安全的。
///
/// ## 主要数据路径
///   @startuml
///     调用方线程 -> CommWorker (子线程) : openSerial/openWiFi/sendData (QueuedConnection)
///     CommWorker (子线程) -> 调用方线程 : dataReceived/connected/disconnected/errorOccurred
///   @enduml
///
class CommWorker : public QObject
{
    Q_OBJECT

public:
    /// 构造函数 —— 仅初始化基类，不分配任何通讯资源（所有资源延迟到 openXxx 中创建）
    explicit CommWorker(QObject *parent = nullptr);

    /// 析构函数 —— 关闭并销毁当前持有的所有通讯对象（串口、TCP Server/Client、UDP Socket）
    /// @note 析构必然在创建此对象的线程中执行，而非在工作线程中。关闭底层对象的操作
    ///       本身是线程安全的，但建议使用者在销毁前先通过 deleteLater() 保证安全的生命周期。
    ~CommWorker() override;

public slots:
    // ---- 串口 ----

    /// 打开并配置串口
    /// @param portName    串口名称（如 "COM3"），可包含 "COM3 - xxx" 描述格式，内部自动截取纯名称
    /// @param baudRate    波特率（如 9600, 115200）
    /// @param dataBitsIdx 数据位索引（0=Data5, 1=Data6, 2=Data7, 3=Data8）
    /// @param parityIdx   校验位索引（0=NoParity, 1=EvenParity, 2=OddParity, 3=MarkParity, 4=SpaceParity）
    /// @param stopBitsIdx 停止位索引（0=OneStop, 1=OneAndHalfStop, 2=TwoStop）
    /// @threadsafe 本槽由调用方线程通过排队的跨线程信号连接调用，串行执行
    void openSerial(const QString &portName, int baudRate,
                    int dataBitsIdx, int parityIdx, int stopBitsIdx);

    /// 关闭并销毁当前串口对象，发送 disconnected 信号
    void closeSerial();

    // ---- WiFi ----

    /// 打开 WiFi / 网络通讯（三种子模式之一）
    /// @param subMode 子模式（TcpServer / TcpClient / Udp）
    /// @param ip      远端 IP 地址（TcpClient 模式下需要；TcpServer/Udp 忽略此参数）
    /// @param port    绑定的本地端口号（TcpServer/Udp）或远端端口（TcpClient）
    /// @threadsafe 本槽由调用方线程通过排队的跨线程信号连接调用，串行执行
    void openWiFi(WiFiSubMode subMode, const QString &ip, quint16 port);

    /// 关闭并销毁当前 WiFi 相关的所有对象（TcpServer / TcpSocket / UdpSocket）
    /// 若当前模式为 WiFi 则同时发出 disconnected 信号
    void closeWiFi();

    /// 通过串口向 ESP8266 等 WiFi 模组发送 AT 指令
    /// @param atCmd AT 指令内容（不含结尾的 \\r\\n，内部会自动追加）
    /// @note 本方法仅在串口已打开时有效，用于调试/配置 WiFi 模组
    void sendWiFiAtCmd(const QString &atCmd);

    // ---- 通用 ----

    /// 统一的数据发送入口（根据当前 m_mode 分发至串口/TCP/UDP）
    /// @param data 待发送的原始字节数据
    /// @note 对于 UDP 模式，发送目标由上一条收到的数据报的来源地址决定
    void sendData(const QByteArray &data);

signals:
    /// 收到底层数据时发出 —— 包括串口、TCP、UDP 三种来源
    /// @param data 收到的原始字节数据
    /// @note 信号参数将跨线程深拷贝后投递
    void dataReceived(const QByteArray &data);

    /// 连接建立或失败时发出
    /// @param success 是否成功
    /// @param message 附带的可读消息（成功时是提示，失败时是错误原因）
    void connected(bool success, const QString &message);

    /// 底层连接断开时发出（串口关闭 / TCP 客户端断开 / WiFi 模式关闭）
    void disconnected();

    /// 发生错误时发出（串口错误 / TCP Socket 错误 / UDP Socket 错误）
    /// @param error 错误描述字符串
    void errorOccurred(const QString &error);

private slots:
    // ---- 串口回调（由 QSerialPort 信号直接连接，运行于子线程） ----

    /// 串口有数据可读时由 QSerialPort::readyRead 触发
    void onSerialReadyRead();

    /// 串口发生错误时由 QSerialPort::errorOccurred 触发
    void onSerialError();

    // ---- TCP Server 回调 ----

    /// QTcpServer 有新客户端连接请求时触发 —— 循环接受所有未处理的连接
    /// @note 当前实现只保留最后一个客户端（新客户端会关闭并销毁旧的）
    void onTcpNewConnection();

    /// TCP Socket（Server 的客户端或 Client 自身）有数据可读时触发
    /// 通过 sender() 运行时泛型获取发送信号的 socket 指针
    void onTcpReadyRead();

    /// TCP Socket 远端断开时触发 —— 由 sender() 推断并根据引用判断是否为当前激活连接
    void onTcpDisconnected();

    /// TCP Socket 发生错误时触发 —— 通过 sender() 获取错误消息
    void onTcpError();

    // ---- UDP 回调 ----

    /// UDP Socket 有数据报到达时触发 —— 读取数据并记录对端地址用于后续定向发送
    /// @note 这是 UDP "伪连接" 机制的核心：每次收到数据报时更新 lastPeerAddr/lastPeerPort
    void onUdpReadyRead();

    /// UDP Socket 发生错误时由 QUdpSocket::errorOccurred 触发
    void onUdpError();

private:
    // ── 通讯对象（同一时刻最多只有一组处于激活状态） ──

    QSerialPort   *m_serial    = nullptr; ///< 串口对象

    QTcpServer    *m_tcpServer = nullptr; ///< TCP 服务器监听对象（仅 TcpServer 模式）
    QTcpSocket    *m_tcpSocket = nullptr; ///< TCP 连接对象（TcpClient 模式下的自身连接，
                                          ///  或 TcpServer 模式下当前唯一的客户端连接）
    QUdpSocket    *m_udpSocket = nullptr; ///< UDP 收发对象（仅 Udp 模式）

    // ── 状态 ──

    CommMode       m_mode     = CommMode::Serial; ///< 当前激活的通讯模式
};
