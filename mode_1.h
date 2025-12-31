/* mode_1.h */
#ifndef MODE_1_H
#define MODE_1_H

#include <QWidget>
#include <QSequentialAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QPushButton>
#include <QHash>
#include <QMessageBox>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <QStack>          // 【新增】栈
#include "gameboard.h"     // 确保包含 GameBoard 定义以使用 Grid 类型
#include "skilltree.h"

#include "musicmanager.h"

class GameBoard;
class QGridLayout;

namespace Ui { class Mode_1; }

class Mode_1 : public QWidget
{
    Q_OBJECT

    // 【新增】定义一个简单的结构体保存历史状态
    struct GameStateSnapshot {
        Grid grid;      // 棋盘数据 (std::array 值拷贝)
        int score;      // 当时分数
    };

public:
    // 修改构造函数，增加 username 参数
    explicit Mode_1(GameBoard *board, QString username, QWidget *parent = nullptr);
    ~Mode_1();

    // 【新增】重写事件过滤器，让 Label 能响应点击
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setSkillTree(SkillTree* skillTree)
    {
        m_skillTree = skillTree;
        resetSkills();
    }

signals:
    void gameFinished(bool isNormalEnd = false);

private slots:
    void rebuildGrid();
    void createDropAnimation(int left0, int top0);

    // 【新增】倒计时槽函数
    void onTimerTick();


    // 【新增】返回按钮的处理槽函数
    void onBackButtonClicked();


    // 【新增】功能按钮槽函数
    void on_btnUndo_clicked();  // 撤步
    void on_btnHint_clicked();  // 提示 (先占位)
    void on_btnSkill_clicked(); // 技能 (先占位)



private:
    void clearGridLayout();
    Ui::Mode_1            *ui;
    GameBoard             *m_board;
    QGridLayout           *m_gridLayout;
    QVector<QPushButton*>  m_cells;
    QSequentialAnimationGroup *m_dropGroup;
    bool m_ultimateBurstActive = false;

    // 交互逻辑
    void handleCellClick(int r, int c);
    int m_clickCount = 0;
    int m_selR = -1, m_selC = -1;
    QHash<QPushButton*, QString> m_originalStyle;
    void setSelected(QPushButton *btn, bool on);
    void playShake(QPushButton *btn);
    void processInteraction(int r1, int c1, int r2, int c2);
    void applyGravity();
    int countDirection(int r, int c, int dR, int dC);
    void performFallAnimation();
    void checkComboMatches();
    void playEliminateAnim(const QSet<QPoint>& points);

    bool m_isLocked = false;

    enum EffectType { None, Normal, RowBomb, ColBomb, AreaBomb, ColorClear };
    struct ElimResult {
        QSet<QPoint> points;
        EffectType type = None;
        QPoint center = QPoint(-1, -1);
    };
    ElimResult getEliminations(int r, int c);
    void playSpecialEffect(EffectType type, QPoint center, int colorCode);
    void handleDeadlock();

    // ==========================================
    // 【新增核心功能变量】
    // ==========================================
    QString m_username;       // 当前玩家
    int m_score = 0;          // 当前分数
    int m_totalTime = 180;    // 总时间 180秒
    QTimer *m_gameTimer;      // 计时器
    bool m_hasGameStarted = false; // 标记是否已经完成了开场动画

    void startGameSequence(); // 播放“游戏开始”动画
    void addScore(int count); // 加分逻辑
    void gameOver();          // 结算逻辑


    // 【新增】暂停状态标志
    bool m_isPaused = false;

    // 【新增】切换暂停/继续的辅助函数
    void togglePause();



    // 【新增】保存状态的辅助函数
    void saveState();

    // 【新增】撤步栈
    QStack<GameStateSnapshot> m_undoStack;


    // 【新增】提示功能变量
    // ==========================================
    int m_hintCount = 3;                       // 剩余次数
    QList<QPushButton*> m_hintBtns;            // 当前被高亮的按钮
    QSequentialAnimationGroup *m_hintAnimGroup = nullptr; // 提示动画组

    // 查找并显示提示
    void showHint();
    // 停止提示动画
    void stopHint();
    // 辅助：寻找一个可消除的移动 (返回 r1,c1, r2,c2)
    bool findValidMove(int &r1, int &c1, int &r2, int &c2);

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

#endif // MODE_1_H
