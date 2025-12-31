#ifndef SERVERCORE_H
#define SERVERCORE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QColor>
#include <QTimer>

struct ClientInfo {
    QString username;
    QString ipAddress;
    QDateTime connectTime;
    QString status;
    QString gameMode;
    QTcpSocket *socket;
};

struct GameRoom {
    QString roomId;
    QString player1;
    QString player2;
    QJsonArray player1Board;
    QJsonArray player2Board;
    int player1Score;
    int player2Score;
    bool player1Ready;
    bool player2Ready;
    bool gameStarted;
    bool gameEnded;
    QDateTime startTime;
    QTimer *timer;
};
class ServerCore : public QObject
{
    Q_OBJECT

public:
    explicit ServerCore(QObject *parent = nullptr);
    ~ServerCore();

    bool startServer(quint16 port);
    void stopServer();

signals:
    void logMessage(const QString &message, const QColor &color = Qt::white);
    void clientConnected();
    void clientDisconnected(const QString &username);
    void matchStarted(const QString &player1, const QString &player2);
    void matchEnded(const QString &player1, const QString &player2);
    void messageReceived(const QString &from, const QString &type, const QJsonObject &data);
    void userLoggedIn(const QString &username, const QString &ipAddress);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onReadyRead();

private:
    void processLogin(QTcpSocket *socket, const QJsonObject &data);
    void processMatchRequest(QTcpSocket *socket, const QJsonObject &data);
    void processGameMove(QTcpSocket *socket, const QJsonObject &data);
    void processChatMessage(QTcpSocket *socket, const QJsonObject &data);
    void processHeartbeat(QTcpSocket *socket, const QJsonObject &data);
    void processUserStatus(QTcpSocket *socket, const QJsonObject &data);
    void processGameStart(QTcpSocket *socket, const QJsonObject &data);
    void processGameEnd(QTcpSocket *socket, const QJsonObject &data);
    void processPlayerQuit(QTcpSocket *socket, const QJsonObject &data);  // 新增
    void sendResponse(QTcpSocket *socket, const QString &type, const QJsonObject &data = QJsonObject());
    void broadcastOnlineList();

    QHash<QString, GameRoom> m_gameRooms; // roomId -> GameRoom
    QHash<QString, QString> m_userToRoom; // username -> roomId
    void handleGameStart(const QString &roomId);
    void handleGameEnd(const QString &roomId);
    void broadcastRoomUpdate(const QString &roomId, const QString &excludeUser = "");
    void cleanupRoom(const QString &roomId);
    void cleanupExpiredRooms();

    QTcpServer *m_tcpServer;
    QHash<QTcpSocket*, ClientInfo> m_clients;
    QHash<QString, QTcpSocket*> m_usernameToSocket;
    QTimer *m_roomCleanupTimer;               // 房间清理定时器
};

#endif // SERVERCORE_H
