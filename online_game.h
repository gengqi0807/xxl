#ifndef ONLINE_GAME_H
#define ONLINE_GAME_H

#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QJsonArray>
#include <QStack>
#include <QSequentialAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QVector>
#include <QGridLayout>
#include <QSet>
#include <QPoint>
#include <QQueue>

#include "gameboard.h"
#include "skilltree.h"
#include "networkmanager.h"
#include "musicmanager.h"

namespace Ui {
class OnlineGame;
}

class OnlineGame : public QWidget
{
    Q_OBJECT

public:
    explicit OnlineGame(const QString &myUsername, const QString &opponentUsername,
                        QWidget *parent = nullptr);
    ~OnlineGame();

    void setMySkillTree(SkillTree* skillTree);

signals:
    void gameFinished();
    void returnToMenu();

private slots:
    void onGameTimerTick();
    void on_btnBack_clicked();
    void on_btnMyUndo_clicked();
    void on_btnMySkill_clicked();

    // 网络消息处理
    void onServerMessage(const QString &type, const QJsonObject &data);
    void onGameStartReceived(const QJsonObject &data);
    void onGameMoveReceived(const QJsonObject &massageData);
    void onGameEndReceived(const QJsonObject &data);
    void onPlayerQuitReceived(const QJsonObject &data);

private:
    Ui::OnlineGame *ui;

    // 玩家信息
    QString m_myUsername;
    QString m_opponentUsername;
    QString m_roomId;

    // 游戏状态
    int m_myScore;
    int m_opponentScore;
    int m_totalTime;
    QTimer *m_gameTimer;
    QTimer *m_syncTimer;
    bool m_isGameActive;
    bool m_isGameStarted;
    bool m_gameEnded;

    int m_lastSyncedScore;
    Grid m_lastSyncedGrid;
    bool m_hasInitialSync;
    qint64 m_lastSyncTime;
    QJsonArray m_lastBoardArray;

    // 棋盘逻辑
    GameBoard *m_myBoard;
    GameBoard *m_opponentBoard;
    SkillTree *m_mySkillTree;

    // 棋盘显示（我的棋盘）
    QVector<QPushButton*> m_myCells;
    QGridLayout *m_myGridLayout;
    QSequentialAnimationGroup *m_myDropGroup;
    bool m_myLocked;
    bool m_myPaused;
    int m_myClickCount;
    int m_mySelR, m_mySelC;

    // 棋盘显示（对手棋盘）
    QVector<QPushButton*> m_opponentCells;
    QGridLayout *m_opponentGridLayout;
    QSequentialAnimationGroup *m_opponentDropGroup;
    bool m_opponentLocked;  // 新增：对手棋盘锁定状态

    // 技能状态
    bool m_myScoreDoubleActive;
    bool m_myColorUnifyActive;
    bool m_myUltimateBurstActive;
    QTimer *m_mySkillEffectTimer;

    // 撤步栈
    struct GameStateSnapshot {
        Grid grid;
        int score;
    };
    QStack<GameStateSnapshot> m_myUndoStack;

    // 消除结果枚举
    enum EffectType { None, Normal, RowBomb, ColBomb, AreaBomb, ColorClear };
    struct ElimResult {
        QSet<QPoint> points;
        EffectType type = None;
        QPoint center = QPoint(-1, -1);
    };

    // =============== 我的棋盘方法 ===============
    void initMyBoard();
    void rebuildMyGrid();
    void clearMyGridLayout();
    void createMyDropAnimation(int left0, int top0);
    void handleMyCellClick(int r, int c);
    void setMySelected(QPushButton *btn, bool on);
    void playMyShake(QPushButton *btn);
    void processMyInteraction(int r1, int c1, int r2, int c2);
    void performMyFallAnimation();
    void checkMyComboMatches();
    void playMyEliminateAnim(const QSet<QPoint>& points);
    void addMyScore(int count);
    void saveMyState();

    // 消除检测
    ElimResult getMyEliminations(int r, int c);
    int countMyDirection(int r, int c, int dR, int dC);
    void playMySpecialEffect(EffectType type, QPoint center, int colorCode);
    void handleMyDeadlock();

    // =============== 对手棋盘方法 ===============
    void initOpponentBoard();
    void rebuildOpponentGrid();
    void clearOpponentGridLayout();
    void createOpponentDropAnimation(int left0, int top0);
    void updateOpponentFromNetwork(const QJsonArray &boardArray, int score);

    // 【新增】对手棋盘动画方法
    void playOpponentEliminateAnim(const QSet<QPoint>& points);
    void performOpponentFallAnimation();
    void playOpponentSpecialEffect(EffectType type, QPoint center, int colorCode);
    void playOpponentCellShake(QPushButton *btn);
    void processOpponentUpdate(const Grid& oldGrid, const Grid& newGrid);

    // =============== 通用方法 ===============
    void updateUI();
    void updateMyInfo();
    void updateOpponentInfo();
    void updateCountdown();
    void startGameSequence();
    void gameOver();
    void endGameWithResult(bool isWinner);

    // 动画工具
    void showTempMessage(const QString& message, const QColor& color);

    // 技能相关
    void resetMySkills();
    void onMySkillEffectTimeout();
    void showSkillEndHint(const QString& message);

    // 网络同步
    void syncMyBoard();
    void syncBoardToServer();
    QJsonArray boardToJsonArray(const Grid &grid);
    void jsonArrayToBoard(const QJsonArray &array, Grid &grid);

    // 【新增】棋盘比较辅助函数
    void findDifferences(const Grid& oldGrid, const Grid& newGrid,
                         QSet<QPoint>& eliminated, QSet<QPoint>& newCells);

    // 辅助函数
    QString getCellImagePath(int colorIndex) const;
};

#endif // ONLINE_GAME_H
