// networkmanager.h - 添加matchResponse信号
#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    static NetworkManager* instance();

    bool connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

    void login(const QString &username, const QString &password);
    void logout();
    void requestOnlineList();
    void sendMatchRequest(const QString &gameMode);
    void cancelMatchRequest();
    void sendChatMessage(const QString &to, const QString &message);
    void sendGameMove(const QString &opponent, const QJsonObject &moveData);
    void sendHeartbeat();
    void requestUserStatusUpdate(const QString &status,
                                 const QString &gameMode = "",
                                 const QString &opponent = "");
    void updateUserStatus(const QString &status,
                          const QString &gameMode = "",
                          const QString &opponent = "");
    QString getUsername() const { return m_username; }
    QString getServerIP() const { return m_serverIP; }
    int getServerPort() const { return m_serverPort; }
    void setServerAddress(const QString &ip, int port);

    void sendGameStart(const QString &roomId, const QJsonArray &board, int score);
    void sendGameMove(const QString &roomId, const QJsonArray &board, int score);
    void sendGameEnd(const QString &roomId, int finalScore);
    void sendRawJson(const QJsonObject &json);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &error);

    void loginResult(bool success, const QString &message, const QJsonObject &userData);
    void onlineListUpdated(const QJsonArray &users);
    void matchResponse(bool success, const QString &message, int queuePosition = -1); // 新增
    void matchQueued(int queuePosition);
    void matchFound(const QString &player1, const QString &player2, const QString &roomId);  // 添加roomId
    void matchCancelled();
    void chatMessageReceived(const QString &from, const QString &message, const QString &timestamp);
    void gameMoveReceived(const QJsonObject &moveData);
    void serverMessage(const QString &type, const QJsonObject &data);
    void systemMessage(const QString &message);
    void serverShutdown(const QString &message);
    void welcomeMessage(const QString &message);

    void gameStartReceived(const QJsonObject &data);          // 游戏开始信号
    void gameUpdateReceived(const QJsonObject &data);        // 游戏更新信号
    void gameEndReceived(const QJsonObject &data);           // 游戏结束信号
    void opponentReady(const QJsonObject &data);            // 对手准备就
    void playerQuitReceived(const QJsonObject &data);  // 新增：玩家退出信号

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);
    void onHeartbeatTimeout();

private:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    void processMessage(const QJsonObject &message);
    void sendJson(const QJsonObject &json);

    QTcpSocket *m_socket;
    QTimer *m_heartbeatTimer;
    QString m_username;
    QString m_serverIP;
    int m_serverPort;
    bool m_isLoggedIn;

    static NetworkManager* m_instance;
};

#endif // NETWORKMANAGER_H
