#include "database.h"
#include <QDebug>
#include <QJsonArray>

Database::Database(QObject *parent)
    : QObject(parent)
{
    m_db = QSqlDatabase::addDatabase("QMYSQL", "server_connection");
}

bool Database::connect()
{

    m_db.setHostName("rm-2zei160h77c07q797uo.mysql.rds.aliyuncs.com");   // 本地
    m_db.setPort(3306);
    m_db.setDatabaseName("game");    // 你的库名
    m_db.setUserName("root");        // 改成你的账号
    m_db.setPassword("62545743Hxb");      // 改成你的密码

    //m_db.setHostName("127.0.0.1");
    //m_db.setPort(3306);
    //m_db.setDatabaseName("game");
    //m_db.setUserName("bank");
    //m_db.setPassword("210507377Qq@");

    if (!m_db.open()) {
        qDebug() << "数据库连接失败:" << m_db.lastError().text();
        return false;
    }

    // 确保必要的表存在
    createTables();

    return true;
}

bool Database::createTables()
{
    QSqlQuery query(m_db);

    // 检查在线对战记录表
    QString createOnlineTable =
        "CREATE TABLE IF NOT EXISTS online_matches ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  player1 VARCHAR(50) NOT NULL,"
        "  player2 VARCHAR(50) NOT NULL,"
        "  player1_score INT NOT NULL,"
        "  player2_score INT NOT NULL,"
        "  winner VARCHAR(50),"
        "  match_time DATETIME NOT NULL,"
        "  duration INT,"
        "  mode VARCHAR(20)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    if (!query.exec(createOnlineTable)) {
        qDebug() << "创建online_matches表失败:" << query.lastError().text();
        return false;
    }

    // 检查用户表中的联机统计字段
    QString checkColumns =
        "SHOW COLUMNS FROM user LIKE 'online_wins'";

    if (!query.exec(checkColumns) || !query.next()) {
        // 添加联机统计字段
        QString addColumns =
            "ALTER TABLE user "
            "ADD COLUMN IF NOT EXISTS online_wins INT DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS online_losses INT DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS online_points INT DEFAULT 0";

        if (!query.exec(addColumns)) {
            qDebug() << "添加联机统计字段失败:" << query.lastError().text();
        }
    }

    return true;
}

// database.cpp - 修改verifyUser函数
bool Database::verifyUser(const QString &username, const QString &password)
{

    // 检查数据库连接
    if (!m_db.isOpen()) {
        qDebug() << "数据库未连接，尝试重新连接";
        if (!connect()) {
            qDebug() << "重新连接数据库失败";
            return false;
        }
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT password FROM user WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        return false;
    }

    if (!query.next()) {
        return false;
    }

    QString dbPassword = query.value(0).toString();
    bool result = (dbPassword == password);

    return result;
}

QJsonObject Database::getUserData(const QString &username)
{
    QJsonObject userData;

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT points, skill_points, online_wins, online_losses, online_points "
        "FROM user WHERE username = ?"
        );
    query.addBindValue(username);

    if (query.exec() && query.next()) {
        userData["points"] = query.value(0).toInt();
        userData["skill_points"] = query.value(1).toInt();
        userData["online_wins"] = query.value(2).toInt();
        userData["online_losses"] = query.value(3).toInt();
        userData["online_points"] = query.value(4).toInt();
    }

    return userData;
}

// 修改saveOnlineMatch函数，添加更多信息
bool Database::saveOnlineMatch(const QString &player1, const QString &player2,
                               int score1, int score2, const QString &winner)
{
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO online_matches "
        "(player1, player2, player1_score, player2_score, winner, match_time, duration, mode) "
        "VALUES (?, ?, ?, ?, ?, NOW(), 180, '闪电')"
        );

    query.addBindValue(player1);
    query.addBindValue(player2);
    query.addBindValue(score1);
    query.addBindValue(score2);
    query.addBindValue(winner);

    if (!query.exec()) {
        qDebug() << "保存联机对战记录失败:" << query.lastError().text();
        return false;
    }

    // 更新用户统计
    bool isDraw = (winner == "draw");

    if (!isDraw) {
        QString winnerUsername = winner;
        QString loserUsername = (winner == player1) ? player2 : player1;
        int winnerScore = (winner == player1) ? score1 : score2;

        // 更新胜者数据
        QSqlQuery updateWinner(m_db);
        updateWinner.prepare(
            "UPDATE user SET online_wins = online_wins + 1, "
            "online_points = online_points + ? WHERE username = ?"
            );
        updateWinner.addBindValue(winnerScore / 10);
        updateWinner.addBindValue(winnerUsername);
        updateWinner.exec();

        // 更新败者数据
        QSqlQuery updateLoser(m_db);
        updateLoser.prepare(
            "UPDATE user SET online_losses = online_losses + 1 "
            "WHERE username = ?"
            );
        updateLoser.addBindValue(loserUsername);
        updateLoser.exec();
    } else {
        // 平局处理
        QSqlQuery update1(m_db);
        update1.prepare(
            "UPDATE user SET online_points = online_points + ? WHERE username = ?"
            );
        update1.addBindValue(score1 / 20); // 平局积分减半
        update1.addBindValue(player1);
        update1.exec();

        QSqlQuery update2(m_db);
        update2.prepare(
            "UPDATE user SET online_points = online_points + ? WHERE username = ?"
            );
        update2.addBindValue(score2 / 20);
        update2.addBindValue(player2);
        update2.exec();
    }

    return true;
}

QJsonObject Database::getRankings(int limit)
{
    QJsonObject result;
    QJsonArray rankings;

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT username, points, online_wins, online_losses, online_points "
        "FROM user ORDER BY online_points DESC LIMIT ?"
        );
    query.addBindValue(limit);

    if (query.exec()) {
        int rank = 1;
        while (query.next()) {
            QJsonObject player;
            player["rank"] = rank++;
            player["username"] = query.value(0).toString();
            player["points"] = query.value(1).toInt();
            player["online_wins"] = query.value(2).toInt();
            player["online_losses"] = query.value(3).toInt();
            player["online_points"] = query.value(4).toInt();

            rankings.append(player);
        }
    }

    result["rankings"] = rankings;
    result["total"] = rankings.size();

    return result;
}

Database::~Database()
{
    disconnect();
}

void Database::disconnect()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool Database::registerUser(const QString &username, const QString &password)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO user(username, password) VALUES(?, ?)");
    query.addBindValue(username);
    query.addBindValue(password);

    return query.exec();
}

bool Database::updateUserStats(const QString &username, int score, const QString &mode)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE user SET points = points + ? WHERE username = ?");
    query.addBindValue(score);
    query.addBindValue(username);

    return query.exec();
}

QJsonObject Database::getMatchHistory(const QString &username, int limit)
{
    QJsonObject result;
    QJsonArray matches;

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT player1, player2, player1_score, player2_score, winner, match_time "
        "FROM online_matches "
        "WHERE player1 = ? OR player2 = ? "
        "ORDER BY match_time DESC LIMIT ?"
        );
    query.addBindValue(username);
    query.addBindValue(username);
    query.addBindValue(limit);

    if (query.exec()) {
        while (query.next()) {
            QJsonObject match;
            match["player1"] = query.value(0).toString();
            match["player2"] = query.value(1).toString();
            match["player1_score"] = query.value(2).toInt();
            match["player2_score"] = query.value(3).toInt();
            match["winner"] = query.value(4).toString();
            match["match_time"] = query.value(5).toString();

            matches.append(match);
        }
    }

    result["matches"] = matches;
    result["username"] = username;

    return result;
}
