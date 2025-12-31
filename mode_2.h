#ifndef MODE_2_H
#define MODE_2_H

#include <QWidget>
#include <QSequentialAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QPushButton>
#include <QStack>
#include <QLabel>
#include "gameboard.h"
#include "skilltree.h"

#include "musicmanager.h"

class QGridLayout;

namespace Ui { class mode_2; }

class Mode_2 : public QWidget
{
    Q_OBJECT

    struct GameStateSnapshot {
        Grid grid;
        int score;
    };

public:
    explicit Mode_2(GameBoard *board, QString username, QWidget *parent = nullptr);
    ~Mode_2();

    // 【核心】事件过滤器：取代了 handleCellClick
    bool eventFilter(QObject *watched, QEvent *event) override;
    // 设置技能树
    void setSkillTree(SkillTree* skillTree) {
        m_skillTree = skillTree;
        // 【新增】重置技能使用状态
        resetSkills();
    }

signals:
    void gameFinished(bool isNormalEnd = false);

private slots:
    void rebuildGrid();
    void createDropAnimation(int left0, int top0);
    void onTimerTick();
    void onBackButtonClicked();
    void on_btnUndo_clicked();
    void on_btnHint_clicked();
    void on_btnSkill_clicked();

private:
    Ui::mode_2 *ui;
    GameBoard *m_board;
    QGridLayout *m_gridLayout;
    QVector<QPushButton*> m_cells;
    QSequentialAnimationGroup *m_dropGroup = nullptr;
    bool m_ultimateBurstActive = false;

    // === 旋风模式特有变量 ===
    QLabel *m_selectorFrame; // 2x2 的选择光圈
    int m_selR = -1;         // 当前光圈左上角的行
    int m_selC = -1;         // 当前光圈左上角的列

    void updateSelectorPos(QPoint mousePos); // 根据鼠标更新光圈位置
    void tryRotateInteraction();             // 执行旋转交互
    void processRotation(int r, int c);      // 旋转动画

    // === 通用逻辑 (保留) ===
    void clearGridLayout();
    void performFallAnimation();
    void checkComboMatches();
    void playEliminateAnim(const QSet<QPoint>& points);

    // 消除判定相关
    enum EffectType { None, Normal, RowBomb, ColBomb, AreaBomb, ColorClear };
    struct ElimResult {
        QSet<QPoint> points;
        EffectType type = None;
        QPoint center = QPoint(-1, -1);
    };
    ElimResult getEliminations(int r, int c);
    void playSpecialEffect(EffectType type, QPoint center, int colorCode); // 修复参数类型匹配
    int countDirection(int r, int c, int dR, int dC);

    // 状态管理
    void handleDeadlock();
    void addScore(int count);
    void gameOver();
    void togglePause();
    void startGameSequence();
    void saveState();

    QString m_username;
    int m_score = 0;
    int m_totalTime = 180;
    QTimer *m_gameTimer;
    bool m_hasGameStarted = false;
    bool m_isPaused = false;
    bool m_isLocked = false;
    QStack<GameStateSnapshot> m_undoStack;

    // === 提示功能 (已修改为旋转逻辑) ===
    int m_hintCount = 3;
    QSequentialAnimationGroup *m_hintAnimGroup = nullptr;
    bool findValidMove(int &outR, int &outC); // 查找可消除的旋转点
    void showHint(int r, int c);              // 高亮 2x2 区域
    void stopHint();

    // 新增：技能树引用
    SkillTree* m_skillTree = nullptr;
    // 【新增】技能管理器
    class SkillManager* m_skillManager = nullptr;

    // 【新增】技能激活状态
    bool m_scoreDoubleActive = false;
    bool m_colorUnifyActive = false;
    QTimer* m_skillEffectTimer = nullptr;

    // 【新增】技能相关函数
    void resetSkills();  // 重置技能状态
    void onSkillEffectTimeout();  // 技能效果结束
    void showSkillEndHint(const QString& message);
    void showTempMessage(const QString& message, const QColor& color);
};

#endif // MODE_2_H
