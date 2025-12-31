// networkmanager.cpp - 实现matchResponse处理
#include "networkmanager.h"
#include <QHostAddress>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>

NetworkManager* NetworkManager::m_instance = nullptr;

NetworkManager* NetworkManager::instance()
{
    if (!m_instance) {
        m_instance = new NetworkManager();
    }
    return m_instance;
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_serverIP("10.61.18.134")
    //, m_serverIP("127.0.0.1")
    , m_serverPort(12345)
    , m_isLoggedIn(false)
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &NetworkManager::onError);

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(30000); // 30秒发送一次心跳
    connect(m_heartbeatTimer, &QTimer::timeout, this, &NetworkManager::onHeartbeatTimeout);
}

NetworkManager::~NetworkManager()
{
    disconnectFromServer();
}

bool NetworkManager::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    m_socket->connectToHost(m_serverIP, m_serverPort);
    return m_socket->waitForConnected(3000);
}

void NetworkManager::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        if (m_isLoggedIn) {
            logout();
        }
        m_socket->disconnectFromHost();
    }
    m_heartbeatTimer->stop();
    m_isLoggedIn = false;
    m_username.clear();
}

bool NetworkManager::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetworkManager::login(const QString &username, const QString &password)
{
    if (!isConnected()) {
        if (!connectToServer()) {
            emit loginResult(false, "无法连接到服务器", QJsonObject());
            return;
        }
    }

    QJsonObject loginMsg;
    loginMsg["type"] = "login";
    loginMsg["username"] = username;
    loginMsg["password"] = password;

    sendJson(loginMsg);
}

void NetworkManager::logout()
{
    if (m_isLoggedIn && isConnected()) {
        QJsonObject logoutMsg;
        logoutMsg["type"] = "logout";
        logoutMsg["username"] = m_username;

        sendJson(logoutMsg);
    }

    m_isLoggedIn = false;
    m_username.clear();
    m_heartbeatTimer->stop();
}

void NetworkManager::requestOnlineList()
{
    if (m_isLoggedIn && isConnected()) {
        QJsonObject requestMsg;
        requestMsg["type"] = "get_online_list";

        sendJson(requestMsg);
    }
}

void NetworkManager::sendMatchRequest(const QString &gameMode)
{
    if (isConnected()) {
        QJsonObject matchMsg;
        matchMsg["type"] = "match_request";
        matchMsg["mode"] = gameMode;

        sendJson(matchMsg);
    }
}

void NetworkManager::cancelMatchRequest()
{
    if (isConnected()) {
        QJsonObject cancelMsg;
        cancelMsg["type"] = "cancel_match";

        sendJson(cancelMsg);
    }
}

void NetworkManager::sendChatMessage(const QString &to, const QString &message)
{
    if (m_isLoggedIn && isConnected()) {
        QJsonObject chatMsg;
        chatMsg["type"] = "chat";
        chatMsg["to"] = to;
        chatMsg["message"] = message;

        sendJson(chatMsg);
    }
}

void NetworkManager::sendGameMove(const QString &opponent, const QJsonObject &moveData)
{
    if (m_isLoggedIn && isConnected()) {
        QJsonObject gameMsg;
        gameMsg["type"] = "game_move";
        gameMsg["opponent"] = opponent;
        gameMsg["data"] = moveData;

        sendJson(gameMsg);
    }
}

void NetworkManager::sendHeartbeat()
{
    if (m_isLoggedIn && isConnected()) {
        QJsonObject heartbeatMsg;
        heartbeatMsg["type"] = "heartbeat";
        heartbeatMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        sendJson(heartbeatMsg);
    }
}

void NetworkManager::requestUserStatusUpdate(const QString &status,
                                             const QString &gameMode,
                                             const QString &opponent)
{
    if (m_isLoggedIn && isConnected()) {
        QJsonObject statusMsg;
        statusMsg["type"] = "user_status";
        statusMsg["status"] = status;
        statusMsg["game_mode"] = gameMode;
        statusMsg["opponent"] = opponent;

        sendJson(statusMsg);
    }
}

void NetworkManager::setServerAddress(const QString &ip, int port)
{
    m_serverIP = ip;
    m_serverPort = port;

    // 如果已经连接，断开重新连接
    if (isConnected()) {
        disconnectFromServer();
    }
}

void NetworkManager::onConnected()
{
    qDebug() << "已连接到服务器:" << m_serverIP << ":" << m_serverPort;
    emit connected();
}

void NetworkManager::onDisconnected()
{
    qDebug() << "与服务器断开连接";
    m_isLoggedIn = false;
    m_username.clear();
    m_heartbeatTimer->stop();
    emit disconnected();
}

// networkmanager.cpp - 修改onReadyRead函数
void NetworkManager::onReadyRead()
{
    QByteArray rawData = m_socket->readAll();

    // 尝试分割多个JSON消息（有些服务器可能一次发送多个消息）
    QString dataStr = QString::fromUtf8(rawData);
    QStringList jsonMessages;

    // 简单的JSON消息分割逻辑（按大括号匹配）
    int braceCount = 0;
    QString currentMessage;

    for (int i = 0; i < dataStr.length(); i++) {
        QChar ch = dataStr[i];
        currentMessage.append(ch);

        if (ch == '{') {
            braceCount++;
        } else if (ch == '}') {
            braceCount--;
            if (braceCount == 0) {
                jsonMessages.append(currentMessage);
                currentMessage.clear();
            }
        }
    }

    // 如果有未闭合的消息，也尝试处理
    if (!currentMessage.isEmpty() && jsonMessages.isEmpty()) {
        jsonMessages.append(dataStr);
    }

    for (const QString &jsonStr : jsonMessages) {
        if (jsonStr.trimmed().isEmpty()) continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError) {
            qDebug() << "JSON解析错误:" << error.errorString()
                << "位置:" << error.offset
                << "内容:" << jsonStr;

            // 尝试清理可能的垃圾数据
            QString cleanedStr = jsonStr;
            // 移除可能的空白字符
            cleanedStr = cleanedStr.trimmed();

            // 再次尝试解析
            doc = QJsonDocument::fromJson(cleanedStr.toUtf8(), &error);
            if (error.error != QJsonParseError::NoError) {
                qDebug() << "第二次解析失败，跳过此消息";
                continue;
            }
        }

        if (!doc.isObject()) {
            qDebug() << "无效的JSON格式（不是对象）";
            continue;
        }

        QJsonObject message = doc.object();
        processMessage(message);
    }
}

void NetworkManager::onError(QAbstractSocket::SocketError socketError)
{
    QString errorMsg;
    switch (socketError) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMsg = "连接被拒绝";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorMsg = "远程主机关闭连接";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMsg = "找不到服务器";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMsg = "连接超时";
        break;
    default:
        errorMsg = m_socket->errorString();
        break;
    }

    emit connectionError(errorMsg);
    qDebug() << "网络错误:" << errorMsg;
}

void NetworkManager::onHeartbeatTimeout()
{
    sendHeartbeat();
}

void NetworkManager::processMessage(const QJsonObject &message)
{
    QString type = message["type"].toString();
    QJsonObject data = message["data"].toObject();

    if (type == "welcome") {
        qDebug() << "服务器欢迎消息:" << data["message"].toString();
        emit welcomeMessage(data["message"].toString());
    }
    else if (type == "login_result") {
        bool success = data["success"].toBool();
        QString msg = data["message"].toString();

        if (success) {
            m_isLoggedIn = true;
            m_username = data["username"].toString();
            m_heartbeatTimer->start();

            // 请求在线列表
            requestOnlineList();
        } else {
            m_isLoggedIn = false;
            m_username.clear();
        }

        QJsonObject userData = data["user_data"].toObject();
        emit loginResult(success, msg, userData);
    }
    else if (type == "online_list") {
        QJsonArray users = data["users"].toArray();
        emit onlineListUpdated(users);
    }
    else if (type == "match_response") {
        bool success = data["success"].toBool();
        QString msg = data["message"].toString();
        int queuePosition = data["queue_position"].toInt(-1);

        // 新增：发射matchResponse信号
        emit matchResponse(success, msg, queuePosition);

        if (success && queuePosition > 0) {
            emit matchQueued(queuePosition);
        }
    }
    else if (type == "match_found") {
        QString player1 = data["player1"].toString();
        QString player2 = data["player2"].toString();
        QString roomId = data["room_id"].toString();  // 确保解析房间ID

        qDebug() << "收到匹配成功消息 - 玩家1:" << player1
                 << "玩家2:" << player2 << "房间:" << roomId;

        emit matchFound(player1, player2, roomId);  // 传递房间ID
    }
    else if (type == "match_cancelled") {
        emit matchCancelled();
    }
    else if (type == "chat") {
        QString from = data["from"].toString();
        QString messageText = data["message"].toString();
        QString timestamp = data["timestamp"].toString();
        emit chatMessageReceived(from, messageText, timestamp);
    }
    else if (type == "game_move") {
        emit gameMoveReceived(message);
        qDebug() << "处理game_move消息";
    }
    else if (type == "heartbeat_ack") {
        // 心跳确认，无需处理
    }
    else if (type == "system") {
        QString sysMsg = data["message"].toString();
        emit systemMessage(sysMsg);
    }
    else if (type == "game_start") {
        QString roomId = data["room_id"].toString();
        qDebug() << "收到游戏开始消息，房间ID:" << roomId;
        emit gameStartReceived(data);
    }
    else if (type == "game_end") {
        emit gameEndReceived(data);
    }
    else if (type == "opponent_ready") {
        emit opponentReady(data);
    }
    else if (type == "player_quit") {  // 新增：处理玩家退出
        emit playerQuitReceived(data);
    }
    else if (type == "server_shutdown") {
        QString shutdownMsg = data["message"].toString();
        emit serverShutdown(shutdownMsg);
    }

    // 转发原始消息
    emit serverMessage(type, data);
}

void NetworkManager::sendJson(const QJsonObject &json)
{
    if (!isConnected()) {
        qDebug() << "发送失败：未连接到服务器";
        return;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    // 【添加调试信息】打印发送的JSON内容
    QString jsonStr = QString::fromUtf8(data);
    qDebug() << "发送JSON:" << jsonStr;

    m_socket->write(data);
}

// networkmanager.cpp - 实现状态更新方法
void NetworkManager::updateUserStatus(const QString &status,
                                      const QString &gameMode,
                                      const QString &opponent)
{
    if (isConnected()) {
        QJsonObject statusMsg;
        statusMsg["type"] = "user_status";
        statusMsg["status"] = status;
        statusMsg["game_mode"] = gameMode;
        statusMsg["opponent"] = opponent;
        statusMsg["username"] = m_username;

        sendJson(statusMsg);

        qDebug() << "发送状态更新:" << m_username << "->" << status << "模式:" << gameMode;
    } else {
        qDebug() << "无法发送状态更新: 未登录或未连接";
    }
}

// 添加新的发送方法
void NetworkManager::sendGameStart(const QString &roomId, const QJsonArray &board, int score)
{
    QJsonObject gameStartMsg;
    gameStartMsg["type"] = "game_start";
    gameStartMsg["room_id"] = roomId;
    gameStartMsg["board"] = board;
    gameStartMsg["score"] = score;
    gameStartMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    sendJson(gameStartMsg);
}

void NetworkManager::sendGameMove(const QString &roomId, const QJsonArray &board, int score)
{
    QJsonObject gameMoveMsg;
    gameMoveMsg["type"] = "game_move";
    gameMoveMsg["room_id"] = roomId;
    gameMoveMsg["board"] = board;
    gameMoveMsg["score"] = score;
    gameMoveMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    sendJson(gameMoveMsg);
}

void NetworkManager::sendGameEnd(const QString &roomId, int finalScore)
{
    QJsonObject gameEndMsg;
    gameEndMsg["type"] = "game_end";
    gameEndMsg["room_id"] = roomId;
    gameEndMsg["final_score"] = finalScore;
    gameEndMsg["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    sendJson(gameEndMsg);
}

// 添加一个新的公共方法，用于发送任意JSON
void NetworkManager::sendRawJson(const QJsonObject &json)
{
    if (!isConnected()) {
        qDebug() << "未连接到服务器，无法发送消息";
        return;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QString jsonStr = QString::fromUtf8(data);
    qDebug() << "发送原始JSON:" << jsonStr;

    m_socket->write(data);
    m_socket->flush();
}
