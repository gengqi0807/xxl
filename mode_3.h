#ifndef MODE_3_H
#define MODE_3_H

#include <QWidget>
#include <QSequentialAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QPushButton>
#include <QStack>
#include <QLabel>
#include <QSet>
#include "gameboard.h"
#include "skilltree.h"

#include "musicmanager.h"

class QGridLayout;

namespace Ui { class mode_3; }

class Mode_3 : public QWidget
{
    Q_OBJECT

    struct GameStateSnapshot {
        Grid grid;
        int score;
        int currentAnimal;  // 新增：保存当前随机小动物
    };

public:
    explicit Mode_3(GameBoard *board, QString username, QWidget *parent = nullptr);
    ~Mode_3();

    // 事件过滤器：用于点击交互
    bool eventFilter(QObject *watched, QEvent *event) override;
    // 设置技能树
    void setSkillTree(SkillTree* skillTree) {
        m_skillTree = skillTree;
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
    Ui::mode_3 *ui;
    GameBoard *m_board;
    QGridLayout *m_gridLayout;
    QVector<QPushButton*> m_cells;
    QSequentialAnimationGroup *m_dropGroup = nullptr;
    bool m_ultimateBurstActive = false;
    QDialog* m_skillDialog = nullptr;

    // === 变身模式特有变量 ===
    int m_currentAnimal = -1;        // 当前随机生成的小动物类型 (0-5)
    void generateRandomAnimal();      // 生成随机小动物
    void updateAnimalDisplay();       // 更新UI显示

    // 选中效果
    QLabel *m_selectionIndicator;    // 选中指示器（单个格子）
    int m_selectedR = -1;            // 当前选中的行
    int m_selectedC = -1;            // 当前选中的列

    void updateSelectionPos(QPoint mousePos); // 更新选中位置
    void tryTransformInteraction();            // 执行变身交互
    void processTransform(int r, int c);       // 执行变身动画

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
    void playSpecialEffect(EffectType type, QPoint center, int colorCode);
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

    // === 提示功能 ===
    int m_hintCount = 3;
    QSequentialAnimationGroup *m_hintAnimGroup = nullptr;
    bool findValidMove(int &outR, int &outC); // 查找可消除的点击位置
    void showHint(int r, int c);              // 高亮提示位置
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

#endif // MODE_3_H
