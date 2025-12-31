#ifndef RANKPAGE_H
#define RANKPAGE_H

#include <QWidget>
#include <QTableWidget>
#include "rankmanager.h"

namespace Ui {
class RankPage;
}

class RankPage : public QWidget
{
    Q_OBJECT

public:
    explicit RankPage(const QString& username, QWidget *parent = nullptr);
    ~RankPage() override;

    //设置用户名方法
    void setUsername(const QString& username) { m_username = username; refreshRankings(); }

signals:
    void backToMenu();

public:
    void refreshRankings();

private slots:
    void on_backButton_clicked();

private:
    Ui::RankPage *ui;
    RankManager* m_rankManager;
    QString m_username;

    void initTable(QTableWidget* table, int columnCount);
    void fillModeTable(QTableWidget* table, const QVector<RankInfo>& records);
    void fillPersonalTable(const QVector<RankInfo>& records);
    void setTableHeaders();  // 新增方法
};

#endif // RANKPAGE_H
