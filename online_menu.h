// online_menu.h
#ifndef ONLINE_MENU_H
#define ONLINE_MENU_H

#include <QWidget>
#include <QTimer>
#include <QJsonArray>
#include "networkmanager.h"  // 新增

namespace Ui {
class OnlineMenu;
}

class OnlineMenu : public QWidget
{
    Q_OBJECT

public:
    explicit OnlineMenu(QWidget *parent = nullptr);
    ~OnlineMenu();

signals:
    void matchRequested(const QString &gameMode);           // 请求匹配（携带游戏模式）
    void cancelMatchRequested();     // 取消匹配
    void backRequested();            // 返回菜单

private slots:
    void on_btnStartMatch_clicked(); // 开始匹配
    void on_btnCancelMatch_clicked(); // 取消匹配
    void onMatchTimerTick();         // 匹配计时器更新

    // 网络事件处理
    void onMatchResponse(bool success, const QString &message, int queuePosition);
    void onMatchQueued(int queuePosition);
    void onMatchFound(const QString &player1, const QString &player2);
    void onMatchCancelled();
    void onOnlineListUpdated(const QJsonArray &users);

private:
    Ui::OnlineMenu *ui;
    QTimer *m_matchTimer;            // 匹配计时器
    int m_matchSeconds;              // 匹配时长（秒）
    bool m_isMatching;               // 是否正在匹配中

    NetworkManager *m_networkManager; // 网络管理器

    void updateMatchStatus(const QString &status);
    void updateQueuePosition(int position);
    void updateOnlineUserCount(int count);
};

#endif // ONLINE_MENU_H
