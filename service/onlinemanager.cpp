#include "onlinemanager.h"

OnlineManager::OnlineManager(QObject *parent)
    : QObject(parent)
{
}

void OnlineManager::addUser(const QString &username, const QString &ipAddress)
{
    OnlineUser user;
    user.username = username;
    user.ipAddress = ipAddress;
    user.connectTime = QDateTime::currentDateTime();
    user.lastActivity = QDateTime::currentDateTime();
    user.status = "在线";
    user.gameMode = "空闲";
    user.opponent = "";

    m_onlineUsers.insert(username, user);
}

// onlinemanager.cpp - 确保removeUser方法能正确移除用户
void OnlineManager::removeUser(const QString &username)
{
    if (m_onlineUsers.contains(username)) {
        m_onlineUsers.remove(username);
        qDebug() << "用户从在线列表移除:" << username;
    } else {
        qDebug() << "尝试移除不存在的用户:" << username;
    }
}

void OnlineManager::updateUserStatus(const QString &username, const QString &status,
                                     const QString &gameMode, const QString &opponent)
{
    if (m_onlineUsers.contains(username)) {
        OnlineUser &user = m_onlineUsers[username];
        user.status = status;
        if (!gameMode.isEmpty()) {
            user.gameMode = gameMode;
        }
        if (!opponent.isEmpty()) {
            user.opponent = opponent;
        }
        user.lastActivity = QDateTime::currentDateTime();
    }
}

QList<OnlineUser> OnlineManager::getOnlineUsers() const
{
    return m_onlineUsers.values();
}

int OnlineManager::getOnlineCount() const
{
    return m_onlineUsers.size();
}

void OnlineManager::updateActivity(const QString &username)
{
    if (m_onlineUsers.contains(username)) {
        m_onlineUsers[username].lastActivity = QDateTime::currentDateTime();
    }
}

void OnlineManager::cleanupInactiveUsers(int timeoutSeconds)
{
    QDateTime now = QDateTime::currentDateTime();
    QList<QString> toRemove;

    for (auto it = m_onlineUsers.begin(); it != m_onlineUsers.end(); ++it) {
        if (it->lastActivity.secsTo(now) > timeoutSeconds) {
            toRemove.append(it.key());
        }
    }

    for (const QString &username : toRemove) {
        m_onlineUsers.remove(username);
    }
}

bool OnlineManager::isUserOnline(const QString &username) const
{
    return m_onlineUsers.contains(username);
}

OnlineUser OnlineManager::getUserInfo(const QString &username) const
{
    if (m_onlineUsers.contains(username)) {
        return m_onlineUsers.value(username);
    }
    return OnlineUser();
}
