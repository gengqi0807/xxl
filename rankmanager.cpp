#include "rankmanager.h"
#include <QSqlError>
#include <QDebug>
#include <QSqlQuery>
#include <QVariant>

RankManager::RankManager(QObject *parent) : QObject(parent)
{
    m_db = QSqlDatabase::database();

    // 检查数据库连接
    if (!m_db.isOpen()) {
        qWarning() << "RankManager: 数据库连接未打开";
    } else {
        qDebug() << "RankManager: 数据库连接正常";
    }
}

RankManager::~RankManager()
{
}

bool RankManager::addRecord(const QString &username, int score, const QString &mode)
{
    if (!m_db.isOpen()) {
        qWarning() << "RankManager::addRecord: 数据库未连接";
        return false;
    }

    //允许分数为0的记录
    if (score < 0) return false;

    qDebug() << "=== 正在保存游戏记录 ===";
    qDebug() << "用户名:" << username;
    qDebug() << "分数:" << score;
    qDebug() << "模式:" << mode;
    qDebug() << "时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QSqlQuery query;
    query.prepare("INSERT INTO game_records (username, mode, score, time) "
                  "VALUES (:username, :mode, :score, :time)");
    query.bindValue(":username", username);
    query.bindValue(":mode", mode);
    query.bindValue(":score", score);
    query.bindValue(":time", QDateTime::currentDateTime());

    bool success = query.exec();

    if (success) {
        qDebug() << "游戏记录保存成功";
        qDebug() << "插入的ID:" << query.lastInsertId().toInt();
    } else {
        qWarning() << "游戏记录保存失败:" << query.lastError().text();
        qWarning() << "执行的SQL:" << query.lastQuery();
        qWarning() << "绑定值: username=" << username
                   << ", mode=" << mode
                   << ", score=" << score;
    }

    return success;
}

QVector<RankInfo> RankManager::getTopRecords(int count, const QString &mode)
{
    QVector<RankInfo> result;
    if (!m_db.isOpen()) {
        qWarning() << "RankManager::getTopRecords: 数据库未连接";
        return result;
    }

    QString sql = "SELECT username, score, mode, time FROM game_records WHERE 1=1 ";

    if (!mode.isEmpty()) {
        sql += "AND mode = :mode ";
    }
    sql += "ORDER BY score DESC LIMIT :count";

    QSqlQuery query;
    query.prepare(sql);
    if (!mode.isEmpty()) {
        query.bindValue(":mode", mode);
    }
    query.bindValue(":count", count);

    qDebug() << "=== 获取排行榜数据 ===";
    qDebug() << "模式:" << (mode.isEmpty() ? "全部" : mode);
    qDebug() << "查询SQL:" << sql;

    if (query.exec()) {
        int rank = 1;
        int recordCount = 0;
        while (query.next()) {
            RankInfo info;
            info.username = query.value(0).toString();
            info.score = query.value(1).toInt();
            info.mode = query.value(2).toString();
            info.time = query.value(3).toDateTime();
            info.rank = rank++;

            qDebug() << "记录" << recordCount++ << ":"
                     << "用户=" << info.username
                     << "分数=" << info.score
                     << "模式=" << info.mode
                     << "时间=" << info.time.toString("yyyy-MM-dd hh:mm:ss");

            result.append(info);
        }
        qDebug() << "共获取" << recordCount << "条记录";
    } else {
        qWarning() << "获取排行榜记录失败:" << query.lastError().text();
        qWarning() << "执行的SQL:" << query.lastQuery();
    }

    return result;
}

QVector<RankInfo> RankManager::getUserRecords(const QString &username, int count)
{
    QVector<RankInfo> result;
    if (!m_db.isOpen()) {
        qWarning() << "RankManager::getUserRecords: 数据库未连接";
        return result;
    }

    QSqlQuery query;
    query.prepare("SELECT username, score, mode, time FROM game_records "
                  "WHERE username = :username "
                  "ORDER BY time DESC LIMIT :count");
    query.bindValue(":username", username);
    query.bindValue(":count", count);

    qDebug() << "=== 获取用户个人记录 ===";
    qDebug() << "用户:" << username;
    qDebug() << "查询SQL:" << query.lastQuery();

    if (query.exec()) {
        int recordCount = 0;
        while (query.next()) {
            RankInfo info;
            info.username = query.value(0).toString();
            info.score = query.value(1).toInt();
            info.mode = query.value(2).toString();
            info.time = query.value(3).toDateTime();
            info.rank = -1;

            qDebug() << "个人记录" << recordCount++ << ":"
                     << "分数=" << info.score
                     << "模式=" << info.mode
                     << "时间=" << info.time.toString("yyyy-MM-dd hh:mm:ss");

            result.append(info);
        }
        qDebug() << "共获取" << recordCount << "条个人记录";
    } else {
        qWarning() << "获取用户记录失败:" << query.lastError().text();
    }

    return result;
}

int RankManager::getUserRank(const QString &username, const QString &mode)
{
    if (!m_db.isOpen()) return -1;

    QString sql = "SELECT COUNT(*) + 1 FROM game_records WHERE score > ("
                  "SELECT MAX(score) FROM game_records WHERE username = :username ";
    if (!mode.isEmpty()) {
        sql += "AND mode = :mode";
    }
    sql += ")";

    if (!mode.isEmpty()) {
        sql += " AND mode = :mode2";
    }

    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":username", username);
    if (!mode.isEmpty()) {
        query.bindValue(":mode", mode);
        query.bindValue(":mode2", mode);
    }

    if (query.exec() && query.next()) {
        int rank = query.value(0).toInt();
        qDebug() << "用户" << username << "在模式" << mode << "中的排名:" << rank;
        return rank;
    }

    return -1;
}

bool RankManager::initTable()
{
    //检查表是否存在
    if (!m_db.isOpen()) return false;

    QSqlQuery query;
    if (query.exec("SELECT COUNT(*) FROM game_records")) {
        query.next();
        qDebug() << "game_records表中现有记录数:" << query.value(0).toInt();
    }

    return true;
}
