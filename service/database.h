#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QJsonObject>

class Database : public QObject
{
    Q_OBJECT

public:
    Database(QObject *parent = nullptr);
    ~Database();

    bool connect();
    void disconnect();

    bool verifyUser(const QString &username, const QString &password);
    QJsonObject getUserData(const QString &username);

    bool registerUser(const QString &username, const QString &password);
    bool updateUserStats(const QString &username, int score, const QString &mode);

    QJsonObject getRankings(int limit = 10);
    QJsonObject getMatchHistory(const QString &username, int limit = 20);

    bool saveOnlineMatch(const QString &player1, const QString &player2,
                         int score1, int score2, const QString &winner);

private:
    QSqlDatabase m_db;
    bool createTables();
};

#endif // DATABASE_H
