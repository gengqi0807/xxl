#include "mode_1.h"
#include "ui_mode_1.h"
#include "gameboard.h"
#include "networkmanager.h"
#include <QGridLayout>
#include <QPushButton>
#include <QDir>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QRandomGenerator>
#include <QLabel>
#include <QDebug>
#include <QDateTime>
#include <QDialog>
#include <QVBoxLayout>

// 1. 修改构造函数，接收 username
Mode_1::Mode_1(GameBoard *board, QString username, QWidget *parent)
    : QWidget(parent), ui(new Ui::Mode_1), m_board(board), m_username(username)
{
    ui->setupUi(this);

    // 初始化数据
    m_score = 0;
    m_totalTime = 5; // 3分钟
    m_hasGameStarted = false;


    // 1. 设置 labelScore 为暂停按钮样式
    ui->labelScore->setText("暂停");
    ui->labelScore->setStyleSheet(
        "color:#fff; font:bold 15pt 'Microsoft YaHei'; "
        );
    ui->labelScore->setAlignment(Qt::AlignCenter);

    // 【关键】安装事件过滤器，捕捉点击
    ui->labelScore->installEventFilter(this);
    // 开启鼠标追踪（可选，为了更好的交互体验）
    ui->labelScore->setCursor(Qt::PointingHandCursor);



    // 更新UI显示
    ui->labelScore_2->setText(QString("分数 %1").arg(m_score));
    ui->labelCountdown->setText("03:00");

    m_dropGroup = new QSequentialAnimationGroup(this);
    m_gridLayout = new QGridLayout(ui->boardWidget);
    m_gridLayout->setSpacing(2);
    m_gridLayout->setContentsMargins(4, 4, 4, 4);

    connect(m_board, &GameBoard::gridUpdated, this, &Mode_1::rebuildGrid);

    // -----------------------------------------------------------
    // 【问题就在这里】
    // 下面这行代码必须删除或注释掉！
    // 否则每次点返回，它都会触发死局重置逻辑，导致棋盘被刷新。
    // connect(ui->btnBack, &QPushButton::clicked, this, &Mode_1::handleDeadlock);  // <--- 删除这一行
    // -----------------------------------------------------------

    // 初始化定时器
    m_gameTimer = new QTimer(this);
    m_gameTimer->setInterval(1000); // 1秒一次
    connect(m_gameTimer, &QTimer::timeout, this, &Mode_1::onTimerTick);

    rebuildGrid();


    // 初始化提示次数
    m_hintCount = 3;
    ui->btnHint->setText(QString("提示 (%1)").arg(m_hintCount));

    // 正确的连接：只连接到返回按钮处理逻辑
    connect(ui->btnBack, &QPushButton::clicked, this, &Mode_1::onBackButtonClicked);

    m_skillEffectTimer = new QTimer(this);
    m_skillEffectTimer->setSingleShot(true);
    connect(m_skillEffectTimer, &QTimer::timeout, this, &Mode_1::onSkillEffectTimeout);

    // 获取网络管理器并更新状态为"游戏中"
    NetworkManager *networkManager = NetworkManager::instance();
    if (networkManager && networkManager->isConnected()) {
        networkManager->updateUserStatus("游戏中", "闪电");
    }
}
// 析构函数定义
Mode_1::~Mode_1()
{
    delete ui;
}

void Mode_1::rebuildGrid()
{
    clearGridLayout();
    m_cells.resize(ROW * COL);
    const Grid &gr = m_board->grid();

    /* ===== 取 boardWidget 内部可用区域 ===== */
    QRect cr      = ui->boardWidget->contentsRect();   // 去掉边框、padding
    int availW    = cr.width();
    int availH    = cr.height();
    int cellSize  = 48;
    int gap       = 2;
    int totalW    = COL * (cellSize + gap) - gap;
    int totalH    = ROW * (cellSize + gap) - gap;

    /* 居中留白 */
    int ox = cr.left() + (availW - totalW) / 2;   // 首格 X
    int oy = cr.top()  + (availH - totalH) / 2;   // 首格 Y
    /* ========================================= */

    /* 创建按钮：父对象 = boardWidget，先手动定位，不落布局 */
    for (int r = 0; r < ROW; ++r)
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = new QPushButton(ui->boardWidget);
            btn->setFixedSize(cellSize, cellSize);
            btn->setStyleSheet("border:none;");
            QString path = QString("%1%2.png").arg(QDir::currentPath() + "/").arg(gr[r][c].pic);
            btn->setIcon(QIcon(path));
            btn->setIconSize(QSize(cellSize, cellSize));

            int targetX = ox + c * (cellSize + gap);
            int targetY = oy + r * (cellSize + gap);

            /* 初始：头顶外 */
            btn->move(targetX, targetY - (ROW - r) * (cellSize + gap));
            auto *eff = new QGraphicsOpacityEffect(btn);
            eff->setOpacity(0.0);
            btn->setGraphicsEffect(eff);
            btn->show();
            m_cells[r * COL + c] = btn;
            connect(btn, &QPushButton::clicked, this, [this, r, c]() {
                handleCellClick(r, c);
            });
        }

    /* 启动掉落动画：把原点传进去，动画里继续用 */
    createDropAnimation(ox, oy);
}

/* mode_1.cpp - 修复后的 createDropAnimation */
/* 2. 修改 createDropAnimation 处理开局逻辑 */
void Mode_1::createDropAnimation(int left0, int top0)
{
    if (m_dropGroup->state() == QAbstractAnimation::Running) {
        m_dropGroup->stop();
    }
    m_dropGroup->clear();
    m_dropGroup->disconnect(this);

    int cellSize = 48;
    int gap      = 2;

    for (int r = ROW - 1; r >= 0; --r) {
        auto *rowPar = new QParallelAnimationGroup(this);
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = m_cells[r * COL + c];
            if (!btn) continue;

            auto *eff = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
            if (!eff) {
                eff = new QGraphicsOpacityEffect(btn);
                btn->setGraphicsEffect(eff);
            }

            int targetX = left0 + c * (cellSize + gap);
            int targetY = top0  + r * (cellSize + gap);

            auto *drop = new QPropertyAnimation(btn, "pos");
            drop->setDuration(180 + (ROW - 1 - r) * 15);
            drop->setStartValue(QPoint(targetX, targetY - 200));
            drop->setEndValue(QPoint(targetX, targetY));
            drop->setEasingCurve(QEasingCurve::OutBounce);

            auto *fade = new QPropertyAnimation(eff, "opacity");
            fade->setDuration(300);
            fade->setStartValue(0.0);
            fade->setEndValue(1.0);

            auto *cellAnim = new QParallelAnimationGroup(this);
            cellAnim->addAnimation(drop);
            cellAnim->addAnimation(fade);
            rowPar->addAnimation(cellAnim);
        }
        m_dropGroup->addAnimation(rowPar);
        m_dropGroup->addPause(1);
    }

    connect(m_dropGroup, &QSequentialAnimationGroup::finished, this, [this]() {
        for (int r = 0; r < ROW; ++r) {
            for (int c = 0; c < COL; ++c) {
                QPushButton *btn = m_cells[r * COL + c];
                if (btn) m_gridLayout->addWidget(btn, r, c);
            }
        }
        m_isLocked = false;

        // 【新增逻辑】如果是第一次初始化完成，播放 Start 动画并开始计时
        if (!m_hasGameStarted) {
            m_hasGameStarted = true;
            startGameSequence(); // 播放 "Game Start"
        }
        // 注意：如果是 handleDeadlock 触发的重建，不会重置 m_hasGameStarted，也就不会重置倒计时
    });

    m_dropGroup->start();
}


/* 3. 新增：游戏开始动画 */
void Mode_1::startGameSequence()
{
    m_isLocked = true; // 动画期间锁住
    resetSkills();

    // 【新增】播放游戏过程背景音乐
    MusicManager::instance().playSceneMusic(MusicManager::MusicScene::Playing);

    QLabel *lbl = new QLabel(ui->boardWidget);
    lbl->setText("Ready Go!");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(
        "color: #ffeb3b;"
        "font: bold 40pt 'Microsoft YaHei';"
        "background: transparent;"
        );
    lbl->adjustSize();
    QRect cr = ui->boardWidget->rect();
    lbl->move((cr.width() - lbl->width()) / 2, (cr.height() - lbl->height()) / 2);
    lbl->show();
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(lbl);
    lbl->setGraphicsEffect(eff);

    // 停留一下然后淡出
    QSequentialAnimationGroup *seq = new QSequentialAnimationGroup(this);
    seq->addPause(500); // 停 0.5秒

    QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
    fade->setDuration(1000);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);

    seq->addAnimation(fade);

    connect(seq, &QAbstractAnimation::finished, this, [this, lbl](){
        lbl->deleteLater();
        m_isLocked = false;
        m_gameTimer->start(); // 【关键】开始倒计时
    });

    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

/* 4. 新增：定时器逻辑 */
void Mode_1::onTimerTick()
{
    if (m_totalTime > 0) {
        m_totalTime--;

        // 格式化时间 mm:ss
        int m = m_totalTime / 60;
        int s = m_totalTime % 60;
        ui->labelCountdown->setText(QString("%1:%2")
                                        .arg(m, 2, 10, QChar('0'))
                                        .arg(s, 2, 10, QChar('0')));
    } else {
        m_gameTimer->stop();
        gameOver();
    }
}

/* 5. 新增：加分函数 */
void Mode_1::addScore(int count)
{
    // 【新增】如果终极爆发技能激活，跳过正常计分
    if (m_ultimateBurstActive) return;

    int points = count * 50;
    // 如果得分翻倍激活
    if (m_scoreDoubleActive) {
        points *= 2;
    }

    m_score += points;
    ui->labelScore_2->setText(QString("分数 %1").arg(m_score));
}


/* mode_1.cpp - 修复 Double Free 崩溃 */
void Mode_1::clearGridLayout()
{
    // 【新增】停止技能效果计时器
    if (m_skillEffectTimer && m_skillEffectTimer->isActive()) {
        m_skillEffectTimer->stop();
    }

    // 1. 如果有掉落动画正在进行，必须停止，否则动画会访问野指针
    if (m_dropGroup) {
        m_dropGroup->stop();
        m_dropGroup->clear();
    }

    // 2. 优先通过 m_cells 清理按钮
    // QWidget 析构时会自动把自己从父布局（m_gridLayout）中移除
    for (QPushButton *b : m_cells) {
        if (b) {
            delete b;
        }
    }
    m_cells.clear(); // 清空列表

    // 3. 清理布局中剩余的非 Widget 项目（如弹簧、空占位符等）
    if (m_gridLayout && ui->boardWidget) {
        QLayoutItem *item;
        while ((item = m_gridLayout->takeAt(0)) != nullptr) {
            // 如果布局里还有没被 m_cells 记录的控件，也一并删掉
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
    }
}




void Mode_1::handleCellClick(int r, int c)
{

    // 1. 用户只要点了棋盘，就停止提示动画
    stopHint();

    if (m_isLocked || m_isPaused) return;
    if (m_isLocked) return; // 【关键】如果是锁定状态，直接无视点击，但不改变样式

    QPushButton *btn = m_cells[r * COL + c];

    if (m_clickCount == 0) {
        /* 第一次点击：仅高亮 */
        m_selR = r; m_selC = c;
        setSelected(btn, true);
        m_clickCount = 1;
        return;
    }

    /* 第二次点击 */
    QPushButton *firstBtn = m_cells[m_selR * COL + m_selC];

    if (qAbs(m_selR - r) + qAbs(m_selC - c) != 1) {
        /* 不相邻 → 直接重选，不抖动 */
        setSelected(firstBtn, false);
        m_selR = r; m_selC = c;
        setSelected(btn, true);
        return;
    }

    /* 相邻 → 尝试交换 */
    bool ok = m_board->trySwap(m_selR, m_selC, r, c);
    setSelected(firstBtn, false);   // 高亮无论成功失败都消失
    m_clickCount = 0;

    if (!ok) {
        /* 不可交换 → 两个一起抖动 */
        playShake(firstBtn);
        playShake(btn);
    }else {
        // 【新增】在产生不可逆变化前，保存当前状态到栈中
        saveState();
        // 2. 可交换：进入消除流程
        // 先在逻辑层真正交换数据
        std::swap(m_board->m_grid[m_selR][m_selC].pic, m_board->m_grid[r][c].pic);

        // 执行消除处理
        processInteraction(m_selR, m_selC, r, c);
    }

}

void Mode_1::setSelected(QPushButton *btn, bool on)
{
    if (on) {
        auto *shadow = new QGraphicsDropShadowEffect(btn);
        shadow->setColor(QColor("#ff9de0"));
        shadow->setBlurRadius(12);
        shadow->setOffset(0, 0);   // 纯外环，不偏移
        btn->setGraphicsEffect(shadow);
    } else {
        btn->setGraphicsEffect(nullptr); // 去掉发光
    }
}

void Mode_1::playShake(QPushButton *btn)
{
    auto *anim = new QPropertyAnimation(btn, "pos");
    QPoint p = btn->pos();
    anim->setDuration(120);
    anim->setKeyValueAt(0, p);
    anim->setKeyValueAt(0.25, p - QPoint(4, 0));
    anim->setKeyValueAt(0.75, p + QPoint(4, 0));
    anim->setKeyValueAt(1, p);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}


/* 辅助：计算某个方向有多少个连续同色 (不包含中心自己) */
int Mode_1::countDirection(int r, int c, int dR, int dC)
{
    int color = m_board->m_grid[r][c].pic;
    int count = 0;
    int nr = r + dR;
    int nc = c + dC;
    while(nr >= 0 && nr < ROW && nc >= 0 && nc < COL &&
           m_board->m_grid[nr][nc].pic == color) {
        count++;
        nr += dR;
        nc += dC;
    }
    return count;
}

/* 核心算法：根据你的规则计算需要消除的点 */
/* mode_1.cpp - 修改 getEliminations */

Mode_1::ElimResult Mode_1::getEliminations(int r, int c)
{
    ElimResult res;
    res.center = QPoint(r, c); // 记录触发点

    int color = m_board->m_grid[r][c].pic;
    if (color == -1) return res;

    int up    = countDirection(r, c, -1, 0);
    int down  = countDirection(r, c, 1, 0);
    int left  = countDirection(r, c, 0, -1);
    int right = countDirection(r, c, 0, 1);

    // 规则 1: 全屏消除 (魔鸟/闪电)
    if ((up + down >= 4) || (left + right >= 4)) {
        res.type = ColorClear;
        for (int i = 0; i < ROW; ++i) {
            for (int j = 0; j < COL; ++j) {
                if (m_board->m_grid[i][j].pic == color) {
                    res.points.insert(QPoint(i, j));
                }
            }
        }
        return res;
    }

    // 规则 2: 行/列消除
    if (up + down == 3) {
        res.type = ColBomb;
        for (int i = 0; i < ROW; ++i) res.points.insert(QPoint(i, c));
        return res;
    }
    if (left + right == 3) {
        res.type = RowBomb;
        for (int j = 0; j < COL; ++j) res.points.insert(QPoint(r, j));
        return res;
    }

    // 规则 3: 5x5 炸弹 (T/L型)
    if (up + down >= 2 && left + right >= 2) {
        res.type = AreaBomb;
        for (int i = r - 2; i <= r + 2; ++i) {
            for (int j = c - 2; j <= c + 2; ++j) {
                if (i >= 0 && i < ROW && j >= 0 && j < COL) {
                    res.points.insert(QPoint(i, j));
                }
            }
        }
        return res;
    }

    // 规则 4: 普通三消
    if (up + down >= 2) {
        for (int i = r - up; i <= r + down; ++i) res.points.insert(QPoint(i, c));
        res.type = Normal;
    }
    if (left + right >= 2) {
        for (int j = c - left; j <= c + right; ++j) res.points.insert(QPoint(r, j));
        res.type = Normal; // 即使是十字消除，如果没有达到炸弹条件，也算普通，或者你可以加个 CrossBomb
    }

    return res;
}
/* 统筹流程：交换后，处理两个点，并执行动画 */
/* mode_1.cpp - 修复后的 processInteraction */

/* mode_1.cpp - 修复类型匹配错误的 processInteraction */

void Mode_1::processInteraction(int r1, int c1, int r2, int c2)
{
    m_isLocked = true; // 上锁

    // 1. 【修复点】使用 ElimResult 接收结果，而不是 QSet<QPoint>
    ElimResult res1 = getEliminations(r1, c1);
    ElimResult res2 = getEliminations(r2, c2);

    // 2. 提取点集用于合并
    QSet<QPoint> allToRemove = res1.points;
    allToRemove.unite(res2.points);

    // 3. 如果没消除，回滚并抖动
    if (allToRemove.isEmpty()) {
        std::swap(m_board->m_grid[r1][c1].pic, m_board->m_grid[r2][c2].pic);
        playShake(m_cells[r1 * COL + c1]);
        playShake(m_cells[r2 * COL + c2]);
        ui->boardWidget->setEnabled(true);
        m_isLocked = false; // 解锁
        return;
    }

    // 4. 视觉交换动画
    int idx1 = r1 * COL + c1;
    int idx2 = r2 * COL + c2;
    QPushButton *btn1 = m_cells[idx1];
    QPushButton *btn2 = m_cells[idx2];

    // 交换 m_cells 指针
    std::swap(m_cells[idx1], m_cells[idx2]);

    QParallelAnimationGroup *swapGroup = new QParallelAnimationGroup(this);

    QPropertyAnimation *anim1 = new QPropertyAnimation(btn1, "pos");
    anim1->setDuration(300);
    anim1->setStartValue(btn1->pos());
    anim1->setEndValue(btn2->pos());

    QPropertyAnimation *anim2 = new QPropertyAnimation(btn2, "pos");
    anim2->setDuration(300);
    anim2->setStartValue(btn2->pos());
    anim2->setEndValue(btn1->pos());

    swapGroup->addAnimation(anim1);
    swapGroup->addAnimation(anim2);

    // 5. 动画结束：播放特效 + 消除
    // 【关键】lambda 需要捕获 res1 和 res2 以便判断播放哪种特效
    connect(swapGroup, &QAbstractAnimation::finished, this, [this, swapGroup, allToRemove, res1, res2](){
        swapGroup->deleteLater();

        // 播放特效 (如果有)
        // 简单策略：只要触发了高级特效就播，如果两个都有则都播
        if (res1.type != Normal && res1.type != None)
            playSpecialEffect(res1.type, res1.center, 0);

        if (res2.type != Normal && res2.type != None)
            playSpecialEffect(res2.type, res2.center, 0);

        // 执行消除
        playEliminateAnim(allToRemove);
    });

    swapGroup->start();
}

/* mode_1.cpp 中的 performFallAnimation 函数 */

/* 请直接复制这个函数覆盖原有的 performFallAnimation */

/* mode_1.cpp - 最终修复版 performFallAnimation */

void Mode_1::performFallAnimation()
{
    QRect cr = ui->boardWidget->contentsRect();
    int cellSize = 48;
    int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;

    QParallelAnimationGroup *fallGroup = new QParallelAnimationGroup(this);
    auto *rng = QRandomGenerator::global();

    // 辅助结构：存方块和它的颜色
    struct BlockData {
        QPushButton* btn;
        int color;
    };

    /* 【关键步骤 0】：先把所有幸存按钮从 QGridLayout 里"踢"出来
       这样它们就变成了绝对定位的悬浮控件，不再受布局限制，
       防止布局管理器把它们强行拽回原来的格子导致"空缺"。 */
    for (QPushButton *b : m_cells) {
        if (b) {
            m_gridLayout->removeWidget(b);
            // 移除后它们还在 boardWidget 上，只是不受管束了，位置保持不变
        }
    }

    for (int c = 0; c < COL; ++c) {
        QList<BlockData> survivors;

        // 1. 收集幸存者
        for (int r = ROW - 1; r >= 0; --r) {
            int idx = r * COL + c;
            if (m_cells[idx] != nullptr) {
                survivors.append({m_cells[idx], m_board->m_grid[r][c].pic});
                m_cells[idx] = nullptr;
            }
        }

        // 2. 重建这一列
        int survivorIdx = 0;
        for (int r = ROW - 1; r >= 0; --r) {
            QPushButton *btn = nullptr;
            int finalColor = 0;
            bool isExistingBtn = false;

            int destX = ox + c * (cellSize + gap);
            int destY = oy + r * (cellSize + gap);

            if (survivorIdx < survivors.size()) {
                // --- 旧方块 ---
                BlockData bd = survivors[survivorIdx];
                btn = bd.btn;
                finalColor = bd.color;
                isExistingBtn = true;
                survivorIdx++;
            } else {
                // --- 新方块 ---
                finalColor = rng->bounded(6);
                isExistingBtn = false;

                btn = new QPushButton(ui->boardWidget);
                btn->setFixedSize(cellSize, cellSize);
                btn->setStyleSheet("border:none;");
                QString path = QString("%1%2.png").arg(QDir::currentPath() + "/").arg(finalColor);
                btn->setIcon(QIcon(path));
                btn->setIconSize(QSize(cellSize, cellSize));
                btn->show();

                // 新方块从高空掉落
                int startY = destY - (ROW * cellSize + 100);
                btn->move(destX, startY);
            }

            // 3. 更新全局状态
            m_cells[r * COL + c] = btn;
            m_board->m_grid[r][c].pic = finalColor;

            // 4. 设置信号
            if (isExistingBtn) btn->disconnect();
            connect(btn, &QPushButton::clicked, this, [this, r, c]() {
                handleCellClick(r, c);
            });

            // 5. 创建动画
            QPoint startPos = btn->pos();
            QPoint endPos = QPoint(destX, destY);

            // 只有位置不同才动，防止微小抖动
            if (startPos != endPos) {
                QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos");
                anim->setDuration(500);
                anim->setStartValue(startPos);
                anim->setEndValue(endPos);
                anim->setEasingCurve(QEasingCurve::OutBounce);
                fallGroup->addAnimation(anim);
            } else {
                btn->move(endPos);
            }
        }
    }

    // 动画结束后，把它们重新收编回布局
    connect(fallGroup, &QAbstractAnimation::finished, this, [this, fallGroup](){

        /* 【关键步骤 6】：一切尘埃落定，把按钮重新塞回 QGridLayout
           这样窗口缩放时它们能自动适应，且保证下次操作的基础位置正确 */
        for (int r = 0; r < ROW; ++r) {
            for (int c = 0; c < COL; ++c) {
                QPushButton *btn = m_cells[r * COL + c];
                if (btn) {
                    m_gridLayout->addWidget(btn, r, c);
                }
            }
        }


        fallGroup->deleteLater();
        checkComboMatches();
    });

    fallGroup->start();
}
/* mode_1.cpp - 新增函数 */

/* mode_1.cpp - 修改 checkComboMatches */

void Mode_1::checkComboMatches()
{
    QSet<QPoint> allMatches;

    // 【新增】用来记录已经播放过特效的中心点，防止重复播放
    QSet<QPoint> processedCenters;

    // 扫描全盘连击
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {

            // 优化：如果这个点已经被标记为待消除了，且不是特殊连消的关键点，理论上可以跳过
            // 但为了保险起见（因为可能是交叉消除的一部分），我们还是检查一下，主要靠 processedCenters 去重

            ElimResult res = getEliminations(r, c);

            if (!res.points.isEmpty()) {
                // 1. 收集要消除的点
                allMatches.unite(res.points);

                // 2. 【新增】检查是否有特殊效果
                if (res.type != Normal && res.type != None) {
                    // 如果这个特效组的中心点还没处理过，就播放特效
                    if (!processedCenters.contains(res.center)) {
                        playSpecialEffect(res.type, res.center, 0);
                        processedCenters.insert(res.center); // 标记已处理
                    }
                }
            }
        }
    }

    if (!allMatches.isEmpty()) {
        m_isLocked = true;
        playEliminateAnim(allMatches); // 继续消除
    } else {
        // 死局检测
        if (m_board->isDead(m_board->grid())) {
            handleDeadlock();
        } else {
            m_isLocked = false;
        }
    }
}




/* mode_1.cpp - 新增特效函数 */

void Mode_1::playSpecialEffect(EffectType type, QPoint center, int colorCode)
{
    if (type == None || type == Normal) return;

    // 计算中心点的像素坐标
    int cellSize = 48; int gap = 2;
    // 这里需要重新获取 ox, oy，建议把 ox, oy 变成成员变量，或者重新算一次
    QRect cr = ui->boardWidget->contentsRect();
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;

    int centerX = ox + center.y() * (cellSize + gap) + cellSize / 2;
    int centerY = oy + center.x() * (cellSize + gap) + cellSize / 2;

    // 1. 行/列 激光炮
    if (type == RowBomb || type == ColBomb) {
        QLabel *beam = new QLabel(ui->boardWidget);
        beam->setAttribute(Qt::WA_TransparentForMouseEvents); // 鼠标穿透
        beam->show();

        // 样式：中间白，两边透明的渐变
        QString qss = "background: qlineargradient(x1:0, y1:0, x2:%1, y2:%2, "
                      "stop:0 rgba(255,255,255,0), stop:0.5 rgba(255,255,255,230), stop:1 rgba(255,255,255,0));";

        QPropertyAnimation *anim = new QPropertyAnimation(beam, "geometry");
        anim->setDuration(400);
        anim->setEasingCurve(QEasingCurve::OutExpo);

        if (type == RowBomb) {
            // 横向激光
            beam->setStyleSheet(qss.arg("0").arg("1")); // 垂直渐变
            anim->setStartValue(QRect(ox, centerY - 2, totalW, 4)); // 初始细线
            anim->setEndValue(QRect(ox, centerY - 25, totalW, 50)); // 瞬间变宽
        } else {
            // 纵向激光
            beam->setStyleSheet(qss.arg("1").arg("0")); // 水平渐变
            anim->setStartValue(QRect(centerX - 2, oy, 4, totalH));
            anim->setEndValue(QRect(centerX - 25, oy, 50, totalH));
        }

        // 同时变透明
        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(beam);
        beam->setGraphicsEffect(eff);
        QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
        fade->setDuration(400);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->start(QAbstractAnimation::DeleteWhenStopped); // 删除 effect

        anim->start(QAbstractAnimation::DeleteWhenStopped); // 动画结束删除 label
        connect(anim, &QAbstractAnimation::finished, beam, &QLabel::deleteLater);
    }

    // 2. 区域炸弹 (冲击波)
    else if (type == AreaBomb) {
        QLabel *shockwave = new QLabel(ui->boardWidget);
        shockwave->setAttribute(Qt::WA_TransparentForMouseEvents);
        // 圆形径向渐变
        shockwave->setStyleSheet(
            "background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, "
            "stop:0 rgba(255,255,255,0), stop:0.7 rgba(255, 200, 100, 200), stop:1 rgba(255,255,255,0));"
            "border-radius: 100px;"); // 半径设大一点保证是圆的
        shockwave->show();

        QPropertyAnimation *anim = new QPropertyAnimation(shockwave, "geometry");
        anim->setDuration(500);
        anim->setEasingCurve(QEasingCurve::OutQuad);

        // 从中心扩散
        QRect startRect(centerX, centerY, 0, 0);
        QRect endRect(centerX - 150, centerY - 150, 300, 300); // 扩散半径 150

        anim->setStartValue(startRect);
        anim->setEndValue(endRect);

        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QAbstractAnimation::finished, shockwave, &QLabel::deleteLater);
    }

    // 3. 全屏闪光
    else if (type == ColorClear) {
        QLabel *flash = new QLabel(ui->boardWidget);
        flash->setAttribute(Qt::WA_TransparentForMouseEvents);
        flash->setStyleSheet("background-color: rgba(255, 255, 255, 180);");
        flash->setGeometry(ui->boardWidget->rect()); // 覆盖整个区域
        flash->show();

        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(flash);
        flash->setGraphicsEffect(eff);

        QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
        fade->setDuration(600);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);

        connect(fade, &QAbstractAnimation::finished, flash, &QLabel::deleteLater);
        fade->start();
    }
}

void Mode_1::playEliminateAnim(const QSet<QPoint>& points)
{
    // 【插入计分】
    if (!points.isEmpty()) {
        addScore(points.size());
    }

    // 【新增】播放消除音效
    // 计算消除数量并播放对应音效
    int elimCount = points.size();
    if (elimCount >= 3) {
        MusicManager::instance().playMatchSound(elimCount);
    }

    QParallelAnimationGroup *elimGroup = new QParallelAnimationGroup(this);

    for (const QPoint &p : points) {
        int idx = p.x() * COL + p.y();
        QPushButton *btn = m_cells[idx];
        if (!btn) continue;

        // 1. 创建动画：缩小 + 变透明
        QPropertyAnimation *scale = new QPropertyAnimation(btn, "geometry");
        scale->setDuration(250);
        QRect rect = btn->geometry();
        scale->setStartValue(rect);
        scale->setEndValue(rect.adjusted(24, 24, -24, -24)); // 向中心缩小

        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(btn);
        btn->setGraphicsEffect(eff);
        QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
        fade->setDuration(250);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);

        elimGroup->addAnimation(scale);
        elimGroup->addAnimation(fade);

        // 2. 逻辑层先把数据标记为 -1 (空)
        m_board->m_grid[p.x()][p.y()].pic = -1;
    }

    // 3. 动画结束后，执行物理删除和下落
    connect(elimGroup, &QAbstractAnimation::finished, this, [this, elimGroup, points](){
        // 真正的物理删除
        for (const QPoint &p : points) {
            int idx = p.x() * COL + p.y();
            if (m_cells[idx]) {
                delete m_cells[idx];    // 释放内存
                m_cells[idx] = nullptr; // 置空指针
            }
        }
        elimGroup->deleteLater();

        // 【新增】重置终极爆发标志位
        m_ultimateBurstActive = false;
        // 进入下落阶段
        performFallAnimation();
    });

    elimGroup->start();
}

/* mode_1.cpp - 优化后的 handleDeadlock */
void Mode_1::handleDeadlock()
{
    if (m_isLocked && !ui->btnBack->hasFocus()) return; // 避免重复触发，但允许测试按钮
    m_isLocked = true;

    // 1. 创建提示标签
    QLabel *lbl = new QLabel(ui->boardWidget);
    lbl->setText("死局！重新洗牌");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(
        "background-color: rgba(0, 0, 0, 180);"
        "color: #ff9de0;"
        "font: bold 26pt 'Microsoft YaHei';"
        "border-radius: 15px;"
        "border: 2px solid #ff9de0;"
        );
    lbl->adjustSize();
    lbl->resize(lbl->width() + 60, lbl->height() + 40);

    // 居中
    QRect cr = ui->boardWidget->rect();
    lbl->move((cr.width() - lbl->width()) / 2, (cr.height() - lbl->height()) / 2);
    lbl->show();
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents); // 【关键】让鼠标能穿透，虽然它锁住了

    // 2. 淡入淡出动画
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(lbl);
    lbl->setGraphicsEffect(eff);

    QPropertyAnimation *anim = new QPropertyAnimation(eff, "opacity");
    anim->setDuration(2000);
    anim->setKeyValueAt(0.0, 0.0);
    anim->setKeyValueAt(0.1, 1.0);
    anim->setKeyValueAt(0.8, 1.0);
    anim->setKeyValueAt(1.0, 0.0);

    connect(anim, &QAbstractAnimation::finished, this, [this, lbl](){
        lbl->deleteLater(); // 销毁标签

        // 重新生成棋盘 -> 这会触发 gridUpdated 信号 -> 进而触发 rebuildGrid -> createDropAnimation
        // createDropAnimation 结束后会自动执行 m_isLocked = false
        m_board->initNoThree();
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
/* mode_1.cpp - 修复后的 gameOver 函数 */
void Mode_1::gameOver()
{
    m_isLocked = true; // 锁住游戏，防止继续操作

    // 【新增】停止技能效果计时器
    if (m_skillEffectTimer && m_skillEffectTimer->isActive()) {
        m_skillEffectTimer->stop();
    }

    // ==========================================
    // 1. 数据库写入 - 修复：保存到 game_records 表，而不是用户表
    // ==========================================
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            QSqlQuery q(db);

            // A. 插入游戏记录到 game_records 表（这是排行榜读取的表）
            QString insertSql = QString(
                "INSERT INTO game_records (username, mode, score, time) VALUES (?, ?, ?, NOW())"
                );
            q.prepare(insertSql);
            q.addBindValue(m_username);
            q.addBindValue("闪电模式");  // 完整的模式名称，与排行榜查询一致
            q.addBindValue(m_score);    // 直接使用当前局得分

            if (!q.exec()) {
                qDebug() << "DB Error (Insert to game_records):" << q.lastError().text();
                qDebug() << "执行的SQL:" << insertSql;
                qDebug() << "参数: username=" << m_username << ", mode=闪电模式, score=" << m_score;
            } else {
                qDebug() << "游戏记录保存成功: 用户=" << m_username << ", 分数=" << m_score << ", 模式=闪电模式";
            }

            // B. 累加总分到 user 表（这是用户的累计积分）
            QSqlQuery qUp(db);
            qUp.prepare("UPDATE user SET points = points + ? WHERE username = ?");
            qUp.addBindValue(m_score);
            qUp.addBindValue(m_username);

            if (!qUp.exec()) {
                qDebug() << "DB Error (Update user points):" << qUp.lastError().text();
            } else {
                qDebug() << "用户累计积分更新成功: 用户=" << m_username << ", 增加分数=" << m_score;
            }

            NetworkManager *networkManager = NetworkManager::instance();
            if (networkManager && networkManager->isConnected()) {
                networkManager->updateUserStatus("在线", "空闲");
            }
        } else {
            qWarning() << "数据库连接未打开，无法保存记录";
        }
    }

    // ==========================================
    // 2. 创建自定义样式的弹窗
    // ==========================================
    QDialog dlg(this);
    dlg.setWindowTitle("结算");
    dlg.setFixedSize(320, 240);

    // 去掉标题栏的问号，保留关闭按钮
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // 深色背景 + 粉色边框 + 圆角
    dlg.setStyleSheet(
        "QDialog {"
        "   background-color: #162640;"          // 深灰
        "   border: none;"
        "   border-radius: 10px;"
        "}"
        "QLabel {"
        "   color: #ffffff;"
        "   font-family: 'Microsoft YaHei';"
        "   background: transparent;"
        "   border: none;"
        "}"
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2=0, stop:0 #ff9de0, stop:1 #ff6bcb);"
        "   border-radius: 15px;"
        "   color: white;"
        "   font: bold 14pt 'Microsoft YaHei';"
        "   height: 36px;"
        "}"
        "QPushButton:hover { background: #ff4fa5; }"
        "QPushButton:pressed { background: #e0438c; }"
        );

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(20);
    layout->setContentsMargins(30, 30, 30, 30);

    // 标题标签
    QLabel *lblTitle = new QLabel("GAME OVER", &dlg);
    lblTitle->setAlignment(Qt::AlignCenter);
    lblTitle->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ff9de0;");
    layout->addWidget(lblTitle);

    // 分数标签
    QLabel *lblScore = new QLabel(QString("本局得分: %1").arg(m_score), &dlg);
    lblScore->setAlignment(Qt::AlignCenter);
    lblScore->setStyleSheet("font-size: 18pt;");
    layout->addWidget(lblScore);

    // 确认按钮
    QPushButton *btnOk = new QPushButton("返回菜单", &dlg);
    connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(btnOk);

    // ==========================================
    // 3. 执行弹窗并处理返回
    // ==========================================
    dlg.exec();

    // 弹窗关闭后，执行这里：
    emit gameFinished(true); // 通知 MainWindow 切回菜单
}


/* mode_1.cpp - 新增返回按钮逻辑 */
void Mode_1::onBackButtonClicked()
{
    // 1. 暂停游戏逻辑
    bool wasRunning = m_gameTimer->isActive();
    m_gameTimer->stop();

    // 【新增】停止技能效果计时器
    if (m_skillEffectTimer && m_skillEffectTimer->isActive()) {
        m_skillEffectTimer->stop();
    }

    // 2. 创建自定义样式的确认弹窗
    QDialog dlg(this);
    dlg.setWindowTitle("确认返回");
    dlg.setFixedSize(360, 220);
    // 去掉标题栏问号
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // 统一样式（保持和结算界面一致的深色霓虹风）
    dlg.setStyleSheet(
        "QDialog {"
        "   background-color: #162640;"
        "   border: none;"                 // 去掉粉色边框
        "   border-radius: 10px;"
        "}"
        "QLabel {"
        "   color: #ffffff;"
        "   font: 14pt 'Microsoft YaHei';"
        "   background: transparent;"      // 去掉黑底
        "   border: none;"
        "}"
        "QPushButton {"
        "   border-radius: 15px;"
        "   color: white;"
        "   font: bold 12pt 'Microsoft YaHei';"
        "   height: 30px;"
        "   width: 80px;"
        "}"
        "QPushButton:pressed { padding-top: 2px; padding-left: 2px; }"
        );

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // 提示文字
    QLabel *lblMsg = new QLabel("若返回，当前对局信息丢失。\n是否确定返回？", &dlg);
    lblMsg->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(lblMsg);

    // 按钮布局 (水平排列)
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(40);
    btnLayout->addStretch();

    // 确定按钮 (红色系，警示)
    QPushButton *btnYes = new QPushButton("确定", &dlg);
    btnYes->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff5555, stop:1 #ff7777); }"
        "QPushButton:hover { background: #ff4444; }"
        );
    connect(btnYes, &QPushButton::clicked, &dlg, &QDialog::accept); // accept 代表确定
    btnLayout->addWidget(btnYes);

    // 取消按钮 (绿色系/蓝色系，安全)
    QPushButton *btnNo = new QPushButton("取消", &dlg);
    btnNo->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4caf50, stop:1 #66bb6a); }"
        "QPushButton:hover { background: #43a047; }"
        );
    connect(btnNo, &QPushButton::clicked, &dlg, &QDialog::reject); // reject 代表取消
    btnLayout->addWidget(btnNo);

    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // 3. 显示弹窗并等待结果 (阻塞式)
    int ret = dlg.exec();

    // 4. 根据结果处理
    if (ret == QDialog::Accepted) {
        // === 点击了"确定" ===
        // 直接触发结束信号，不写入数据库，相当于净身出户
        NetworkManager *networkManager = NetworkManager::instance();
        if (networkManager && networkManager->isConnected()) {
            networkManager->updateUserStatus("在线", "空闲");
        }
        emit gameFinished();
    } else {
        // === 点击了"取消" 或 叉掉了窗口 ===
        // 恢复游戏
        if (wasRunning) {
            m_gameTimer->start();
        }
    }
}


/* mode_1.cpp - 新增函数实现 */

// 事件过滤器：捕捉 labelScore 的点击
bool Mode_1::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->labelScore) {
        if (event->type() == QEvent::MouseButtonPress) {
            togglePause(); // 触发暂停切换
            return true;   // 事件已处理
        }
    }
    return QWidget::eventFilter(watched, event); // 其他事件交给父类处理
}

// 切换暂停/开始逻辑
void Mode_1::togglePause()
{
    // 只有游戏正式开始后（倒计时启动后）才能暂停，防止开场动画时乱点
    if (!m_hasGameStarted) return;

    if (m_isPaused) {
        // === 当前是暂停，切换为继续 ===
        m_isPaused = false;
        m_gameTimer->start(); // 恢复倒计时

        ui->labelScore->setText("暂停");
        ui->labelScore->setStyleSheet(
            "color:#fff; font:bold 15pt 'Microsoft YaHei';"
            //"background-color: transparent;"
            );

        // 可选：给个提示说游戏继续

    } else {
        // === 当前是进行中，切换为暂停 ===
        m_isPaused = true;
        m_gameTimer->stop(); // 停止倒计时

        ui->labelScore->setText("继续");
        ui->labelScore->setStyleSheet(
            "color:#ff9de0; font:bold 15pt 'Microsoft YaHei'; border:2px solid #ff9de0; border-radius:10px;"
            //"background-color: rgba(0,0,0,100);"
            );

        // 可选：可以在棋盘上覆盖一个"暂停中"的遮罩，这里为了简单只锁操作
    }
}


/* mode_1.cpp - 新增撤步相关函数 */

// 保存当前状态
void Mode_1::saveState()
{
    // 创建快照
    GameStateSnapshot snapshot;
    snapshot.grid = m_board->grid(); // 拷贝当前棋盘
    snapshot.score = m_score;        // 拷贝当前分数

    // 压入栈
    m_undoStack.push(snapshot);

    // 可选：限制步数，比如最多悔棋 5 步
    // if (m_undoStack.size() > 5) m_undoStack.removeFirst();
}

// 撤步按钮点击槽函数
void Mode_1::on_btnUndo_clicked()
{
    // 1. 基本校验
    if (m_isLocked || m_isPaused) return; // 动画中或暂停时不能悔棋
    if (m_undoStack.isEmpty()) {
        // 可以做个小动画提示"没有步骤可撤销"，或者直接忽略
        // qDebug() << "Stack empty";
        return;
    }

    // 2. 弹出上一步状态
    GameStateSnapshot lastState = m_undoStack.pop();

    // 3. 恢复数据
    m_board->m_grid = lastState.grid; // 覆盖棋盘数据
    m_score = lastState.score;        // 覆盖分数

    // 4. 更新 UI
    ui->labelScore_2->setText(QString("分数 %1").arg(m_score));

    // 【关键】不播放任何下落动画，直接刷新网格显示
    // rebuildGrid 会根据 m_board->grid() 重新创建所有按钮，看起来就是"瞬间还原"
    rebuildGrid();
}



void Mode_1::on_btnSkill_clicked()
{
    // 1. 基础状态检查
    if (m_isLocked || m_isPaused) return;

    if (!m_skillTree) {
        showTempMessage("技能树未初始化", QColor(255, 157, 224));
        return;
    }

    // 2. 筛选可用技能
    QList<SkillNode*> availableSkills;
    QList<SkillNode*> equippedSkills = m_skillTree->getEquippedSkills();
    for (SkillNode* skill : equippedSkills) {
        if (!skill->used) {
            availableSkills.append(skill);
        }
    }

    if (availableSkills.isEmpty()) {
        showTempMessage("没有可用技能", QColor(255, 157, 224));
        return;
    }

    // 3. 暂停游戏计时
    bool wasRunning = m_gameTimer->isActive();
    if (wasRunning) {
        m_gameTimer->stop();
    }

    // ============================================================
    //  UI 重构：现代化技能面板
    // ============================================================

    QDialog* skillDialog = new QDialog(this);
    skillDialog->setWindowTitle("SKILL SELECT");
    // 稍微调大一点尺寸，适应网格布局
    skillDialog->setFixedSize(500, 400);
    skillDialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog); // 无边框窗口，更科幻
    skillDialog->setAttribute(Qt::WA_DeleteOnClose);
    skillDialog->setAttribute(Qt::WA_TranslucentBackground); // 允许透明背景

    // 布局容器 (外层负责圆角和背景)
    QVBoxLayout* mainLayout = new QVBoxLayout(skillDialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFrame* bgFrame = new QFrame(skillDialog);
    mainLayout->addWidget(bgFrame);

    // 【核心样式】深蓝磨砂玻璃质感 + 霓虹粉边框
    bgFrame->setStyleSheet(
        "QFrame {"
        "   background-color: rgba(22, 38, 64, 240);" // 深蓝半透明
        "   border: 2px solid #ff9de0;"
        "   border-radius: 16px;"
        "}"
        "QLabel#Title {"
        "   color: #ff9de0;"
        "   font: bold 20pt 'Microsoft YaHei';"
        "   border: none;"
        "   background: transparent;"
        "}"
        );

    QVBoxLayout* contentLayout = new QVBoxLayout(bgFrame);
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(20, 20, 20, 20);

    // 标题栏
    QLabel* title = new QLabel("DEPLOY SKILL", bgFrame);
    title->setObjectName("Title"); // 关联上面的样式
    title->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(title);

    // 技能网格区域
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setSpacing(15); // 卡片间距
    contentLayout->addLayout(gridLayout);

    // 技能按钮样式 (卡片风格)
    QString btnStyle =
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 10);" // 极淡的白
        "   border: 1px solid rgba(255, 157, 224, 100);" // 淡粉边框
        "   border-radius: 8px;"
        "   color: white;"
        "   font: bold 12pt 'Microsoft YaHei';"
        "   padding: 10px;"
        "   text-align: center;"
        "}"
        "QPushButton:hover {"
        "   background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 157, 224, 40), stop:1 rgba(0, 229, 255, 40));"
        "   border: 1px solid #ff9de0;" // 悬停时边框变亮
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(255, 157, 224, 80);"
        "   padding-top: 12px; padding-left: 12px;" // 按压位移感
        "}";

    // 5. 循环生成技能卡片
    int row = 0;
    int col = 0;

    for (SkillNode* skill : availableSkills) {
        QPushButton* skillBtn = new QPushButton(skill->name, bgFrame);
        skillBtn->setToolTip(skill->description);
        skillBtn->setMinimumHeight(60); // 保证卡片高度
        skillBtn->setCursor(Qt::PointingHandCursor);
        skillBtn->setStyleSheet(btnStyle);

        // 添加到网格 (每行2个)
        gridLayout->addWidget(skillBtn, row, col);
        col++;
        if (col > 1) { col = 0; row++; }

        // 连接逻辑 (保持之前的特效逻辑完全不变)
        connect(skillBtn, &QPushButton::clicked, skillDialog, [this, skill, skillDialog, wasRunning]() {
            skillDialog->accept();

            int scoreToAdd = 0;
            int timeToAdd = 0;
            QString skillMessage;

            // --- 技能逻辑开始 (带特效) ---

            if (skill->id == "row_clear") {
                int row = QRandomGenerator::global()->bounded(ROW);
                playSpecialEffect(RowBomb, QPoint(row, 0), 0); // 特效
                QSet<QPoint> pts;
                for (int c = 0; c < COL; ++c) { if (m_board->m_grid[row][c].pic != -1) pts.insert(QPoint(row, c)); }
                if (!pts.isEmpty()) playEliminateAnim(pts);

            } else if (skill->id == "time_extend") {
                timeToAdd = 5;
                skillMessage = "TIME+5s";


            } else if (skill->id == "rainbow_bomb") {
                int color = QRandomGenerator::global()->bounded(6);
                playSpecialEffect(ColorClear, QPoint(ROW/2, COL/2), color); // 特效
                QSet<QPoint> pts;
                for (int r=0; r<ROW; ++r) { for (int c=0; c<COL; ++c) { if (m_board->m_grid[r][c].pic == color) pts.insert(QPoint(r, c)); }}
                if (!pts.isEmpty()) playEliminateAnim(pts);

            } else if (skill->id == "cross_clear") {
                int cR = QRandomGenerator::global()->bounded(ROW);
                int cC = QRandomGenerator::global()->bounded(COL);
                playSpecialEffect(RowBomb, QPoint(cR, cC), 0); // 特效
                playSpecialEffect(ColBomb, QPoint(cR, cC), 0); // 特效
                QSet<QPoint> pts;
                for (int c=0; c<COL; ++c) if (m_board->m_grid[cR][c].pic != -1) pts.insert(QPoint(cR, c));
                for (int r=0; r<ROW; ++r) if (m_board->m_grid[r][cC].pic != -1) pts.insert(QPoint(r, cC));
                if (!pts.isEmpty()) playEliminateAnim(pts);

            } else if (skill->id == "score_double") {
                m_scoreDoubleActive = true;
                m_skillEffectTimer->start(8000);
                skillMessage = "DOUBLE SCORE (8s)";

            } else if (skill->id == "color_unify") {
                m_colorUnifyActive = true;
                m_skillEffectTimer->start(6000);
                playSpecialEffect(ColorClear, QPoint(0,0), 0); // 特效
                int c1 = QRandomGenerator::global()->bounded(6);
                int c2 = QRandomGenerator::global()->bounded(6);
                int c3 = QRandomGenerator::global()->bounded(6);
                for (int r=0; r<ROW; ++r) { for (int c=0; c<COL; ++c) { if (m_board->m_grid[r][c].pic != -1) {
                            int ch = QRandomGenerator::global()->bounded(3);
                            m_board->m_grid[r][c].pic = (ch==0?c1 : (ch==1?c2:c3));
                        }}}
                skillMessage = "UNIFY COLOR (6s)";
                rebuildGrid();

            } else if (skill->id == "time_freeze") {
                timeToAdd = 15;
                skillMessage = "TIME FREEZE";
                QLabel *flash = new QLabel(ui->boardWidget);
                flash->setStyleSheet("background-color: rgba(0, 200, 255, 100);");
                flash->setGeometry(ui->boardWidget->rect());
                flash->show(); flash->setAttribute(Qt::WA_TransparentForMouseEvents);
                QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(flash); flash->setGraphicsEffect(eff);
                QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
                fade->setDuration(1000); fade->setStartValue(1.0); fade->setEndValue(0.0);
                connect(fade, &QAbstractAnimation::finished, flash, &QLabel::deleteLater); fade->start();

            } else if (skill->id == "ultimate_burst") {
                QSet<QPoint> pts;
                m_ultimateBurstActive = true;
                playSpecialEffect(ColorClear, QPoint(0,0), 0); // 特效
                for (int r=0; r<ROW; ++r) for (int c=0; c<COL; ++c) if (m_board->m_grid[r][c].pic != -1) pts.insert(QPoint(r, c));
                int sc = 3200; if (m_scoreDoubleActive) sc*=2;
                m_score += sc; ui->labelScore_2->setText(QString("分数 %1").arg(m_score));
                if (!pts.isEmpty()) playEliminateAnim(pts);
            }

            // --- 逻辑结束 ---

            if (!skillMessage.isEmpty()) showTempMessage(skillMessage, QColor(255, 157, 224));
            if (scoreToAdd > 0) { if (m_scoreDoubleActive) scoreToAdd *= 2; addScore(scoreToAdd); }
            if (timeToAdd > 0) { m_totalTime += timeToAdd; ui->labelCountdown->setText(QString("%1:%2").arg(m_totalTime/60, 2, 10, QChar('0')).arg(m_totalTime%60, 2, 10, QChar('0'))); }

            skill->used = true;
            if (wasRunning && !m_isPaused) m_gameTimer->start();
        });
    }

    // 填充空白处（如果技能数是奇数，网格会有点空，加个弹簧顶上去）
    contentLayout->addStretch();

    // 底部取消按钮 (设计成长的横条)
    QPushButton* cancelBtn = new QPushButton("CANCEL", bgFrame);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(0, 0, 0, 80);"
        "   border: 1px solid #555;"
        "   border-radius: 8px;"
        "   color: #aaa;"
        "   font: 10pt 'Microsoft YaHei';"
        "   height: 30px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 50, 50, 50);"
        "   border: 1px solid #ff5555;"
        "   color: white;"
        "}"
        );
    contentLayout->addWidget(cancelBtn);

    connect(cancelBtn, &QPushButton::clicked, skillDialog, [skillDialog, wasRunning, this]() {
        skillDialog->reject();
    });

    connect(skillDialog, &QDialog::finished, this, [skillDialog, wasRunning, this](int result) {
        if (result == QDialog::Rejected && wasRunning && !m_isPaused) {
            m_gameTimer->start();
        }
    });

    skillDialog->show();
}
// 【新增】显示临时消息函数
void Mode_1::showTempMessage(const QString& message, const QColor& color)
{
    if (!ui->boardWidget) return;

    QLabel* lbl = new QLabel(ui->boardWidget);
    lbl->setText(message);
    lbl->setAlignment(Qt::AlignCenter);

    // 使用传入的颜色
    QString colorStr = QString("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
    lbl->setStyleSheet(
        QString(
            "color: %1;"
            "font: bold 24pt 'Microsoft YaHei';"
            "background: transparent;"
            ).arg(colorStr)
        );

    lbl->adjustSize();
    QRect cr = ui->boardWidget->rect();
    lbl->move((cr.width() - lbl->width()) / 2, (cr.height() - lbl->height()) / 2);
    lbl->show();
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect* eff = new QGraphicsOpacityEffect(lbl);
    lbl->setGraphicsEffect(eff);

    // 快速淡入淡出动画
    QSequentialAnimationGroup* seq = new QSequentialAnimationGroup(this);

    QPropertyAnimation* fadeIn = new QPropertyAnimation(eff, "opacity");
    fadeIn->setDuration(300);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutQuad);
    seq->addAnimation(fadeIn);

    seq->addPause(1500); // 停留1.5秒

    QPropertyAnimation* fadeOut = new QPropertyAnimation(eff, "opacity");
    fadeOut->setDuration(300);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InQuad);
    seq->addAnimation(fadeOut);

    connect(seq, &QAbstractAnimation::finished, this, [lbl, seq](){
        lbl->deleteLater();
        seq->deleteLater();
    });

    seq->start();
}

/* mode_1.cpp - 提示功能实现 */

// 点击提示按钮
void Mode_1::on_btnHint_clicked()
{
    // 校验：没次数了、锁定了、暂停了、或者当前正在播放提示动画，都不处理
    if (m_hintCount <= 0 || m_isLocked || m_isPaused) return;
    if (m_hintAnimGroup && m_hintAnimGroup->state() == QAbstractAnimation::Running) return;

    // 执行查找逻辑
    int r1, c1, r2, c2;
    if (findValidMove(r1, c1, r2, c2)) {
        // 扣除次数
        m_hintCount--;
        ui->btnHint->setText(QString("提示 (%1)").arg(m_hintCount));

        // 如果次数用完，禁用按钮并变灰
        if (m_hintCount <= 0) {
            ui->btnHint->setEnabled(false);
            ui->btnHint->setStyleSheet(
                "QPushButton{ border:2px solid #555; background:#333; color:#888; border-radius:10px; }"
                );
        }

        // 高亮这两个方块
        m_hintBtns.clear();
        m_hintBtns.append(m_cells[r1 * COL + c1]);
        m_hintBtns.append(m_cells[r2 * COL + c2]);

        showHint();
    } else {
        // 理论上不会发生，因为有死局检测，但以防万一
        qDebug() << "No valid move found (Deadlock logic missing?)";
    }
}

// 遍历寻找可行解
bool Mode_1::findValidMove(int &r1, int &c1, int &r2, int &c2)
{
    // 简单的全盘遍历：只看右边和下边，减少重复计算
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            // 1. 尝试跟右边换
            if (c + 1 < COL) {
                if (m_board->trySwap(r, c, r, c + 1)) {
                    r1 = r; c1 = c; r2 = r; c2 = c + 1;
                    return true;
                }
            }
            // 2. 尝试跟下边换
            if (r + 1 < ROW) {
                if (m_board->trySwap(r, c, r + 1, c)) {
                    r1 = r; c1 = c; r2 = r + 1; c2 = c;
                    return true;
                }
            }
        }
    }
    return false;
}

// 启动高亮动画 (黄色呼吸光晕)
void Mode_1::showHint()
{
    if (m_hintBtns.isEmpty()) return;

    // 创建动画组
    if (m_hintAnimGroup) { m_hintAnimGroup->deleteLater(); }
    m_hintAnimGroup = new QSequentialAnimationGroup(this);
    m_hintAnimGroup->setLoopCount(-1); // 无限循环，直到用户点击

    QParallelAnimationGroup *pulseGroup = new QParallelAnimationGroup(this);

    for (QPushButton *btn : m_hintBtns) {
        // 给按钮加特效 (黄色光晕)
        QGraphicsDropShadowEffect *eff = new QGraphicsDropShadowEffect(btn);
        eff->setColor(QColor(255, 235, 59)); // 黄色
        eff->setBlurRadius(0);
        eff->setOffset(0, 0);
        btn->setGraphicsEffect(eff);

        // 动画：BlurRadius 从 0 -> 40 -> 0 (呼吸效果)
        QPropertyAnimation *anim = new QPropertyAnimation(eff, "blurRadius");
        anim->setDuration(800);
        anim->setStartValue(0);
        anim->setKeyValueAt(0.5, 40);
        anim->setEndValue(0);
        anim->setEasingCurve(QEasingCurve::InOutSine);

        pulseGroup->addAnimation(anim);
    }

    m_hintAnimGroup->addAnimation(pulseGroup);
    m_hintAnimGroup->start();
}

// 停止高亮动画
void Mode_1::stopHint()
{
    // 如果没有正在进行的提示，直接返回
    if (!m_hintAnimGroup && m_hintBtns.isEmpty()) return;

    // 1. 停止动画
    if (m_hintAnimGroup) {
        m_hintAnimGroup->stop();
        m_hintAnimGroup->deleteLater();
        m_hintAnimGroup = nullptr;
    }

    // 2. 清除特效
    for (QPushButton *btn : m_hintBtns) {
        if (btn) btn->setGraphicsEffect(nullptr); // 移除光晕
    }
    m_hintBtns.clear();
}

void Mode_1::resetSkills()
{
    if (!m_skillTree) return;

    // 重置所有技能的 used 状态
    const auto& skills = m_skillTree->getAllSkills();
    for (SkillNode* skill : skills) {
        skill->used = false;
    }

    // 重置技能效果状态
    m_scoreDoubleActive = false;
    m_colorUnifyActive = false;

    // 更新技能按钮样式（如果需要）
    // ui->btnSkill->setStyleSheet(...);
}

// mode_1.cpp - 优化后的技能效果结束提示
void Mode_1::onSkillEffectTimeout()
{
    // 技能效果结束提示
    if (m_scoreDoubleActive) {
        m_scoreDoubleActive = false;
        showSkillEndHint("得分翻倍效果结束");
    }

    if (m_colorUnifyActive) {
        m_colorUnifyActive = false;
        showSkillEndHint("颜色统一效果结束");
    }
}

// mode_1.cpp - 新增函数：显示技能结束提示（非干扰性）
void Mode_1::showSkillEndHint(const QString& message)
{
    if (!ui->boardWidget) return;

    // 创建提示标签
    QLabel* hintLabel = new QLabel(ui->boardWidget);
    hintLabel->setText(message);
    hintLabel->setAlignment(Qt::AlignCenter);

    // 样式：半透明，不遮挡游戏内容
    hintLabel->setStyleSheet(
        "background-color: rgba(0, 0, 0, 180);"
        "color: #ff9de0;"  // 粉色主题
        "font: bold 16pt 'Microsoft YaHei';"
        "border-radius: 8px;"
        "border: 1px solid #ff9de0;"
        "padding: 10px;"
        );

    // 计算位置：显示在棋盘上方，不遮挡主要内容
    QRect boardRect = ui->boardWidget->rect();
    int labelWidth = boardRect.width() * 0.8;  // 宽度为棋盘80%
    int labelHeight = 50;
    int labelX = boardRect.left() + (boardRect.width() - labelWidth) / 2;
    int labelY = boardRect.top() + 20;  // 顶部留出空间

    hintLabel->setGeometry(labelX, labelY, labelWidth, labelHeight);
    hintLabel->show();
    hintLabel->raise();  // 确保在最上层
    hintLabel->setAttribute(Qt::WA_TransparentForMouseEvents);  // 鼠标穿透

    // 创建淡入淡出动画
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(hintLabel);
    hintLabel->setGraphicsEffect(effect);

    QSequentialAnimationGroup* animGroup = new QSequentialAnimationGroup(this);

    // 淡入 (0.5秒)
    QPropertyAnimation* fadeIn = new QPropertyAnimation(effect, "opacity");
    fadeIn->setDuration(500);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::InOutQuad);

    // 停留 (2秒)
    animGroup->addAnimation(fadeIn);
    animGroup->addPause(2000);

    // 淡出 (0.5秒)
    QPropertyAnimation* fadeOut = new QPropertyAnimation(effect, "opacity");
    fadeOut->setDuration(500);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    animGroup->addAnimation(fadeOut);

    // 动画结束后清理
    connect(animGroup, &QAbstractAnimation::finished, this, [hintLabel, animGroup]() {
        hintLabel->deleteLater();
        animGroup->deleteLater();
    });

    animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}



