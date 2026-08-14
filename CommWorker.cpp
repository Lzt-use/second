// =============================================================================
// 文件: CommWorker.cpp
// 描述: CommWorker 的实现 —— 跨线程统一通讯层，在子线程中串行执行所有 IO 操作。
//       公有槽被调用方通过跨线程排队的信号连接触发，私有槽由底层 QIODevice 信号
//       直连触发（同线程 DirectConnection）。信号从子线程跨线程发射到主线程。
// =============================================================================

#include "CommWorker.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkInterface>

// ──────────────────────────────────────────────────────
// 构造 / 析构
// ──────────────────────────────────────────────────────

CommWorker::CommWorker(QObject *parent)
    : QObject(parent)
{
    // 构造函数不分配任何通讯资源。
    // 对象通常在主线程构造，随后通过 moveToThread() 移至工作线程。
    // 所有实际 IO 对象的创建推迟到 openSerial/openWiFi 槽中，
    // 确保对象在正确线程的上下文中创建（Qt 要求 QObject 及子对象存活于同一线程）。
}

CommWorker::~CommWorker()
{
    // 析构在创建此对象的线程（通常为主线程）中执行。
    // 跨线程 delete 底层 QObject 是安全的，因为 Qt 会通过
    // QObject 的线程亲和性机制处理生命周期。
    // 更优雅的做法是通过 deleteLater() 在工作线程中销毁，
    // 但直接 close+delete 在实际上也是工程实践中普遍可行的。

    if (m_serial) {
        m_serial->close();
        delete m_serial;
    }
    if (m_tcpServer) {
        m_tcpServer->close();
        delete m_tcpServer;
    }
    if (m_tcpSocket) {
        m_tcpSocket->close();
        delete m_tcpSocket;
    }
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
    }
}

// ═══════════════════════════════════════════════════════
//  串口
// ═══════════════════════════════════════════════════════

void CommWorker::openSerial(const QString &portName, int baudRate,
                             int dataBitsIdx, int parityIdx, int stopBitsIdx)
{
    // ── 第一步：清理旧的通讯对象 ──
    // 保证同一时刻只有一种通讯模式处于激活状态
    closeSerial();
    closeWiFi();

    // ── 第二步：创建新的 QSerialPort 对象 ──
    // QSerialPort 的 parent 设为 this，确保随 CommWorker 销毁而自动销毁
    m_serial = new QSerialPort(this);

    // 解析纯端口名称：调用方传入的 portName 可能包含 "COM3 - 描述" 这样的
    // 复合格式（常见于 QSerialPortInfo::portName() + " - " + description()），
    // 这里取 " - " 之前的纯端口名称部分
    QString pureName = portName.split(" - ").first().trimmed();
    m_serial->setPortName(pureName);
    m_serial->setBaudRate(baudRate);

    // ── 第三步：配置数据位 / 校验位 / 停止位 ──
    // 对外接口接受索引值（方便 UI 层 combobox 绑定），内部通过静态数组
    // 将索引映射到 QSerialPort 枚举。
    //
    // 索引映射表:
    //   dataBitsIdx: 0→Data5, 1→Data6, 2→Data7, 3→Data8
    //   parityIdx  : 0→NoParity, 1→EvenParity, 2→OddParity, 3→MarkParity, 4→SpaceParity
    //   stopBitsIdx: 0→OneStop, 1→OneAndHalfStop, 2→TwoStop
    //
    // 使用 qBound() 将索引钳制在有效范围内，防止越界访问

    static QSerialPort::DataBits dbs[] = {
        QSerialPort::Data5, QSerialPort::Data6, QSerialPort::Data7, QSerialPort::Data8
    };
    static QSerialPort::Parity pars[] = {
        QSerialPort::NoParity, QSerialPort::EvenParity,
        QSerialPort::OddParity,  QSerialPort::MarkParity, QSerialPort::SpaceParity
    };
    static QSerialPort::StopBits sbs[] = {
        QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop
    };

    m_serial->setDataBits(dbs[qBound(0, dataBitsIdx, 3)]);
    m_serial->setParity(pars[qBound(0, parityIdx, 4)]);
    m_serial->setStopBits(sbs[qBound(0, stopBitsIdx, 2)]);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    // ── 第四步：打开串口并连接信号 ──
    // QSerialPort::readyRead 与 errorOccurred 信号在此以 DirectConnection 方式
    // 连接到私有槽（因为 m_serial 和 this 在同一线程），回调执行效率最高。
    // 私有槽内部再通过 emit 将数据跨线程转发给调用方。

    if (m_serial->open(QIODevice::ReadWrite)) {
        connect(m_serial, &QSerialPort::readyRead,  this, &CommWorker::onSerialReadyRead);
        connect(m_serial, &QSerialPort::errorOccurred, this, &CommWorker::onSerialError);
        m_mode = CommMode::Serial;
        emit connected(true, QStringLiteral("串口 %1 打开成功 (波特率:%2)")
                       .arg(pureName).arg(baudRate));
    } else {
        emit connected(false, QStringLiteral("串口打开失败: %1").arg(m_serial->errorString()));
        // 打开失败时立即销毁对象，避免残留无效的 QSerialPort 实例
        delete m_serial;
        m_serial = nullptr;
    }
}

void CommWorker::closeSerial()
{
    // 关闭串口流程：
    //  1. 检查是否持有串口对象（可能从未打开或已经被关闭）
    //  2. 若已打开，先调用 close() 释放硬件资源
    //  3. delete 对象，置 nullptr
    //  4. 通过 disconnected 信号通知调用方连接已断开
    if (m_serial) {
        if (m_serial->isOpen()) m_serial->close();
        delete m_serial;
        m_serial = nullptr;
        emit disconnected();
    }
}

void CommWorker::onSerialReadyRead()
{
    // 串口数据到达回调 —— 运行在子线程
    // 直接 readAll() 获取全部可用数据，非空时跨线程发射 dataReceived 信号。
    // Qt 信号系统自动将 QByteArray 深拷贝到接收方线程的事件队列中。
    if (!m_serial) return;
    QByteArray data = m_serial->readAll();
    if (!data.isEmpty()) emit dataReceived(data);
}

void CommWorker::onSerialError()
{
    // 串口错误回调 —— 将 QSerialPort::errorString() 跨线程转发给调用方
    if (m_serial)
        emit errorOccurred(m_serial->errorString());
}

// ═══════════════════════════════════════════════════════
//  WiFi
// ═══════════════════════════════════════════════════════

void CommWorker::openWiFi(WiFiSubMode subMode, const QString &ip, quint16 port)
{
    // ── 第一步：清理旧通讯对象 ──
    closeSerial();
    closeWiFi();   // 先清理旧网络对象，避免新旧 socket 冲突

    // ── 第二步：根据子模式创建对应的网络对象 ──
    // 关键设计：所有子对象（QTcpServer / QTcpSocket / QUdpSocket）的 parent
    // 均为 this，保证生命周期绑定到 CommWorker。

    switch (subMode) {

    // ─── TCP Server 模式 ───
    case WiFiSubMode::TcpServer: {
        m_tcpServer = new QTcpServer(this);
        // 直接连接 newConnection 信号到 onTcpNewConnection 槽（同线程 DirectConnection）
        connect(m_tcpServer, &QTcpServer::newConnection,
                this, &CommWorker::onTcpNewConnection);

        // 监听所有 IPv4 网络接口的指定端口
        if (m_tcpServer->listen(QHostAddress::AnyIPv4, port)) {
            m_mode = CommMode::WiFi;
            emit connected(true, QStringLiteral("TCP 服务器已启动 :%1").arg(port));
        } else {
            emit connected(false, QStringLiteral("TCP 服务器启动失败: %1")
                           .arg(m_tcpServer->errorString()));
            delete m_tcpServer;
            m_tcpServer = nullptr;
        }
        break;
    }

    // ─── TCP Client 模式 ───
    case WiFiSubMode::TcpClient: {
        m_tcpSocket = new QTcpSocket(this);
        // 连接 TCP Socket 的 readyRead / disconnected / errorOccurred 到对应私有槽
        connect(m_tcpSocket, &QTcpSocket::readyRead,  this, &CommWorker::onTcpReadyRead);
        connect(m_tcpSocket, &QTcpSocket::disconnected, this, &CommWorker::onTcpDisconnected);
        connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &CommWorker::onTcpError);

        // 异步发起连接到远端主机
        // 连接成功/失败的结果将通过 onTcpReadyRead / onTcpError / onTcpDisconnected 异步通知
        m_tcpSocket->connectToHost(ip, port);
        break;
    }

    // ─── UDP 模式 ───
    case WiFiSubMode::Udp: {
        m_udpSocket = new QUdpSocket(this);
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &CommWorker::onUdpReadyRead);
        connect(m_udpSocket, &QUdpSocket::errorOccurred, this, &CommWorker::onUdpError);

        // bind() 绑定本地端口，使用 ShareAddress 允许其他进程复用该端口
        // 绑定成功后进入就绪状态，随时可收发数据报
        if (m_udpSocket->bind(port, QUdpSocket::ShareAddress)) {
            m_mode = CommMode::WiFi;
            emit connected(true, QStringLiteral("UDP 已绑定端口 %1").arg(port));
        } else {
            emit connected(false, QStringLiteral("UDP 绑定失败: %1")
                           .arg(m_udpSocket->errorString()));
            delete m_udpSocket;
            m_udpSocket = nullptr;
        }
        break;
    }
    }
}

void CommWorker::closeWiFi()
{
    // 关闭所有 WiFi 相关的网络对象：
    //  1. 关闭 QTcpServer（停止监听，断开所有客户端）
    //  2. 关闭并销毁 m_tcpSocket（断开 TCP 连接）
    //  3. 关闭并销毁 m_udpSocket（释放 UDP 端口）
    //  4. 若当前模式是 WiFi，发出 disconnected 信号通知调用方

    if (m_tcpServer) {
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }
    if (m_tcpSocket) {
        m_tcpSocket->close();
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
    }
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
    if (m_mode == CommMode::WiFi) emit disconnected();
}

void CommWorker::sendWiFiAtCmd(const QString &atCmd)
{
    // AT 指令透传 —— 通过串口发送给 ESP8266/ESP32 等 WiFi 模组
    // 典型用法：AT\r\n → 模组回复 OK
    // 注意：本方法仅在串口已打开时才有效，因为 AT 通讯走的是串口而非 WiFi socket

    if (m_serial && m_serial->isOpen()) {
        QByteArray cmd = (atCmd + "\r\n").toUtf8();
        m_serial->write(cmd);
    } else {
        emit errorOccurred(QStringLiteral("WiFi AT 指令需要串口连接 ESP8266"));
    }
}

// ── TCP Server 回调 ──

void CommWorker::onTcpNewConnection()
{
    // TCP Server 新客户端连接处理 —— 运行在子线程
    //
    // 设计策略：当前实现采用 **单客户端** 模式。
    // 每次新的客户端连接到来时，会关闭并销毁旧的 m_tcpSocket（如果存在），
    // 然后只保留最新连接的客户端。这是简化设计的工程折中 ——
    // 适合大多数嵌入式/工控场景下的点对点通讯。
    //
    // 若需支持多客户端并发，应在外部维护 QList<QTcpSocket*> 而非复用 m_tcpSocket。

    if (!m_tcpServer) return;

    // 使用 while 循环处理所有积压的连接请求（虽然通常只有一个）
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *client = m_tcpServer->nextPendingConnection();

        // 只保留一个客户端 —— 新客户端替换旧客户端
        if (m_tcpSocket) {
            m_tcpSocket->close();
            delete m_tcpSocket;
        }
        m_tcpSocket = client;

        // 为新客户端连接信号槽（同线程 DirectConnection）
        connect(m_tcpSocket, &QTcpSocket::readyRead,  this, &CommWorker::onTcpReadyRead);
        connect(m_tcpSocket, &QTcpSocket::disconnected, this, &CommWorker::onTcpDisconnected);
        connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &CommWorker::onTcpError);

        // 跨线程通知调用方客户端已连接
        emit connected(true, QStringLiteral("客户端已连接: %1:%2")
                       .arg(m_tcpSocket->peerAddress().toString())
                       .arg(m_tcpSocket->peerPort()));
    }
}

// ── TCP 数据到达（Server 端客户端 和 Client 端共用此槽） ──

void CommWorker::onTcpReadyRead()
{
    // TCP 数据到达回调 —— 运行在子线程
    //
    // 通过 sender() 获取发送信号的 QTcpSocket 指针。
    // 这是此设计的巧妙之处：同一个 onTcpReadyRead 槽同时服务于
    // TcpServer 模式下的 client socket 和 TcpClient 模式下的自身 socket，
    // 无需为两种模式编写重复代码。
    //
    // qobject_cast<> 是安全的动态转换，若 sender() 不是 QTcpSocket 则返回 nullptr。

    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (sock) {
        QByteArray data = sock->readAll();
        if (!data.isEmpty()) emit dataReceived(data);
    }
}

void CommWorker::onTcpDisconnected()
{
    // TCP 断开回调 —— 运行在子线程
    // 通过 sender() 获取断开连接的 socket 指针。
    // 仅当断开的是当前激活的 m_tcpSocket 时才通知调用方，
    // 避免旧的已经被替换掉的 socket 断开时误发 disconnected。
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (sock == m_tcpSocket) {
        emit disconnected();
    }
}

void CommWorker::onTcpError()
{
    // TCP 错误回调 —— 将错误信息跨线程转发给调用方
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (sock) emit errorOccurred(sock->errorString());
}

// ── UDP 回调 ──

void CommWorker::onUdpReadyRead()
{
    // UDP 数据报到达回调 —— 运行在子线程
    //
    // UDP 是无连接协议，因此需要在此记录对端的 IP 和端口，用于后续的 sendData
    // 能够定向回发数据。这种设计实现了 UDP 的 "伪连接" 模式：
    //   收到数据报 → 记住来源地址 → 后续 sendData 写回此地址
    //
    // 记录方式：利用 QObject 的 property 机制在 m_udpSocket 上存储
    // lastPeerAddr（QString）和 lastPeerPort（quint16）两个动态属性。
    // 这种方式的优点是无需额外成员变量，且属性随对象生命周期自动管理。
    //
    // 局限性：每次有新数据报到达时，回发地址会被更新为最新的发送方，
    // 因此不支持同时与多个对端通信 —— 适合工控场景下的一对一 UDP 通信。

    if (!m_udpSocket) return;

    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(m_udpSocket->pendingDatagramSize());

        QHostAddress senderAddr;
        quint16 senderPort = 0;

        // readDatagram 同时读取数据和发送方地址信息
        m_udpSocket->readDatagram(data.data(), data.size(), &senderAddr, &senderPort);

        // ── 核心：记录来源地址用于后续定向发送 ──
        // 将最后一次收到的数据报的 IP 和端口存储为 QObject 动态属性，
        // sendData 中将读取这些属性来确定 UDP 回发目标。
        m_udpSocket->setProperty("lastPeerAddr", senderAddr.toString());
        m_udpSocket->setProperty("lastPeerPort", senderPort);

        // 跨线程发射数据接收信号
        emit dataReceived(data);
    }
}

void CommWorker::onUdpError()
{
    // UDP 错误回调 —— 将错误信息跨线程转发给调用方
    if (m_udpSocket) emit errorOccurred(m_udpSocket->errorString());
}

// ═══════════════════════════════════════════════════════
//  通用发送
// ═══════════════════════════════════════════════════════

void CommWorker::sendData(const QByteArray &data)
{
    // 统一数据发送入口 —— 根据当前模式 m_mode 分发到不同底层接口
    //
    // 线程安全性：此方法作为公有槽，由调用方通过排队的跨线程信号连接调用，
    // 确保在与 IO 回调相同的子线程中串行执行，与 onXxxReadyRead 之间
    // 不会产生竞态条件。

    if (m_mode == CommMode::Serial) {

        // ── 串口发送 ──
        // 使用同步等待 waitForBytesWritten(100ms) 保证数据写入硬件缓冲区。
        // 若 100ms 内未完成写入则报告错误。这一阻塞时间很短，
        // 在子线程中不会影响 UI 响应。
        if (m_serial && m_serial->isOpen()) {
            m_serial->write(data);
            if (!m_serial->waitForBytesWritten(100)) {
                emit errorOccurred(QStringLiteral("串口发送失败"));
            }
        }

    } else if (m_mode == CommMode::WiFi) {

        // ── TCP 发送 ──
        // 仅当 TCP socket 处于已连接状态时才发送，随后 flush() 强制刷新缓冲区。
        if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
            m_tcpSocket->write(data);
            m_tcpSocket->flush();
        }

        // ── UDP 发送 ──
        // 读取之前 onUdpReadyRead 中记录的 lastPeerAddr / lastPeerPort 属性，
        // 使用 writeDatagram 将数据报定向发送回最近通信的对端。
        // 若从未收到过任何数据报（地址为空或 port 为 0），则报告错误。
        else if (m_udpSocket) {
            QString addr = m_udpSocket->property("lastPeerAddr").toString();
            quint16 port = m_udpSocket->property("lastPeerPort").toUInt();
            if (!addr.isEmpty() && port != 0) {
                m_udpSocket->writeDatagram(data, QHostAddress(addr), port);
            } else {
                emit errorOccurred(QStringLiteral("UDP 未收到过对端数据，无法确定发送目标"));
            }
        }
    }
}
