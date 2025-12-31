#ifndef MODE_AI_H
#define MODE_AI_H

#include <QWidget>
#include <QSequentialAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QPushButton>
#include <QHash>
#include "gameboard.h"
#include "musicmanager.h"

// 【新增】 这里必须加前向声明，否则编译器不认识 QGridLayout
class QGridLayout;

namespace Ui { class Mode_AI; }

class Mode_AI : public QWidget
{
    Q_OBJECT

public:
    explicit Mode_AI(GameBoard *board, QWidget *parent = nullptr);
    ~Mode_AI();

signals:
    void gameFinished(); // 不需要参数，不保存记录

private slots:
    void rebuildGrid();
    void createDropAnimation(int left0, int top0);
    void onTimerTick();
    void onBackButtonClicked(); // 直接退出

    // AI 思考槽函数
    void performAIMove();

private:
    void clearGridLayout();
    Ui::Mode_AI           *ui;
    GameBoard             *m_board;
    QGridLayout           *m_gridLayout; // 现在编译器知道这是一个类了
    QVector<QPushButton*>  m_cells;
    QSequentialAnimationGroup *m_dropGroup;

    // 核心动画与逻辑
    void playShake(QPushButton *btn);
    void processInteraction(int r1, int c1, int r2, int c2);
    void performFallAnimation();
    void checkComboMatches();
    void playEliminateAnim(const QSet<QPoint>& points);
    void handleDeadlock();

    // 辅助计算
    int countDirection(int r, int c, int dR, int dC);

    // 复用 Mode_1 的消除判定逻辑
    enum EffectType { None, Normal, RowBomb, ColBomb, AreaBomb, ColorClear };
    struct ElimResult {
        QSet<QPoint> points;
        EffectType type = None;
        QPoint center = QPoint(-1, -1);
    };
    ElimResult getEliminations(int r, int c, const Grid& gridSnapshot);
    void playSpecialEffect(EffectType type, QPoint center, int colorCode);

    // AI 决策数据结构
    struct MoveChoice {
        int r1, c1;
        int r2, c2;
        int score; // 权重
    };
    MoveChoice calculateBestMove();

    // 状态变量
    int m_score = 0;
    int m_totalTime = 300;
    QTimer *m_gameTimer;
    QTimer *m_aiThinkTimer;
    bool m_isLocked = false;
    bool m_hasGameStarted = false;

    void startGameSequence();
    void addScore(int count);

    int evaluatePotential(const Grid& g, const QSet<QPoint>& ignoreCells);

    // 【新增】递归搜索函数：返回该分支的最高期望得分
    // depth: 剩余搜索深度
    // currentGrid: 当前递归层级的棋盘快照
    int recursiveSearch(Grid currentGrid, int depth, int alpha, int beta);
};

#endif // MODE_AI_H
