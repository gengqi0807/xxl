#ifndef RANKMANAGER_H
#define RANKMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QVector>

struct RankInfo {
    QString username;
    int score;
    QString mode;
    QDateTime time;
    int rank;
};

class RankManager : public QObject
{
    Q_OBJECT
public:
    explicit RankManager(QObject *parent = nullptr);
    ~RankManager() override;

    bool addRecord(const QString& username, int score, const QString& mode);
    QVector<RankInfo> getTopRecords(int count, const QString& mode = "");
    QVector<RankInfo> getUserRecords(const QString& username, int count = 10);
    int getUserRank(const QString& username, const QString& mode = "");

private:
    QSqlDatabase m_db;
    bool initTable();
};

#endif // RANKMANAGER_H
