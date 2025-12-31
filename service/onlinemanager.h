#ifndef ONLINEMANAGER_H
#define ONLINEMANAGER_H

#include <QObject>
#include <QHash>
#include <QDateTime>

struct OnlineUser {
    QString username;
    QString ipAddress;
    QDateTime connectTime;
    QDateTime lastActivity;
    QString status;  // 在线、游戏中、匹配中、离线
    QString gameMode;
    QString opponent;
};

class OnlineManager : public QObject
{
    Q_OBJECT

public:
    explicit OnlineManager(QObject *parent = nullptr);

    void addUser(const QString &username, const QString &ipAddress);
    void removeUser(const QString &username);
    void updateUserStatus(const QString &username, const QString &status,
                          const QString &gameMode = "", const QString &opponent = "");

    QList<OnlineUser> getOnlineUsers() const;
    int getOnlineCount() const;

    void updateActivity(const QString &username);
    void cleanupInactiveUsers(int timeoutSeconds = 60);

    bool isUserOnline(const QString &username) const;
    OnlineUser getUserInfo(const QString &username) const;

private:
    QHash<QString, OnlineUser> m_onlineUsers;
};

#endif // ONLINEMANAGER_H
