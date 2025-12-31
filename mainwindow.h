#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QMessageBox>

#include <QStackedWidget>
#include "menu.h"

#include "gameboard.h"
#include "skilltree.h"
#include "skilltreepage.h"
#include "rankpage.h"
#include "networkmanager.h"
#include "musicmanager.h"

#include "mode_2.h"
#include "mode_ai.h"

class Mode_1;
class Mode_3;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void switchToSkillTreePage();
    void switchToRankPage();
    QString getCurrentUsername() const { return m_currentUser; }
    SkillTree* getSkillTree() const { return m_skillTree; }

private slots:
    void on_pushButtonLogin_clicked();
    void on_pushButtonRegister_clicked();
    void onStartGame(const QString &mode);
    void onGameFinished(bool isNormalEnd = false);
    void onSkillTreeBack();
    void onRankPageBack();
    void onAIDemoStart();

    // 网络相关槽函数
    void onLoginResult(bool success, const QString &message, const QJsonObject &userData);
    void onNetworkError(const QString &error);
    void onServerMessage(const QString &type, const QJsonObject &data);

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    bool openDB();
    void saveGameRecord(const QString& username, int score, const QString& mode);

    QStackedWidget *stack;
    Menu* menuPage;
    GameBoard *m_gameBoard = nullptr;
    Mode_1 *m_mode1Page = nullptr;
    Mode_2 *m_mode2Page = nullptr;
    Mode_3 *m_mode3Page = nullptr;

    // 网络管理器
    NetworkManager *m_networkManager = nullptr;

    // 技能树相关
    SkillTree *m_skillTree = nullptr;
    SkillTreePage *m_skillTreePage = nullptr;

    // 排行榜相关
    RankPage *m_rankPage = nullptr;
    RankManager *m_rankManager = nullptr;

    QString m_currentUser;
    QString m_currentGameMode;

    // AI演示模式
    Mode_AI *m_modeAIPage = nullptr;
};

#endif // MAINWINDOW_H
