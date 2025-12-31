// menu.h
#ifndef MENU_H
#define MENU_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QJsonArray>
#include "networkmanager.h"
#include "musicmanager.h"

// 前向声明
class MainWindow;
class OnlineMenu;
class OnlineGame;
class MusicSetting;

namespace Ui {
class Menu;
}

class Menu : public QWidget
{
    Q_OBJECT

public:
    explicit Menu(QWidget *parent = nullptr);
    ~Menu();

private slots:
    void on_btnSingle_clicked();          // 左侧单机按钮
    void onModeSelected(QAbstractButton *btn); // 模式选中
    void on_btnOnline_clicked();           // 左侧联机对战按钮
    void on_btnSkill_clicked();           // 左侧技能选择按钮
    void on_btnRank_clicked();
    void on_btnRule_clicked();
    void on_btnSettings_clicked();

    // 网络相关槽函数
    void onLoginResult(bool success, const QString &message, const QJsonObject &userData);
    void onOnlineListUpdated(const QJsonArray &users);
    void onMatchResponse(bool success, const QString &message, int queuePosition);
    void onMatchQueued(int queuePosition);
    void onMatchFound(const QString &player1, const QString &player2);
    void onMatchCancelled();
    void onSystemMessage(const QString &message);
    void onServerShutdown(const QString &message);
    void onChatMessageReceived(const QString &from, const QString &message, const QString &timestamp);
    void onGameMoveReceived(const QJsonObject &moveData);

    // 联机对战相关槽函数
    void onMatchRequested(const QString &gameMode);
    void onCancelMatchRequested();
    void onStartOnlineGame(const QString &myUsername, const QString &opponentName);

signals:
    void startGameRequested(const QString &mode);   // 携带模式名
    void startAIDemoRequested(); // AI演示信号
    void startOnlineGameRequested();                 // 请求开始联机游戏

private:
    Ui::Menu *ui;
    QWidget *createModeSelectPage();      // 工厂函数：模式选择页
    QWidget *createOnlinePage();          // 工厂函数：联机对战页
    QWidget *createRulePage();            // 工厂函数：规则页

    QStackedWidget *rightStack;           // 快捷指针
    QButtonGroup *modeGroup;              // 三选一

    // 联机对战相关成员
    OnlineMenu *m_onlineMenu;
    OnlineGame *m_onlineGame;
    int m_onlinePageIndex;

    // 网络管理器
    NetworkManager *m_networkManager;

    // 当前登录用户名
    QString m_currentUsername;

    // 辅助函数
    void setupNetworkConnections();
    QString getMyUsername() const;
};

#endif // MENU_H
