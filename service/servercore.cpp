#include "servercore.h"
#include "database.h"
#include "matchmaking.h"
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTimer>

ServerCore::ServerCore(QObject *parent)
    : QObject(parent)
    , m_tcpServer(new QTcpServer(this))
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &ServerCore::onNewConnection);
    // 初始化房间清理定时器
    m_roomCleanupTimer = new QTimer(this);
    m_roomCleanupTimer->setInterval(60000); // 每分钟清理一次
    connect(m_roomCleanupTimer, &QTimer::timeout, this, &ServerCore::cleanupExpiredRooms);
    m_roomCleanupTimer->start();
}

bool ServerCore::startServer(quint16 port)
{
    if (m_tcpServer->isListening()) {
        return true;
    }

    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        emit logMessage(QString("监听失败: %1").arg(m_tcpServer->errorString()), Qt::red);
        return false;
    }

    // 获取本机IP地址
    QStringList ipAddresses;
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
            address != QHostAddress::LocalHost) {
            ipAddresses.append(address.toString());
        }
    }
    //ipAddresses.append("LAPTOP-NGN9AGTE");
    emit logMessage(QString("服务器启动成功"), QColor("#4CAF50"));
    emit logMessage(QString("监听端口: %1").arg(port), Qt::white);
    emit logMessage(QString("本地地址: %1").arg(QHostAddress(QHostAddress::LocalHost).toString()), Qt::white);
    if (!ipAddresses.isEmpty()) {
        emit logMessage(QString("网络地址: %1").arg(ipAddresses.join(", ")), Qt::white);
    }

    return true;
}

void ServerCore::stopServer()
{
    if (m_tcpServer->isListening()) {
        // 通知所有客户端服务器关闭
        QJsonObject shutdownMsg;
        shutdownMsg["type"] = "server_shutdown";
        shutdownMsg["message"] = "服务器维护中，请稍后重连";

        for (QTcpSocket *socket : m_clients.keys()) {
            sendResponse(socket, "system", shutdownMsg);
            socket->disconnectFromHost();
        }

        m_tcpServer->close();
        m_clients.clear();
        m_usernameToSocket.clear();

        emit logMessage("服务器已停止", QColor("#FF9800"));
    }
}

void ServerCore::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        QString ipAddress = socket->peerAddress().toString();

        // 添加到临时客户端列表（未登录）
        ClientInfo info;
        info.ipAddress = ipAddress;
        info.connectTime = QDateTime::currentDateTime();
        info.status = "未登录";
        info.socket = socket;

        m_clients.insert(socket, info);

        connect(socket, &QTcpSocket::disconnected, this, &ServerCore::onClientDisconnected);
        connect(socket, &QTcpSocket::readyRead, this, &ServerCore::onReadyRead);

        emit logMessage(QString("新的连接: %1").arg(ipAddress), QColor("#2196F3"));
        emit clientConnected();

        // 发送欢迎消息
        QJsonObject welcomeMsg;
        welcomeMsg["message"] = "欢迎连接到Match3游戏服务器";
        welcomeMsg["version"] = "1.0.0";
        welcomeMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        sendResponse(socket, "welcome", welcomeMsg);
    }
}

// servercore.cpp - 确保onClientDisconnected函数正确发射信号
void ServerCore::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (m_clients.contains(socket)) {
        ClientInfo info = m_clients[socket];
        QString username = info.username;

        if (!username.isEmpty()) {
            // 【关键修复】确保从所有映射中移除用户
            m_usernameToSocket.remove(username);

            // 如果用户在房间中，处理房间逻辑
            if (m_userToRoom.contains(username)) {
                QString roomId = m_userToRoom[username];
                m_userToRoom.remove(username);

                if (m_gameRooms.contains(roomId)) {
                    GameRoom &room = m_gameRooms[roomId];
                    // 标记游戏结束
                    room.gameEnded = true;
                    // 清理房间
                    cleanupRoom(roomId);
                }
            }

            // 发射客户端断开信号
            emit clientDisconnected(username);
        }

        m_clients.remove(socket);
        socket->deleteLater();

        // 广播在线列表更新
        broadcastOnlineList();
    }
}

void ServerCore::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        emit logMessage(QString("JSON解析错误: %1").arg(error.errorString()), Qt::red);
        return;
    }

    if (!doc.isObject()) {
        emit logMessage("无效的消息格式", Qt::red);
        return;
    }

    QJsonObject msg = doc.object();
    QString type = msg["type"].toString();


    // 记录收到的消息（过滤心跳包）
    if (type != "heartbeat") {
        QString from = m_clients.contains(socket) ? m_clients[socket].username : "未知";
        emit logMessage(QString("收到消息 [%1]: %2").arg(from).arg(type), Qt::cyan);
    }

    // 根据消息类型处理
    if (type == "login") {
        processLogin(socket, msg);
    } else if (type == "match_request") {
        processMatchRequest(socket, msg);
    } else if (type == "game_move") {
        processGameMove(socket, msg);
    } else if (type == "chat") {
        processChatMessage(socket, msg);
    } else if (type == "heartbeat") {
        processHeartbeat(socket, msg);
    } else if (type == "user_status") {  // 新增：处理用户状态更新
        processUserStatus(socket, msg);
    }else if (type == "game_start") {
        processGameStart(socket, msg);
    } else if (type == "game_move") {
        processGameMove(socket, msg);
    } else if (type == "player_quit") {  // 新增：处理玩家退出
        processPlayerQuit(socket, msg);
    } else if (type == "game_end") {
        processGameEnd(socket, msg);
    } else {
        emit logMessage(QString("未知的消息类型: %1").arg(type), Qt::yellow);
    }
}

// 实现processGameEnd函数
void ServerCore::processGameEnd(QTcpSocket *socket, const QJsonObject &data)
{
    if (!m_clients.contains(socket)) return;

    QString username = m_clients[socket].username;
    QString roomId = data["room_id"].toString();
    int finalScore = data["final_score"].toInt();

    qDebug() << "收到游戏结束消息:" << username << "房间:" << roomId << "分数:" << finalScore;

    if (m_userToRoom.contains(username) && m_gameRooms.contains(roomId)) {
        GameRoom &room = m_gameRooms[roomId];

        // 更新玩家分数
        if (room.player1 == username) {
            room.player1Score = finalScore;
        } else if (room.player2 == username) {
            room.player2Score = finalScore;
        }

        // 标记玩家已完成游戏
        if (username == room.player1) {
            room.player1Ready = true;
        } else if (username == room.player2) {
            room.player2Ready = true;
        }

        // 如果双方都发送了结束消息，或者时间到，则结束游戏
        if ((room.player1Ready && room.player2Ready) || room.gameEnded) {
            handleGameEnd(roomId);
        } else {
            // 只有一方结束，通知对手
            QString opponent = (room.player1 == username) ? room.player2 : room.player1;
            if (m_usernameToSocket.contains(opponent)) {
                QTcpSocket *opponentSocket = m_usernameToSocket[opponent];

                QJsonObject opponentMsg;
                opponentMsg["type"] = "opponent_finished";
                opponentMsg["username"] = username;
                opponentMsg["score"] = finalScore;

                sendResponse(opponentSocket, "game_update", opponentMsg);

                emit logMessage(QString("玩家 %1 已完成游戏，分数: %2").arg(username).arg(finalScore),
                                QColor("#4CAF50"));
            }
        }
    }
}

void ServerCore::processLogin(QTcpSocket *socket, const QJsonObject &data)
{
    QString username = data["username"].toString();
    QString password = data["password"].toString();

    QJsonObject response;

    // 检查数据库验证
    Database db;
    if (db.verifyUser(username, password)) {
        // 检查是否已经登录
        if (m_usernameToSocket.contains(username)) {
            // 如果是同一用户重新连接，更新socket映射
            QTcpSocket *oldSocket = m_usernameToSocket[username];
            if (oldSocket != socket) {
                // 关闭旧的连接
                oldSocket->disconnectFromHost();
                m_clients.remove(oldSocket);
                m_usernameToSocket[username] = socket;
            }
        }

        // 登录成功
        ClientInfo &info = m_clients[socket];
        info.username = username;
        info.status = "在线";
        info.gameMode = "空闲";

        // 【关键修复】确保用户名到socket的映射正确建立
        m_usernameToSocket[username] = socket;

        response["success"] = true;
        response["message"] = "登录成功";
        response["username"] = username;

        // 发送用户数据
        QJsonObject userData = db.getUserData(username);
        response["user_data"] = userData;

        // 如果用户在房间中，更新房间中的socket映射
        if (m_userToRoom.contains(username)) {
            QString roomId = m_userToRoom[username];
            if (m_gameRooms.contains(roomId)) {
                // 确保房间信息正确
                GameRoom &room = m_gameRooms[roomId];
                // 不需要额外操作，映射已更新
            }
        }

        // 【重要】发射用户登录成功信号
        emit userLoggedIn(username, info.ipAddress);

        // 广播在线列表更新
        broadcastOnlineList();
    } else {
        response["success"] = false;
        response["message"] = "用户名或密码错误";
    }

    sendResponse(socket, "login_result", response);
}

void ServerCore::processPlayerQuit(QTcpSocket *socket, const QJsonObject &data)
{
    if (!m_clients.contains(socket)) return;

    QString quitter = data["quitter"].toString();
    QString opponent = data["opponent"].toString();

    qDebug() << "处理玩家退出，退出者:" << quitter << "，对手:" << opponent;

    // 根据用户名查找房间ID
    QString roomId;

    // 首先尝试用退出者查找
    if (m_userToRoom.contains(quitter)) {
        roomId = m_userToRoom[quitter];
        qDebug() << "通过退出者找到房间ID:" << roomId;
    }
    // 如果找不到，尝试用对手查找
    else if (m_userToRoom.contains(opponent)) {
        roomId = m_userToRoom[opponent];
        qDebug() << "通过对手找到房间ID:" << roomId;
    }
    // 如果都找不到，尝试从数据中获取
    else if (data.contains("room_id")) {
        roomId = data["room_id"].toString();
        qDebug() << "从数据中获取房间ID:" << roomId;
    }

    if (roomId.isEmpty()) {
        qDebug() << "无法找到房间，退出者:" << quitter << "，对手:" << opponent;
        emit logMessage(QString("无法处理玩家 %1 退出：找不到对应的房间")
                            .arg(quitter), QColor("#FF9800"));
        return;
    }

    if (m_gameRooms.contains(roomId)) {
        GameRoom &room = m_gameRooms[roomId];

        // 验证退出者是否在房间中
        if (quitter != room.player1 && quitter != room.player2) {
            qDebug() << "退出者" << quitter << "不在房间" << roomId << "中";
            return;
        }

        // 确定获胜者
        QString winner;
        if (quitter == room.player1) {
            winner = room.player2;
        } else {
            winner = room.player1;
        }

        // 更新分数
        room.player1Score = data["player1_score"].toInt();
        room.player2Score = data["player2_score"].toInt();

        // 标记游戏结束
        room.gameEnded = true;

        // 如果房间有定时器，停止它
        if (room.timer && room.timer->isActive()) {
            room.timer->stop();
        }

        qDebug() << "退出者:" << quitter << "，获胜者:" << winner
                 << "，房间ID:" << roomId;

        // 发送游戏结束消息给获胜者
        QTcpSocket *winnerSocket = m_usernameToSocket.value(winner);
        if (winnerSocket && winnerSocket->isValid()) {
            QJsonObject endMsg;
            endMsg["room_id"] = roomId;
            endMsg["player1_score"] = room.player1Score;
            endMsg["player2_score"] = room.player2Score;
            endMsg["player1"] = room.player1;
            endMsg["player2"] = room.player2;
            endMsg["winner"] = winner;
            endMsg["reason"] = "对手退出";
            endMsg["quitter"] = quitter;

            sendResponse(winnerSocket, "game_end", endMsg);

            qDebug() << "发送获胜消息给:" << winner;

            // 记录日志
            emit logMessage(QString("玩家 %1 退出游戏，房间 %2 结束，胜者: %3")
                                .arg(quitter)
                                .arg(roomId)
                                .arg(winner),
                            QColor("#FF9800"));
        }

        // 如果退出者还在线，也发送结束消息
        QTcpSocket *quitterSocket = m_usernameToSocket.value(quitter);
        if (quitterSocket && quitterSocket->isValid()) {
            QJsonObject endMsg;
            endMsg["room_id"] = roomId;
            endMsg["player1_score"] = room.player1Score;
            endMsg["player2_score"] = room.player2Score;
            endMsg["player1"] = room.player1;
            endMsg["player2"] = room.player2;
            endMsg["winner"] = winner;
            endMsg["reason"] = "你已退出";
            endMsg["quitter"] = quitter;

            sendResponse(quitterSocket, "game_end", endMsg);
        }

        // 延迟清理房间（给客户端处理时间）
        QTimer::singleShot(3000, this, [this, roomId]() {
            cleanupRoom(roomId);
        });

        // 更新用户状态
        if (winnerSocket && m_clients.contains(winnerSocket)) {
            m_clients[winnerSocket].status = "在线";
            m_clients[winnerSocket].gameMode = "空闲";
        }

        if (quitterSocket && m_clients.contains(quitterSocket)) {
            m_clients[quitterSocket].status = "在线";
            m_clients[quitterSocket].gameMode = "空闲";
        }

        // 广播在线列表更新
        broadcastOnlineList();
    } else {
        qDebug() << "房间不存在:" << roomId;
    }
}

// 修改 processMatchRequest 函数中的房间创建和状态更新部分
void ServerCore::processMatchRequest(QTcpSocket *socket, const QJsonObject &data)
{
    if (!m_clients.contains(socket)) return;

    ClientInfo &info = m_clients[socket];
    QString username = info.username;
    QString gameMode = data["mode"].toString();

    // 创建房间ID
    QString roomId = QString("room_%1_%2").arg(QDateTime::currentSecsSinceEpoch()).arg(QRandomGenerator::global()->bounded(1000));

    // 检查是否有等待中的玩家
    QString waitingPlayer;
    for (const ClientInfo &client : m_clients.values()) {
        if (client.status == "匹配中" && client.username != username) {
            waitingPlayer = client.username;
            break;
        }
    }

    QJsonObject response;

    if (!waitingPlayer.isEmpty()) {
        // 找到对手，创建房间
        GameRoom room;
        room.roomId = roomId;
        room.player1 = waitingPlayer;
        room.player2 = username;
        room.player1Score = 0;
        room.player2Score = 0;
        room.gameStarted = false;
        room.gameEnded = false;
        room.player1Ready = false;
        room.player2Ready = false;
        room.timer = nullptr;

        // 【关键修复】确保房间和用户映射正确建立
        m_gameRooms[roomId] = room;
        m_userToRoom[waitingPlayer] = roomId;
        m_userToRoom[username] = roomId;

        // 获取双方的socket - 确保能从映射中找到
        QTcpSocket *player1Socket = m_usernameToSocket.value(waitingPlayer);
        QTcpSocket *player2Socket = m_usernameToSocket.value(username);

        // 【关键修复】如果找不到socket，可能是映射有问题
        if (!player1Socket) {
            qDebug() << "找不到玩家1的socket:" << waitingPlayer;
            // 尝试从m_clients中查找
            for (QTcpSocket *sock : m_clients.keys()) {
                if (m_clients[sock].username == waitingPlayer) {
                    player1Socket = sock;
                    break;
                }
            }
        }

        if (!player2Socket) {
            qDebug() << "找不到玩家2的socket:" << username;
            // player2Socket 应该是当前的socket
            player2Socket = socket;
        }

        // 确保两个socket都存在
        if (player1Socket && player2Socket) {
            // 更新双方状态
            if (m_clients.contains(player1Socket)) {
                m_clients[player1Socket].status = "联机游戏中";
                m_clients[player1Socket].gameMode = gameMode;
            }
            if (m_clients.contains(player2Socket)) {
                m_clients[player2Socket].status = "联机游戏中";
                m_clients[player2Socket].gameMode = gameMode;
            }

            // 发送匹配成功消息
            QJsonObject matchSuccess1;
            matchSuccess1["room_id"] = roomId;
            matchSuccess1["player1"] = waitingPlayer;
            matchSuccess1["player2"] = username;
            matchSuccess1["game_mode"] = gameMode;
            matchSuccess1["match_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            sendResponse(player1Socket, "match_found", matchSuccess1);

            QJsonObject matchSuccess2;
            matchSuccess2["room_id"] = roomId;
            matchSuccess2["player1"] = waitingPlayer;
            matchSuccess2["player2"] = username;
            matchSuccess2["game_mode"] = gameMode;
            matchSuccess2["match_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            sendResponse(player2Socket, "match_found", matchSuccess2);
        }

        response["success"] = true;
        response["message"] = "匹配成功";
        response["room_id"] = roomId;

    } else {
        // 没有找到对手，加入等待队列
        info.status = "匹配中";
        info.gameMode = gameMode;

        response["success"] = true;
        response["message"] = "已加入匹配队列";
        response["queue_position"] = 1;
    }

    sendResponse(socket, "match_response", response);
    broadcastOnlineList();
}

// 实现processGameStart函数
void ServerCore::processGameStart(QTcpSocket *socket, const QJsonObject &data)
{
    if (!m_clients.contains(socket)) return;

    QString username = m_clients[socket].username;
    QString roomId = data["room_id"].toString();
    QJsonArray board = data["board"].toArray();
    int score = data["score"].toInt();

    if (m_userToRoom.contains(username) && m_gameRooms.contains(roomId)) {
        GameRoom &room = m_gameRooms[roomId];

        // 更新玩家棋盘状态
        if (room.player1 == username) {
            room.player1Board = board;
            room.player1Score = score;
            room.player1Ready = true;
        } else if (room.player2 == username) {
            room.player2Board = board;
            room.player2Score = score;
            room.player2Ready = true;
        }

        emit logMessage(QString("玩家 %1 已准备，房间: %2").arg(username).arg(roomId),
                        QColor("#2196F3"));

        // 如果双方都准备好了，开始游戏
        if (room.player1Ready && room.player2Ready && !room.gameStarted) {
            handleGameStart(roomId);
        }
    }
}

void ServerCore::processGameMove(QTcpSocket *socket, const QJsonObject &data)
{
    if (!m_clients.contains(socket)) return;

    QString username = m_clients[socket].username;

    // 【添加调试信息】查看原始数据
    qDebug() << "收到game_move消息，用户:" << username;
    qDebug() << "原始数据:" << QJsonDocument(data).toJson(QJsonDocument::Compact);

    // 检查是否有房间ID
    QString roomId = data["room_id"].toString();
    if (roomId.isEmpty() && m_userToRoom.contains(username)) {
        roomId = m_userToRoom[username];
    }

    if (roomId.isEmpty()) {
        qDebug() << "房间ID为空，无法处理";
        return;
    }

    qDebug() << "房间ID:" << roomId;

    if (m_gameRooms.contains(roomId)) {
        GameRoom &room = m_gameRooms[roomId];

        // 更新棋盘状态和分数
        QJsonArray board = data["board"].toArray();
        int score = data["score"].toInt();

        // 【添加调试】检查棋盘数据
        qDebug() << "棋盘数组大小:" << board.size();
        qDebug() << "分数:" << score;

        if (room.player1 == username) {
            room.player1Board = board;
            room.player1Score = score;
        } else if (room.player2 == username) {
            room.player2Board = board;
            room.player2Score = score;
        }

        // 转发给对手
        QString opponent = (room.player1 == username) ? room.player2 : room.player1;
        qDebug() << "对手:" << opponent;

        QTcpSocket *opponentSocket = m_usernameToSocket.value(opponent);

        if (!opponentSocket) {
            // 如果找不到，从客户端列表中查找
            for (QTcpSocket *sock : m_clients.keys()) {
                if (m_clients[sock].username == opponent) {
                    opponentSocket = sock;
                    break;
                }
            }
        }

        if (opponentSocket && opponentSocket->isValid()) {
            // 【关键修复】创建标准格式的消息
            QJsonObject forwardData;
            forwardData["type"] = "game_move";
            forwardData["room_id"] = roomId;
            forwardData["opponent"] = username;  // 发送者是当前用户
            forwardData["board"] = board;        // 棋盘数据在顶层
            forwardData["score"] = score;        // 分数在顶层
            forwardData["player"] = username;    // 添加player字段
            forwardData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

            // 添加调试信息
            qDebug() << "转发数据给对手:" << opponent;
            qDebug() << "转发数据大小:" << board.size();

            sendResponse(opponentSocket, "game_move", forwardData);
        } else {
            qDebug() << "找不到对手socket或socket无效:" << opponent;
        }
    } else {
        qDebug() << "房间不存在:" << roomId;
    }
}

// 实现handleGameStart函数
void ServerCore::handleGameStart(const QString &roomId)
{
    if (!m_gameRooms.contains(roomId)) return;

    GameRoom &room = m_gameRooms[roomId];
    room.gameStarted = true;
    room.startTime = QDateTime::currentDateTime();

    // 发送开始信号给双方
    QTcpSocket *socket1 = m_usernameToSocket.value(room.player1);
    QTcpSocket *socket2 = m_usernameToSocket.value(room.player2);

    if (socket1 && socket2) {
        QJsonObject startMsg;
        startMsg["room_id"] = roomId;
        startMsg["opponent"] = room.player2;
        startMsg["start_time"] = room.startTime.toString(Qt::ISODate);
        startMsg["game_mode"] = "闪电";
        startMsg["duration"] = 180; // 3分钟

        sendResponse(socket1, "game_start", startMsg);

        startMsg["opponent"] = room.player1;
        sendResponse(socket2, "game_start", startMsg);

        // 设置游戏结束定时器（3分钟）
        room.timer = new QTimer(this);
        room.timer->setSingleShot(true);
        room.timer->setInterval(180000); // 3分钟 = 180000毫秒

        connect(room.timer, &QTimer::timeout, this, [this, roomId]() {
            if (m_gameRooms.contains(roomId)) {
                m_gameRooms[roomId].gameEnded = true;
                handleGameEnd(roomId);
            }
        });

        room.timer->start();

        emit logMessage(QString("游戏房间 %1 开始: %2 vs %3").arg(roomId).arg(room.player1).arg(room.player2),
                        QColor("#4CAF50"));
        emit matchStarted(room.player1, room.player2);

        // 更新用户状态
        if (m_clients.contains(socket1)) {
            m_clients[socket1].status = "联机游戏中";
            m_clients[socket1].gameMode = "闪电";
        }
        if (m_clients.contains(socket2)) {
            m_clients[socket2].status = "联机游戏中";
            m_clients[socket2].gameMode = "闪电";
        }

        broadcastOnlineList();
    }
}

// 实现handleGameEnd函数
void ServerCore::handleGameEnd(const QString &roomId)
{
    if (!m_gameRooms.contains(roomId)) return;

    GameRoom &room = m_gameRooms[roomId];

    // 防止重复处理
    if (room.gameEnded) return;

    room.gameEnded = true;

    // 计算持续时间
    int duration = 180;
    if (room.startTime.isValid()) {
        duration = qMin(180, (int)room.startTime.secsTo(QDateTime::currentDateTime()));
    }

    // 确定胜者
    QString winner;
    if (room.player1Score > room.player2Score) {
        winner = room.player1;
    } else if (room.player2Score > room.player1Score) {
        winner = room.player2;
    } else {
        winner = "draw"; // 平局
    }

    // 发送结束信号给双方
    QTcpSocket *socket1 = m_usernameToSocket.value(room.player1);
    QTcpSocket *socket2 = m_usernameToSocket.value(room.player2);

    if (socket1 || socket2) {
        QJsonObject endMsg;
        endMsg["room_id"] = roomId;
        endMsg["player1_score"] = room.player1Score;
        endMsg["player2_score"] = room.player2Score;
        endMsg["winner"] = winner;
        endMsg["duration"] = duration;
        endMsg["game_mode"] = "闪电";
        endMsg["end_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        if (socket1) {
            sendResponse(socket1, "game_end", endMsg);
        }
        if (socket2) {
            sendResponse(socket2, "game_end", endMsg);
        }

        // 保存对战记录到数据库
        Database db;
        db.saveOnlineMatch(room.player1, room.player2,
                           room.player1Score, room.player2Score,
                           winner);

        emit logMessage(QString("游戏房间 %1 结束: %2 %3 vs %4 %5, 胜者: %6")
                            .arg(roomId)
                            .arg(room.player1).arg(room.player1Score)
                            .arg(room.player2).arg(room.player2Score)
                            .arg(winner == "draw" ? "平局" : winner),
                        QColor("#FF9800"));
        emit matchEnded(room.player1, room.player2);
    }

    // 延迟清理房间（给客户端处理时间）
    QTimer::singleShot(5000, this, [this, roomId]() {
        cleanupRoom(roomId);
    });
}

// 实现cleanupRoom函数
void ServerCore::cleanupRoom(const QString &roomId)
{
    if (m_gameRooms.contains(roomId)) {
        GameRoom &room = m_gameRooms[roomId];

        // 移除用户与房间的映射
        m_userToRoom.remove(room.player1);
        m_userToRoom.remove(room.player2);

        // 删除定时器
        if (room.timer) {
            room.timer->stop();
            room.timer->deleteLater();
        }

        // 更新用户状态为在线
        QTcpSocket *socket1 = m_usernameToSocket.value(room.player1);
        QTcpSocket *socket2 = m_usernameToSocket.value(room.player2);

        if (socket1 && m_clients.contains(socket1)) {
            m_clients[socket1].status = "在线";
            m_clients[socket1].gameMode = "空闲";
        }
        if (socket2 && m_clients.contains(socket2)) {
            m_clients[socket2].status = "在线";
            m_clients[socket2].gameMode = "空闲";
        }

        m_gameRooms.remove(roomId);

        // 广播在线列表更新
        broadcastOnlineList();

        emit logMessage(QString("游戏房间 %1 已清理").arg(roomId), QColor("#FF9800"));
    }
}

// 实现cleanupExpiredRooms函数
void ServerCore::cleanupExpiredRooms()
{
    QDateTime now = QDateTime::currentDateTime();
    QList<QString> roomsToRemove;

    for (auto it = m_gameRooms.begin(); it != m_gameRooms.end(); ++it) {
        GameRoom &room = it.value();

        // 清理已结束超过5分钟的房间
        if (room.gameEnded && room.startTime.secsTo(now) > 300) {
            roomsToRemove.append(it.key());
        }
        // 清理未开始但创建超过2分钟的房间
        else if (!room.gameStarted && room.startTime.secsTo(now) > 120) {
            roomsToRemove.append(it.key());
        }
        // 清理长时间没有活动的房间（比如玩家断开连接）
        else if (room.gameStarted && !room.gameEnded &&
                 room.startTime.secsTo(now) > 180 + 60) { // 游戏时间+1分钟缓冲
            roomsToRemove.append(it.key());
        }
    }

    for (const QString &roomId : roomsToRemove) {
        cleanupRoom(roomId);
    }
}

void ServerCore::processChatMessage(QTcpSocket *socket, const QJsonObject &data)
{
    if (!m_clients.contains(socket)) return;

    QString from = m_clients[socket].username;
    QString to = data["to"].toString();
    QString message = data["message"].toString();

    QJsonObject chatMsg;
    chatMsg["from"] = from;
    chatMsg["message"] = message;
    chatMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (to == "all") {
        // 广播给所有在线用户
        for (QTcpSocket *clientSocket : m_clients.keys()) {
            if (clientSocket != socket) {
                sendResponse(clientSocket, "chat", chatMsg);
            }
        }
        emit logMessage(QString("聊天广播: %1").arg(from), Qt::cyan);
    } else {
        // 私聊给指定用户
        if (m_usernameToSocket.contains(to)) {
            QTcpSocket *toSocket = m_usernameToSocket[to];
            sendResponse(toSocket, "chat", chatMsg);

            // 也发回给自己（确认发送）
            sendResponse(socket, "chat", chatMsg);

            emit logMessage(QString("私聊: %1 -> %2").arg(from).arg(to), Qt::cyan);
        }
    }
}

void ServerCore::processHeartbeat(QTcpSocket *socket, const QJsonObject &data)
{
    // 更新客户端活跃时间
    if (m_clients.contains(socket)) {
        QJsonObject response;
        response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        sendResponse(socket, "heartbeat_ack", response);
    }
}

// servercore.cpp - 修复processUserStatus函数
void ServerCore::processUserStatus(QTcpSocket *socket, const QJsonObject &data){
    if (!m_clients.contains(socket)) return;

    ClientInfo &info = m_clients[socket];
    QString username = info.username;

    if (username.isEmpty()) {
        qDebug() << "用户状态更新失败：用户未登录";
        return;
    }

    QString status = data["status"].toString();
    QString gameMode = data["game_mode"].toString();
    QString opponent = data["opponent"].toString();

    // 更新客户端信息
    info.status = status;
    if (!gameMode.isEmpty()) {
        info.gameMode = gameMode;
    }

    qDebug() << "用户状态更新:" << username << "->" << status << "模式:" << gameMode;

    // 发射消息接收信号，让ServerWindow可以处理
    emit messageReceived(username, "user_status", data);

    // 记录日志
    emit logMessage(QString("用户状态: %1 -> %2 (模式: %3)").arg(username).arg(status).arg(gameMode),
                    QColor("#2196F3"));

    // 广播在线列表更新
    broadcastOnlineList();
}

// 修改 sendResponse 函数，确保格式正确
void ServerCore::sendResponse(QTcpSocket *socket, const QString &type, const QJsonObject &data)
{
    if (!socket || !socket->isValid()) return;

    QJsonObject response;
    response["type"] = type;
    response["data"] = data;
    response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonDocument doc(response);
    socket->write(doc.toJson());

    socket->flush();
}

// servercore.cpp - 修改broadcastOnlineList函数以包含更多信息
void ServerCore::broadcastOnlineList()
{
    QJsonObject onlineList;
    QJsonArray usersArray;

    for (const ClientInfo &info : m_clients.values()) {
        if (!info.username.isEmpty()) {
            QJsonObject userObj;
            userObj["username"] = info.username;
            userObj["status"] = info.status;
            userObj["game_mode"] = info.gameMode;
            userObj["connect_time"] = info.connectTime.toString(Qt::ISODate);
            userObj["ip_address"] = info.ipAddress;

            usersArray.append(userObj);
        }
    }

    onlineList["users"] = usersArray;
    onlineList["count"] = usersArray.size();
    onlineList["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 广播给所有已登录用户
    for (QTcpSocket *socket : m_clients.keys()) {
        if (!m_clients[socket].username.isEmpty()) {
            sendResponse(socket, "online_list", onlineList);
        }
    }
}

// 修改析构函数
ServerCore::~ServerCore()
{
    stopServer();

    // 清理所有定时器
    if (m_roomCleanupTimer) {
        m_roomCleanupTimer->stop();
        delete m_roomCleanupTimer;
    }

    // 清理所有房间的定时器
    for (auto it = m_gameRooms.begin(); it != m_gameRooms.end(); ++it) {
        if (it.value().timer) {
            it.value().timer->stop();
            delete it.value().timer;
        }
    }
}
