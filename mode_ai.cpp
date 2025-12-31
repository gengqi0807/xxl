#include "mode_ai.h"
#include "ui_mode_ai.h"
#include <QGridLayout>
#include <QPushButton>
#include <QDir>
#include <QRandomGenerator>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QDebug>
#include <QLabel>
#include <QDialog>
#include <QVBoxLayout>

Mode_AI::Mode_AI(GameBoard *board, QWidget *parent)
    : QWidget(parent), ui(new Ui::Mode_AI), m_board(board)
{
    ui->setupUi(this);

    // 初始化数据
    m_score = 0;
    m_totalTime = 300; // AI 演示模式给 5 分钟
    m_hasGameStarted = false;
    m_isLocked = true; // 初始锁定，等开场动画

    // 初始化 UI 显示
    ui->labelScore->setText("Score: 0");
    ui->labelCountdown->setText("05:00");

    m_dropGroup = new QSequentialAnimationGroup(this);
    m_gridLayout = new QGridLayout(ui->boardWidget);
    m_gridLayout->setSpacing(2);
    m_gridLayout->setContentsMargins(4, 4, 4, 4);

    connect(m_board, &GameBoard::gridUpdated, this, &Mode_AI::rebuildGrid);

    // 游戏倒计时定时器
    m_gameTimer = new QTimer(this);
    m_gameTimer->setInterval(1000); // 1秒一次
    connect(m_gameTimer, &QTimer::timeout, this, &Mode_AI::onTimerTick);

    // AI 思考定时器 (模拟思考延迟，避免操作太快眼花缭乱)
    m_aiThinkTimer = new QTimer(this);
    m_aiThinkTimer->setSingleShot(true);
    connect(m_aiThinkTimer, &QTimer::timeout, this, &Mode_AI::performAIMove);

    // 按钮连接
    connect(ui->btnBack, &QPushButton::clicked, this, &Mode_AI::onBackButtonClicked);

    // 初始构建网格 (这会触发 createDropAnimation)
    rebuildGrid();
}

Mode_AI::~Mode_AI()
{
    delete ui;
}

/* =========================================================
 * 1. 核心流程控制：初始化与重绘
 * ========================================================= */

void Mode_AI::rebuildGrid()
{
    clearGridLayout();
    m_cells.resize(ROW * COL);
    const Grid &gr = m_board->grid();

    QRect cr = ui->boardWidget->contentsRect();
    int availW = cr.width();
    int availH = cr.height();
    int cellSize = 48;
    int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;

    int ox = cr.left() + (availW - totalW) / 2;
    int oy = cr.top() + (availH - totalH) / 2;

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = new QPushButton(ui->boardWidget);
            btn->setFixedSize(cellSize, cellSize);
            btn->setStyleSheet("border:none;");
            QString path = QString("%1%2.png").arg(QDir::currentPath() + "/").arg(gr[r][c].pic);
            btn->setIcon(QIcon(path));
            btn->setIconSize(QSize(cellSize, cellSize));

            // AI 模式不需要点击交互，禁用点击或不连接槽函数
            btn->setAttribute(Qt::WA_TransparentForMouseEvents);

            int targetX = ox + c * (cellSize + gap);
            int targetY = oy + r * (cellSize + gap);

            btn->move(targetX, targetY - (ROW - r) * (cellSize + gap));
            auto *eff = new QGraphicsOpacityEffect(btn);
            eff->setOpacity(0.0);
            btn->setGraphicsEffect(eff);
            btn->show();
            m_cells[r * COL + c] = btn;
        }
    }

    createDropAnimation(ox, oy);
}

void Mode_AI::clearGridLayout()
{
    if (m_dropGroup) {
        m_dropGroup->stop();
        m_dropGroup->clear();
    }
    for (QPushButton *b : m_cells) {
        if (b) delete b;
    }
    m_cells.clear();

    if (m_gridLayout && ui->boardWidget) {
        QLayoutItem *item;
        while ((item = m_gridLayout->takeAt(0)) != nullptr) {
            if (item->widget()) delete item->widget();
            delete item;
        }
    }
}

void Mode_AI::createDropAnimation(int left0, int top0)
{
    if (m_dropGroup->state() == QAbstractAnimation::Running) {
        m_dropGroup->stop();
    }
    m_dropGroup->clear();
    m_dropGroup->disconnect(this);

    int cellSize = 48;
    int gap = 2;

    for (int r = ROW - 1; r >= 0; --r) {
        auto *rowPar = new QParallelAnimationGroup(this);
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = m_cells[r * COL + c];
            if (!btn) continue;

            auto *eff = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
            int targetX = left0 + c * (cellSize + gap);
            int targetY = top0 + r * (cellSize + gap);

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

        // 如果是首次加载完成，播放开场动画
        if (!m_hasGameStarted) {
            m_hasGameStarted = true;
            startGameSequence();
        } else {
            // 如果是死局重置后的重新加载，直接开始思考
            m_aiThinkTimer->start(100);
        }
    });

    m_dropGroup->start();
}

void Mode_AI::startGameSequence()
{
    m_isLocked = true;
    MusicManager::instance().playSceneMusic(MusicManager::MusicScene::Playing);

    QLabel *lbl = new QLabel(ui->boardWidget);
    lbl->setText("AI DEMO START");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color: #00e5ff; font: bold 32pt 'Microsoft YaHei'; background: transparent;");
    lbl->adjustSize();
    QRect cr = ui->boardWidget->rect();
    lbl->move((cr.width() - lbl->width()) / 2, (cr.height() - lbl->height()) / 2);
    lbl->show();

    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(lbl);
    lbl->setGraphicsEffect(eff);

    QSequentialAnimationGroup *seq = new QSequentialAnimationGroup(this);
    seq->addPause(500);
    QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
    fade->setDuration(1000);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    seq->addAnimation(fade);

    connect(seq, &QAbstractAnimation::finished, this, [this, lbl](){
        lbl->deleteLater();
        m_isLocked = false;
        m_gameTimer->start();

        // 【关键】启动 AI 第一次思考
        m_aiThinkTimer->start(10);
    });

    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void Mode_AI::onTimerTick()
{
    if (m_totalTime > 0) {
        m_totalTime--;
        int m = m_totalTime / 60;
        int s = m_totalTime % 60;
        ui->labelCountdown->setText(QString("%1:%2")
                                        .arg(m, 2, 10, QChar('0'))
                                        .arg(s, 2, 10, QChar('0')));
    } else {
        m_gameTimer->stop();
        m_aiThinkTimer->stop();
        // 时间到，直接退出
        emit gameFinished();
    }
}

void Mode_AI::onBackButtonClicked()
{
    m_gameTimer->stop();
    m_aiThinkTimer->stop();
    // 立即停止所有动画，防止回调访问野指针
    if (m_dropGroup) m_dropGroup->stop();
    emit gameFinished();
}

/* =========================================================
 * 2. AI 智能决策核心
 * ========================================================= */

Mode_AI::MoveChoice Mode_AI::calculateBestMove()
{
    MoveChoice bestMove = {-1, -1, -1, -1, -1};
    Grid rootGrid = m_board->grid();

    // 搜索深度：2层 (我看这一步，再往后看一步)
    // 警告：深度太深会导致计算非常慢 (指数级爆炸)
    int SEARCH_DEPTH = 1; // 建议先设为 1，配合 evaluatePotential 已经很强了。设为 2 会显著变慢。

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            int dirs[2][2] = {{0, 1}, {1, 0}};
            for (int k = 0; k < 2; ++k) {
                int nr = r + dirs[k][0];
                int nc = c + dirs[k][1];
                if (nr >= ROW || nc >= COL) continue;

                // 1. 模拟第一步
                std::swap(rootGrid[r][c].pic, rootGrid[nr][nc].pic);

                // 2. 计算第一步的直接收益
                ElimResult res1 = getEliminations(r, c, rootGrid);
                ElimResult res2 = getEliminations(nr, nc, rootGrid);

                int currentScore = 0;

                if (!res1.points.isEmpty() || !res2.points.isEmpty()) {
                    // --- 基础分计算 (和之前一样) ---
                    QSet<QPoint> allElims = res1.points;
                    allElims.unite(res2.points);
                    currentScore += allElims.size() * 20;

                    auto checkType = [&](EffectType t) {
                        if (t == ColorClear) return 200000;
                        if (t == AreaBomb)   return 80000;
                        if (t == RowBomb || t == ColBomb) return 40000;
                        return 0;
                    };
                    currentScore += checkType(res1.type);
                    currentScore += checkType(res2.type);

                    int lowerRow = qMax(r, nr);
                    currentScore += lowerRow * 100; // 重力优先

                    // --- 【差异点】 ---
                    // 不再只调用一次 evaluatePotential，而是调用 recursiveSearch
                    // 看看"如果我走了这一步，未来还有没有大分"

                    // 只有当这一步不是绝杀(比如5消)时，才去搜后续，节省时间
                    if (currentScore < 10000) {
                        // 去掉被消除的点，模拟剩下的残局
                        // (注：这里为了简化，不真的执行消除下落，直接在当前图上搜，是一种近似)
                        int futurePotential = recursiveSearch(rootGrid, SEARCH_DEPTH, -999999, 999999);
                        currentScore += futurePotential;
                    }
                }

                if (currentScore > bestMove.score) {
                    bestMove = {r, c, nr, nc, currentScore};
                }

                // 3. 回溯
                std::swap(rootGrid[r][c].pic, rootGrid[nr][nc].pic);
            }
        }
    }
    return bestMove;
}

void Mode_AI::performAIMove()
{
    if (m_isLocked) return;

    MoveChoice move = calculateBestMove();

    if (move.score > 0) {
        // 找到了有效移动，执行物理交换和逻辑处理
        // 注意：这里不需要 trySwap 的检查了，因为 calculateBestMove 已经模拟并确认有效

        // 逻辑层交换
        std::swap(m_board->m_grid[move.r1][move.c1].pic, m_board->m_grid[move.r2][move.c2].pic);

        // 视觉层处理 -> 进入 processInteraction -> checkComboMatches -> performFallAnimation -> next AI move
        processInteraction(move.r1, move.c1, move.r2, move.c2);

    } else {
        // AI 找不到移动了，可能是死局
        handleDeadlock();
    }
}

/* =========================================================
 * 3. 游戏逻辑与动画 (消除、特效、下落)
 * ========================================================= */

void Mode_AI::processInteraction(int r1, int c1, int r2, int c2)
{
    m_isLocked = true;

    // 视觉交换动画
    QPushButton *btn1 = m_cells[r1 * COL + c1];
    QPushButton *btn2 = m_cells[r2 * COL + c2];
    std::swap(m_cells[r1*COL+c1], m_cells[r2*COL+c2]); // 指针交换

    QParallelAnimationGroup *swapGroup = new QParallelAnimationGroup(this);
    QPropertyAnimation *a1 = new QPropertyAnimation(btn1, "pos");
    a1->setDuration(300); a1->setStartValue(btn1->pos()); a1->setEndValue(btn2->pos());
    QPropertyAnimation *a2 = new QPropertyAnimation(btn2, "pos");
    a2->setDuration(300); a2->setStartValue(btn2->pos()); a2->setEndValue(btn1->pos());
    swapGroup->addAnimation(a1); swapGroup->addAnimation(a2);

    connect(swapGroup, &QAbstractAnimation::finished, this, [this, swapGroup](){
        swapGroup->deleteLater();
        // 交换完成后，直接检查全盘消除
        checkComboMatches();
    });

    swapGroup->start();
}

void Mode_AI::checkComboMatches()
{
    QSet<QPoint> allMatches;
    QSet<QPoint> processedCenters;

    // 使用当前 grid 检查全盘
    const Grid& currentGrid = m_board->grid();

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            ElimResult res = getEliminations(r, c, currentGrid);
            if (!res.points.isEmpty()) {
                allMatches.unite(res.points);
                if (res.type != Normal && res.type != None) {
                    if (!processedCenters.contains(res.center)) {
                        playSpecialEffect(res.type, res.center, 0);
                        processedCenters.insert(res.center);
                    }
                }
            }
        }
    }

    if (!allMatches.isEmpty()) {
        m_isLocked = true;
        playEliminateAnim(allMatches);
    } else {
        // 这一轮稳定了，检查死局
        if (m_board->isDead(currentGrid)) {
            handleDeadlock();
        } else {
            m_isLocked = false;
            // 【闭环核心】：棋盘稳定了，启动计时器让 AI 思考下一步
            m_aiThinkTimer->start(100);
        }
    }
}

void Mode_AI::playEliminateAnim(const QSet<QPoint>& points)
{
    if (!points.isEmpty()) {
        addScore(points.size());
        int elimCount = points.size();
        if (elimCount >= 3) MusicManager::instance().playMatchSound(elimCount);
    }

    QParallelAnimationGroup *elimGroup = new QParallelAnimationGroup(this);
    for (const QPoint &p : points) {
        int idx = p.x() * COL + p.y();
        QPushButton *btn = m_cells[idx];
        if (!btn) continue;

        QPropertyAnimation *scale = new QPropertyAnimation(btn, "geometry");
        scale->setDuration(250);
        QRect rect = btn->geometry();
        scale->setStartValue(rect);
        scale->setEndValue(rect.adjusted(24, 24, -24, -24));

        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(btn);
        btn->setGraphicsEffect(eff);
        QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
        fade->setDuration(250);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);

        elimGroup->addAnimation(scale);
        elimGroup->addAnimation(fade);

        // 逻辑层置空
        m_board->m_grid[p.x()][p.y()].pic = -1;
    }

    connect(elimGroup, &QAbstractAnimation::finished, this, [this, elimGroup, points](){
        for (const QPoint &p : points) {
            int idx = p.x() * COL + p.y();
            if (m_cells[idx]) { delete m_cells[idx]; m_cells[idx] = nullptr; }
        }
        elimGroup->deleteLater();
        performFallAnimation();
    });

    elimGroup->start();
}

void Mode_AI::performFallAnimation()
{
    QRect cr = ui->boardWidget->contentsRect();
    int cellSize = 48; int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;

    QParallelAnimationGroup *fallGroup = new QParallelAnimationGroup(this);
    auto *rng = QRandomGenerator::global();

    struct BlockData { QPushButton* btn; int color; };
    for (QPushButton *b : m_cells) if (b) m_gridLayout->removeWidget(b);

    for (int c = 0; c < COL; ++c) {
        QList<BlockData> survivors;
        for (int r = ROW - 1; r >= 0; --r) {
            int idx = r * COL + c;
            if (m_cells[idx]) {
                survivors.append({m_cells[idx], m_board->m_grid[r][c].pic});
                m_cells[idx] = nullptr;
            }
        }
        int survivorIdx = 0;
        for (int r = ROW - 1; r >= 0; --r) {
            QPushButton *btn = nullptr;
            int finalColor = 0;
            int destX = ox + c*(cellSize+gap);
            int destY = oy + r*(cellSize+gap);

            if (survivorIdx < survivors.size()) {
                BlockData bd = survivors[survivorIdx++];
                btn = bd.btn; finalColor = bd.color;
            } else {
                finalColor = rng->bounded(6);
                btn = new QPushButton(ui->boardWidget);
                btn->setFixedSize(cellSize, cellSize);
                btn->setStyleSheet("border:none;");
                btn->setIcon(QIcon(QString("%1%2.png").arg(QDir::currentPath() + "/").arg(finalColor)));
                btn->setIconSize(QSize(cellSize, cellSize));
                btn->setAttribute(Qt::WA_TransparentForMouseEvents); // AI 模式按钮不可点
                btn->show();
                btn->move(destX, destY - (ROW*cellSize + 100));
            }
            m_cells[r*COL+c] = btn;
            m_board->m_grid[r][c].pic = finalColor;

            QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos");
            anim->setDuration(500);
            anim->setStartValue(btn->pos());
            anim->setEndValue(QPoint(destX, destY));
            anim->setEasingCurve(QEasingCurve::OutBounce);
            fallGroup->addAnimation(anim);
        }
    }

    connect(fallGroup, &QAbstractAnimation::finished, this, [this, fallGroup](){
        for(int r=0; r<ROW; ++r) for(int c=0; c<COL; ++c)
                if(m_cells[r*COL+c]) m_gridLayout->addWidget(m_cells[r*COL+c], r, c);

        fallGroup->deleteLater();
        checkComboMatches(); // 下落完成后再次检查连消
    });

    fallGroup->start();
}

void Mode_AI::handleDeadlock()
{
    m_isLocked = true;
    QLabel *lbl = new QLabel(ui->boardWidget);
    lbl->setText("Reshuffling...");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(
        "background-color: rgba(0, 0, 0, 180); color: #00e5ff; font: bold 26pt 'Microsoft YaHei'; border-radius: 15px; border: 2px solid #00e5ff;"
        );
    lbl->adjustSize();
    QRect cr = ui->boardWidget->rect();
    lbl->move((cr.width() - lbl->width()) / 2, (cr.height() - lbl->height()) / 2);
    lbl->show();

    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(lbl);
    lbl->setGraphicsEffect(eff);
    QPropertyAnimation *anim = new QPropertyAnimation(eff, "opacity");
    anim->setDuration(2000);
    anim->setKeyValueAt(0.0, 0.0);
    anim->setKeyValueAt(0.1, 1.0);
    anim->setKeyValueAt(0.8, 1.0);
    anim->setKeyValueAt(1.0, 0.0);

    connect(anim, &QAbstractAnimation::finished, this, [this, lbl](){
        lbl->deleteLater();
        m_board->initNoThree(); // 重新洗牌，触发 gridUpdated -> rebuildGrid
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

/* =========================================================
 * 4. 辅助判定与特效
 * ========================================================= */

void Mode_AI::addScore(int count)
{
    m_score += count * 10; // AI 模式简单计分
    ui->labelScore->setText(QString("Score: %1").arg(m_score));
}

// 支持传入 Grid 的消除判断函数
Mode_AI::ElimResult Mode_AI::getEliminations(int r, int c, const Grid& g)
{
    ElimResult res;
    res.center = QPoint(r, c);
    int color = g[r][c].pic;
    if (color == -1) return res;

    auto countDir = [&](int row, int col, int dr, int dc) {
        int cnt = 0;
        int nr = row + dr, nc = col + dc;
        while(nr>=0 && nr<ROW && nc>=0 && nc<COL && g[nr][nc].pic == color) {
            cnt++; nr+=dr; nc+=dc;
        }
        return cnt;
    };

    int up = countDir(r,c,-1,0);
    int down = countDir(r,c,1,0);
    int left = countDir(r,c,0,-1);
    int right = countDir(r,c,0,1);

    // 规则 1: 全屏消除 (魔鸟/闪电)
    if ((up + down >= 4) || (left + right >= 4)) {
        res.type = ColorClear;
        for (int i = 0; i < ROW; ++i) for (int j = 0; j < COL; ++j)
                if (g[i][j].pic == color) res.points.insert(QPoint(i, j));
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
        for (int i = r - 2; i <= r + 2; ++i) for (int j = c - 2; j <= c + 2; ++j)
                if (i >= 0 && i < ROW && j >= 0 && j < COL) res.points.insert(QPoint(i, j));
        return res;
    }

    // 规则 4: 普通三消
    if (up + down >= 2) {
        for (int i = r - up; i <= r + down; ++i) res.points.insert(QPoint(i, c));
        res.type = Normal;
    }
    if (left + right >= 2) {
        for (int j = c - left; j <= c + right; ++j) res.points.insert(QPoint(r, j));
        res.type = Normal;
    }

    return res;
}

void Mode_AI::playSpecialEffect(EffectType type, QPoint center, int colorCode)
{
    if (type == None || type == Normal) return;

    // 简化的特效，因为代码是复用的，这里简单实现一下视觉效果
    // 计算绝对坐标
    QRect cr = ui->boardWidget->contentsRect();
    int cellSize = 48; int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;
    int centerX = ox + center.y() * (cellSize + gap) + cellSize / 2;
    int centerY = oy + center.x() * (cellSize + gap) + cellSize / 2;

    if (type == RowBomb || type == ColBomb) {
        QLabel *beam = new QLabel(ui->boardWidget);
        beam->setAttribute(Qt::WA_TransparentForMouseEvents);
        beam->show();
        QString qss = "background: qlineargradient(x1:0, y1:0, x2:%1, y2:%2, stop:0 rgba(0,229,255,0), stop:0.5 rgba(0,229,255,230), stop:1 rgba(0,229,255,0));";
        QPropertyAnimation *anim = new QPropertyAnimation(beam, "geometry");
        anim->setDuration(400);
        anim->setEasingCurve(QEasingCurve::OutExpo);

        if (type == RowBomb) {
            beam->setStyleSheet(qss.arg("0").arg("1"));
            anim->setStartValue(QRect(ox, centerY - 2, totalW, 4));
            anim->setEndValue(QRect(ox, centerY - 25, totalW, 50));
        } else {
            beam->setStyleSheet(qss.arg("1").arg("0"));
            anim->setStartValue(QRect(centerX - 2, oy, 4, totalH));
            anim->setEndValue(QRect(centerX - 25, oy, 50, totalH));
        }

        connect(anim, &QAbstractAnimation::finished, beam, &QLabel::deleteLater);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    else if (type == AreaBomb) {
        QLabel *shock = new QLabel(ui->boardWidget);
        shock->setAttribute(Qt::WA_TransparentForMouseEvents);
        shock->setStyleSheet("background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 rgba(0,0,0,0), stop:0.7 rgba(255,235,59,200), stop:1 rgba(0,0,0,0)); border-radius:100px;");
        shock->show();
        QPropertyAnimation *anim = new QPropertyAnimation(shock, "geometry");
        anim->setDuration(500);
        anim->setStartValue(QRect(centerX, centerY, 0, 0));
        anim->setEndValue(QRect(centerX-150, centerY-150, 300, 300));
        connect(anim, &QAbstractAnimation::finished, shock, &QLabel::deleteLater);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}


int Mode_AI::evaluatePotential(const Grid& g, const QSet<QPoint>& ignoreCells)
{
    int potentialScore = 0;

    // 遍历全盘（横向和纵向扫描相邻对）
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            // 如果这个格子已经被消除了，跳过
            if (ignoreCells.contains(QPoint(r, c))) continue;

            int color = g[r][c].pic;
            if (color == -1) continue;

            // 检查右边
            if (c + 1 < COL && !ignoreCells.contains(QPoint(r, c+1))) {
                if (g[r][c+1].pic == color) potentialScore += 15; // 发现一个横向二连，加分
            }
            // 检查下边
            if (r + 1 < ROW && !ignoreCells.contains(QPoint(r+1, c))) {
                if (g[r+1][c].pic == color) potentialScore += 15; // 发现一个纵向二连，加分
            }
        }
    }
    return potentialScore;
}


int Mode_AI::recursiveSearch(Grid g, int depth, int alpha, int beta)
{
    // --- 1. 叶子节点 (Base Case) ---
    // 如果搜索深度耗尽，或者当前盘面已经死局，停止递归，返回当前盘面的“静态估值”
    if (depth == 0) {
        // 这里的估值 = 盘面潜在连击分 (evaluatePotential)
        // 注意：这里我们不计算消除分，只计算“好坏程度”，因为消除分已经在上一层叠加了
        return evaluatePotential(g, QSet<QPoint>());
    }

    int maxVal = -100000; // 初始化为极小值

    // --- 2. 树的展开 (Branching) ---
    // 遍历所有可能的移动（子节点）
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {

            // 优化：只向右和向下交换，去重
            int dirs[2][2] = {{0, 1}, {1, 0}};

            for (int k = 0; k < 2; ++k) {
                int nr = r + dirs[k][0];
                int nc = c + dirs[k][1];
                if (nr >= ROW || nc >= COL) continue;

                // 模拟交换
                std::swap(g[r][c].pic, g[nr][nc].pic);

                // 获取这一步的直接消除收益
                ElimResult res = getEliminations(r, c, g); // 注意：getEliminations 需要适配传入 g
                ElimResult res2 = getEliminations(nr, nc, g);

                int moveScore = 0;
                // 如果能消除
                if (!res.points.isEmpty() || !res2.points.isEmpty()) {
                    QSet<QPoint> allPts = res.points;
                    allPts.unite(res2.points);

                    // 1. 基础分
                    moveScore += allPts.size() * 10;

                    // 2. 特效分 (简化计算)
                    if (res.type != Normal && res.type != None) moveScore += 500;
                    if (res2.type != Normal && res2.type != None) moveScore += 500;

                    // 【关键递归】
                    // 这一步的价值 = 当前得分 + 下一步能得到的最大分 (递归调用)
                    // 注意：真实消消乐消除后会掉落，很难模拟。
                    // 这里我们采用"贪心近似"：假设消除后盘面不变，继续搜下一层。
                    // 这是一个权衡，为了能在有限时间内算出结果。
                    int futureVal = recursiveSearch(g, depth - 1, alpha, beta);
                    int totalVal = moveScore + futureVal;

                    if (totalVal > maxVal) {
                        maxVal = totalVal;
                    }

                    // Alpha-Beta 剪枝 (可选，加速搜索)
                    alpha = qMax(alpha, maxVal);
                    if (beta <= alpha) {
                        std::swap(g[r][c].pic, g[nr][nc].pic); // 还原
                        return maxVal; // 剪枝
                    }
                }

                // 还原交换 (Backtracking)
                std::swap(g[r][c].pic, g[nr][nc].pic);
            }
        }
    }

    // 如果这一层没有任何可行步，返回0
    return (maxVal == -100000) ? 0 : maxVal;
}

// =========================================================
//  根节点决策
// =========================================================
