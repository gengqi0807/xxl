#include "mode_2.h"
#include "ui_mode_2.h"
#include "networkmanager.h"
#include <QGridLayout>
#include <QMouseEvent>
#include <QDebug>
#include <QRandomGenerator>
#include <QDir>
#include <QMessageBox>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QDialog>
#include <QtSql> // 确保包含数据库相关头文件

Mode_2::Mode_2(GameBoard *board, QString username, QWidget *parent)
    : QWidget(parent), ui(new Ui::mode_2), m_board(board), m_username(username)
{
    ui->setupUi(this);

    // ============================================================
    // 【新增】覆盖 Mode_1 的粉色样式，改为 Mode_2 的青色主题
    // ============================================================

    // 1. 顶部栏下边框
    ui->topBar->setStyleSheet("background:#111; border-bottom:2px solid #00e5ff;"); // 粉色 #ff9de0 -> 青色 #00e5ff

    // 2. 棋盘外框
    ui->boardWidget->setStyleSheet("background:#162640; border:2px solid #00e5ff; border-radius:12px;");

    // 3. 倒计时文字颜色
    ui->labelCountdown->setStyleSheet("color:#00e5ff; font:700 24pt 'Microsoft YaHei';");

    // 4. 返回按钮 (青色渐变)
    ui->btnBack->setStyleSheet(
        "QPushButton{"
        "  color:#fff; font:14pt 'Microsoft YaHei'; border:none; border-radius:8px;"
        "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00e5ff, stop:1 #00b8d4);"
        "}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00b8d4, stop:1 #00838f);}"
        "QPushButton:pressed{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00838f, stop:1 #006064);}"
        );


    m_score = 0;
    m_totalTime = 180;

    // UI 初始化
    ui->labelScore->setText("暂停");
    ui->labelScore->setStyleSheet(
        "color:#fff; font:bold 15pt 'Microsoft YaHei'; "
        );
    ui->labelScore->setAlignment(Qt::AlignCenter);
    ui->labelScore->installEventFilter(this);
    ui->labelScore->setCursor(Qt::PointingHandCursor);

    ui->labelScore_2->setText("分数 0");
    ui->labelCountdown->setText("03:00");
    ui->btnHint->setText(QString("提示 (%1)").arg(m_hintCount));

    m_gridLayout = new QGridLayout(ui->boardWidget);
    m_gridLayout->setSpacing(2);
    m_gridLayout->setContentsMargins(4, 4, 4, 4);

    // 【核心】开启鼠标追踪
    ui->boardWidget->setMouseTracking(true);
    ui->boardWidget->installEventFilter(this);

    // 创建光圈
    m_selectorFrame = new QLabel(ui->boardWidget);
    m_selectorFrame->setStyleSheet("border: 3px solid #00e5ff; border-radius: 15px; background: transparent;");
    m_selectorFrame->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_selectorFrame->hide();

    // 动画组初始化
    m_dropGroup = new QSequentialAnimationGroup(this);

    m_gameTimer = new QTimer(this);
    m_gameTimer->setInterval(1000);
    connect(m_gameTimer, &QTimer::timeout, this, &Mode_2::onTimerTick);

    connect(m_board, &GameBoard::gridUpdated, this, &Mode_2::rebuildGrid);
    connect(ui->btnBack, &QPushButton::clicked, this, &Mode_2::onBackButtonClicked);

    // 【新增】初始化技能效果计时器
    m_skillEffectTimer = new QTimer(this);
    m_skillEffectTimer->setSingleShot(true);
    connect(m_skillEffectTimer, &QTimer::timeout, this, &Mode_2::onSkillEffectTimeout);

    rebuildGrid();

    // 获取网络管理器并更新状态为"游戏中"
    NetworkManager *networkManager = NetworkManager::instance();
    if (networkManager && networkManager->isConnected()) {
        networkManager->updateUserStatus("游戏中", "旋风");
    }
}

Mode_2::~Mode_2() { delete ui; }

/* =========================================================
 * 1. 交互逻辑 (鼠标追踪与点击)
 * ========================================================= */

bool Mode_2::eventFilter(QObject *watched, QEvent *event)
{
    // 暂停按钮
    if (watched == ui->labelScore && event->type() == QEvent::MouseButtonPress) {
        togglePause();
        return true;
    }

    // 棋盘交互
    if (watched == ui->boardWidget && !m_isLocked && !m_isPaused && m_hasGameStarted) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            updateSelectorPos(me->pos());
            return true;
        }
        else if (event->type() == QEvent::MouseButtonPress) {
            // 点击时停止提示动画
            stopHint();
            tryRotateInteraction();
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            m_selectorFrame->hide();
            m_selR = -1; m_selC = -1;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Mode_2::updateSelectorPos(QPoint mousePos)
{
    int cellSize = 48; int gap = 2;
    QRect cr = ui->boardWidget->contentsRect();
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;

    // 计算鼠标位于哪个 2x2 网格的缝隙
    int c = (mousePos.x() - ox - (cellSize/2)) / (cellSize + gap);
    int r = (mousePos.y() - oy - (cellSize/2)) / (cellSize + gap);

    if (r < 0) r = 0; if (r > ROW - 2) r = ROW - 2;
    if (c < 0) c = 0; if (c > COL - 2) c = COL - 2;

    m_selR = r; m_selC = c;

    int frameSize = (cellSize * 2) + gap + 4;
    int targetX = ox + c * (cellSize + gap) - 2;
    int targetY = oy + r * (cellSize + gap) - 2;

    m_selectorFrame->resize(frameSize, frameSize);
    m_selectorFrame->move(targetX, targetY);
    m_selectorFrame->show();
    m_selectorFrame->raise();
}

void Mode_2::tryRotateInteraction()
{
    if (m_selR == -1 || m_selC == -1) return;

    // 1. 判定逻辑：旋转后是否能消除？
    bool isValid = m_board->tryRotate(m_selR, m_selC);

    if (isValid) {
        saveState(); // 保存状态用于悔棋

        // 2. 只有有效时，才真正改变数据
        Grid &g = m_board->m_grid;
        Spot temp = g[m_selR][m_selC];
        g[m_selR][m_selC]       = g[m_selR+1][m_selC];
        g[m_selR+1][m_selC]     = g[m_selR+1][m_selC+1];
        g[m_selR+1][m_selC+1]   = g[m_selR][m_selC+1];
        g[m_selR][m_selC+1]     = temp;

        // 3. 执行旋转动画
        processRotation(m_selR, m_selC);
    } else {
        // 4. 无效：光圈抖动 (Shake)
        QPropertyAnimation *shake = new QPropertyAnimation(m_selectorFrame, "pos");
        shake->setDuration(200);
        QPoint p = m_selectorFrame->pos();
        shake->setKeyValueAt(0, p);
        shake->setKeyValueAt(0.25, p + QPoint(5, 0));
        shake->setKeyValueAt(0.75, p - QPoint(5, 0));
        shake->setKeyValueAt(1, p);
        shake->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void Mode_2::processRotation(int r, int c)
{
    m_isLocked = true;
    m_selectorFrame->hide();

    QPushButton *bTL = m_cells[r * COL + c];
    QPushButton *bTR = m_cells[r * COL + c + 1];
    QPushButton *bBR = m_cells[(r + 1) * COL + c + 1];
    QPushButton *bBL = m_cells[(r + 1) * COL + c];

    // 更新按钮数组指针
    m_cells[r * COL + c]           = bBL;
    m_cells[r * COL + c + 1]       = bTL;
    m_cells[(r + 1) * COL + c + 1] = bTR;
    m_cells[(r + 1) * COL + c]     = bBR;

    QParallelAnimationGroup *grp = new QParallelAnimationGroup(this);
    int dur = 200; // 旋转速度快一点

    auto createAnim = [&](QPushButton *b, QPoint endPos) {
        QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
        anim->setDuration(dur);
        anim->setStartValue(b->pos());
        anim->setEndValue(endPos);
        anim->setEasingCurve(QEasingCurve::OutQuad);
        return anim;
    };

    grp->addAnimation(createAnim(bTL, bTR->pos()));
    grp->addAnimation(createAnim(bTR, bBR->pos()));
    grp->addAnimation(createAnim(bBR, bBL->pos()));
    grp->addAnimation(createAnim(bBL, bTL->pos()));

    connect(grp, &QAbstractAnimation::finished, this, [this, grp, r, c](){
        grp->deleteLater();

        QSet<QPoint> allMatches;
        // 检查受影响的四个格子的消除情况
        ElimResult r1 = getEliminations(r, c);
        ElimResult r2 = getEliminations(r, c+1);
        ElimResult r3 = getEliminations(r+1, c);
        ElimResult r4 = getEliminations(r+1, c+1);

        allMatches.unite(r1.points);
        allMatches.unite(r2.points);
        allMatches.unite(r3.points);
        allMatches.unite(r4.points);

        // 播放特效（优先播放高级特效）
        if (r1.type != Normal && r1.type != None) playSpecialEffect(r1.type, r1.center, 0);
        else if (r2.type != Normal && r2.type != None) playSpecialEffect(r2.type, r2.center, 0);

        if (!allMatches.isEmpty()) {
            playEliminateAnim(allMatches);
        } else {
            // 如果走到这里说明判断失误，强制重绘保证对齐
            rebuildGrid();
            m_isLocked = false;
        }
    });

    grp->start();
}

/* =========================================================
 * 2. 核心与动画 (Rebuild, Fall, Check)
 * ========================================================= */

void Mode_2::rebuildGrid()
{
    clearGridLayout();
    m_cells.resize(ROW * COL);
    const Grid &gr = m_board->grid();

    QRect cr = ui->boardWidget->contentsRect();
    int cellSize = 48; int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;

    for (int r = 0; r < ROW; ++r)
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = new QPushButton(ui->boardWidget);
            btn->setFixedSize(cellSize, cellSize);
            btn->setStyleSheet("border:none;");
            // 【关键】必须穿透鼠标，否则遮挡 boardWidget 的事件过滤器
            btn->setAttribute(Qt::WA_TransparentForMouseEvents);

            QString path = QString("%1%2.png").arg(QDir::currentPath() + "/").arg(gr[r][c].pic);
            btn->setIcon(QIcon(path));
            btn->setIconSize(QSize(cellSize, cellSize));

            int targetX = ox + c * (cellSize + gap);
            int targetY = oy + r * (cellSize + gap);

            // 【修复 1】初始位置：直接移到和 createDropAnimation 一样的高空位置
            // 之前是 (ROW - r) 导致的阶梯状虚影，现在统一拉到最高
            int startY = targetY - (ROW * cellSize + 100);
            btn->move(targetX, startY);

            // 【修复 2】初始透明度：设置为 0 (完全不可见)
            // 这样在动画开始前，用户绝对看不到任何"虚影"
            QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(btn);
            eff->setOpacity(0.0);
            btn->setGraphicsEffect(eff);

            btn->show();
            m_cells[r * COL + c] = btn;
        }

    createDropAnimation(ox, oy);
}
void Mode_2::createDropAnimation(int left0, int top0)
{
    if (m_dropGroup->state() == QAbstractAnimation::Running) {
        m_dropGroup->stop();
    }
    m_dropGroup->clear();
    m_dropGroup->disconnect(this);

    int cellSize = 48; int gap = 2;

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
            drop->setDuration(180 + (ROW - 1 - r) * 15); // 稍微调慢一点点，更有重量感

            // 【修正点】起始位置设为由 targetY 向上偏移整个棋盘高度，确保从画外落下
            drop->setStartValue(QPoint(targetX, targetY - (ROW * cellSize + 100)));
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
        // m_dropGroup->addPause(1); // 可以去掉微小间隔，让整体下落更流畅
    }

    connect(m_dropGroup, &QSequentialAnimationGroup::finished, this, [this]() {
        for (int r = 0; r < ROW; ++r) {
            for (int c = 0; c < COL; ++c) {
                if (m_cells[r*COL+c]) m_gridLayout->addWidget(m_cells[r*COL+c], r, c);
            }
        }
        m_isLocked = false;
        if (!m_hasGameStarted) {
            m_hasGameStarted = true;
            startGameSequence();
        }
    });
    m_dropGroup->start();
}

void Mode_2::performFallAnimation()
{
    // 与 Mode_1 逻辑一致，但生成新按钮时要注意属性
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
            int destX = ox + c * (cellSize + gap);
            int destY = oy + r * (cellSize + gap);

            if (survivorIdx < survivors.size()) {
                BlockData bd = survivors[survivorIdx++];
                btn = bd.btn;
                finalColor = bd.color;
            } else {
                finalColor = rng->bounded(6);
                btn = new QPushButton(ui->boardWidget);
                btn->setFixedSize(cellSize, cellSize);
                btn->setStyleSheet("border:none;");
                btn->setAttribute(Qt::WA_TransparentForMouseEvents); // 【关键】新生成的也要穿透

                QString path = QString("%1%2.png").arg(QDir::currentPath() + "/").arg(finalColor);
                btn->setIcon(QIcon(path));
                btn->setIconSize(QSize(cellSize, cellSize));
                btn->show();
                btn->move(destX, destY - (ROW * cellSize + 100));
            }
            m_cells[r * COL + c] = btn;
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
        for (QPushButton *b : m_cells) if (b) m_gridLayout->addWidget(b); // 暂不精确坐标，只要塞进去
        fallGroup->deleteLater();
        checkComboMatches();
    });
    fallGroup->start();
}

void Mode_2::checkComboMatches()
{
    QSet<QPoint> allMatches;
    QSet<QPoint> processedCenters;

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            ElimResult res = getEliminations(r, c);
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
        if (m_board->isDead(m_board->grid())) handleDeadlock();
        else {
            m_isLocked = false;
            // 恢复光圈显示
            if (ui->boardWidget->underMouse()) m_selectorFrame->show();
        }
    }
}

void Mode_2::clearGridLayout()
{
    // 【新增】停止技能效果计时器
    if (m_skillEffectTimer && m_skillEffectTimer->isActive()) {
        m_skillEffectTimer->stop();
    }
    if (m_dropGroup) { m_dropGroup->stop(); m_dropGroup->clear(); }
    for (QPushButton *b : m_cells) if (b) delete b;
    m_cells.clear();
    // 强制清理布局
    if (m_gridLayout) {
        QLayoutItem *item;
        while ((item = m_gridLayout->takeAt(0)) != nullptr) {
            if (item->widget()) delete item->widget();
            delete item;
        }
    }
}

/* =========================================================
 * 3. 提示系统 (修改为旋转逻辑)
 * ========================================================= */

void Mode_2::on_btnHint_clicked()
{
    if (m_hintCount <= 0 || m_isLocked || m_isPaused) return;
    if (m_hintAnimGroup && m_hintAnimGroup->state() == QAbstractAnimation::Running) return;

    int r, c;
    if (findValidMove(r, c)) { // 查找有效旋转
        m_hintCount--;
        ui->btnHint->setText(QString("提示 (%1)").arg(m_hintCount));
        if (m_hintCount <= 0) {
            ui->btnHint->setEnabled(false);
            ui->btnHint->setStyleSheet("QPushButton{ border:2px solid #555; background:#333; color:#888; border-radius:10px; }");
        }
        showHint(r, c); // 高亮 r,c 对应的 2x2 区域
    }
}

bool Mode_2::findValidMove(int &outR, int &outC)
{
    // 遍历所有可能的 2x2 区域 (左上角从 0,0 到 ROW-2, COL-2)
    for (int r = 0; r < ROW - 1; ++r) {
        for (int c = 0; c < COL - 1; ++c) {
            if (m_board->tryRotate(r, c)) {
                outR = r; outC = c;
                return true;
            }
        }
    }
    return false;
}


/* =========================================================
 * 4. 辅助函数 (特效、悔棋、结算等 - 保持原样)
 * ========================================================= */

void Mode_2::saveState() {
    GameStateSnapshot snapshot;
    snapshot.grid = m_board->grid();
    snapshot.score = m_score;
    m_undoStack.push(snapshot);
}

void Mode_2::on_btnUndo_clicked() {
    if (m_isLocked || m_isPaused || m_undoStack.isEmpty()) return;
    GameStateSnapshot last = m_undoStack.pop();
    m_board->m_grid = last.grid;
    m_score = last.score;
    ui->labelScore_2->setText(QString("分数 %1").arg(m_score));
    rebuildGrid();
}

void Mode_2::playEliminateAnim(const QSet<QPoint>& points) {
    if (!points.isEmpty()) addScore(points.size());

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
        m_board->m_grid[p.x()][p.y()].pic = -1;
    }

    connect(elimGroup, &QAbstractAnimation::finished, this, [this, elimGroup, points](){
        for (const QPoint &p : points) {
            int idx = p.x() * COL + p.y();
            if (m_cells[idx]) { delete m_cells[idx]; m_cells[idx] = nullptr; }
        }
        elimGroup->deleteLater();
        // 【新增】重置终极爆发标志位
        m_ultimateBurstActive = false;
        performFallAnimation();
    });
    elimGroup->start();
}

// 复制 Mode_1 的 getEliminations, playSpecialEffect, countDirection, handleDeadlock 等函数
// 为节省篇幅，这里假设逻辑相同。请确保 getEliminations 已被正确复制。
// 下面是必要的实现：

Mode_2::ElimResult Mode_2::getEliminations(int r, int c) {
    ElimResult res; res.center = QPoint(r, c);
    int color = m_board->m_grid[r][c].pic;
    if (color == -1) return res;
    int up = countDirection(r, c, -1, 0); int down = countDirection(r, c, 1, 0);
    int left = countDirection(r, c, 0, -1); int right = countDirection(r, c, 0, 1);

    if ((up + down >= 4) || (left + right >= 4)) {
        res.type = ColorClear;
        for (int i=0; i<ROW; ++i) for (int j=0; j<COL; ++j) if(m_board->m_grid[i][j].pic==color) res.points.insert(QPoint(i,j));
        return res;
    }
    if (up + down == 3) { res.type = ColBomb; for(int i=0; i<ROW; ++i) res.points.insert(QPoint(i, c)); return res; }
    if (left + right == 3) { res.type = RowBomb; for(int j=0; j<COL; ++j) res.points.insert(QPoint(r, j)); return res; }
    if (up + down >= 2 && left + right >= 2) {
        res.type = AreaBomb;
        for(int i=r-2; i<=r+2; ++i) for(int j=c-2; j<=c+2; ++j) if(i>=0 && i<ROW && j>=0 && j<COL) res.points.insert(QPoint(i, j));
        return res;
    }
    if (up + down >= 2) { res.type = Normal; for(int i=r-up; i<=r+down; ++i) res.points.insert(QPoint(i,c)); }
    if (left + right >= 2) { res.type = Normal; for(int j=c-left; j<=c+right; ++j) res.points.insert(QPoint(r,j)); }
    return res;
}

int Mode_2::countDirection(int r, int c, int dR, int dC) {
    int color = m_board->m_grid[r][c].pic;
    int cnt = 0; int nr = r+dR; int nc = c+dC;
    while(nr>=0 && nr<ROW && nc>=0 && nc<COL && m_board->m_grid[nr][nc].pic == color) { cnt++; nr+=dR; nc+=dC; }
    return cnt;
}

void Mode_2::handleDeadlock() {
    // 复制 Mode_1::handleDeadlock
    if (m_isLocked && !ui->btnBack->hasFocus()) return;
    m_isLocked = true;
    QLabel *lbl = new QLabel(ui->boardWidget);
    lbl->setText("死局！重新洗牌");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("background-color:rgba(0,0,0,180); color:#00e5ff; font:bold 26pt 'Microsoft YaHei'; border-radius:15px; border:2px solid #00e5ff;");
    lbl->adjustSize(); lbl->resize(lbl->width()+60, lbl->height()+40);
    QRect cr = ui->boardWidget->rect();
    lbl->move((cr.width()-lbl->width())/2, (cr.height()-lbl->height())/2);
    lbl->show();
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
    QTimer::singleShot(2000, [this, lbl](){
        lbl->deleteLater();
        m_board->initNoThree();
    });
}


void Mode_2::startGameSequence() {
    m_isLocked = true;
    resetSkills();

    // 【新增】播放游戏过程背景音乐
    MusicManager::instance().playSceneMusic(MusicManager::MusicScene::Playing);

    QLabel *lbl = new QLabel(ui->boardWidget);
    lbl->setText("Ready Go!"); lbl->setStyleSheet("color:#00e5ff; font:bold 40pt 'Microsoft YaHei';");
    lbl->adjustSize();
    QRect cr = ui->boardWidget->rect();
    lbl->move((cr.width()-lbl->width())/2, (cr.height()-lbl->height())/2);
    lbl->show();
    QTimer::singleShot(1000, [this, lbl](){ lbl->deleteLater(); m_isLocked=false; m_gameTimer->start(); });
}

void Mode_2::onTimerTick() {
    if (m_totalTime > 0) {
        m_totalTime--;
        ui->labelCountdown->setText(QString("%1:%2").arg(m_totalTime/60, 2, 10, QChar('0')).arg(m_totalTime%60, 2, 10, QChar('0')));
    } else { m_gameTimer->stop(); gameOver(); }
}

void Mode_2::addScore(int count)
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
void Mode_2::togglePause() {
    if (!m_hasGameStarted) return;
    m_isPaused = !m_isPaused;
    if (m_isPaused) { m_gameTimer->stop(); ui->labelScore->setText("继续"); }
    else { m_gameTimer->start(); ui->labelScore->setText("暂停"); }
}

void Mode_2::on_btnSkill_clicked()
{
    // 1. 基础状态检查
    if (m_isLocked || m_isPaused) return;

    if (!m_skillTree) {
        showTempMessage("技能树未初始化", QColor(0, 229, 255));
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
        showTempMessage("没有可用技能", QColor(0, 229, 255));
        return;
    }

    // 3. 暂停游戏计时
    bool wasRunning = m_gameTimer->isActive();
    if (wasRunning) {
        m_gameTimer->stop();
    }

    // ============================================================
    //  UI 重构：青色系技能面板
    // ============================================================

    QDialog* skillDialog = new QDialog(this);
    skillDialog->setWindowTitle("SKILL SELECT");
    skillDialog->setFixedSize(500, 400);
    skillDialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    skillDialog->setAttribute(Qt::WA_DeleteOnClose);
    skillDialog->setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout* mainLayout = new QVBoxLayout(skillDialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFrame* bgFrame = new QFrame(skillDialog);
    mainLayout->addWidget(bgFrame);

    // 【青色主题样式】
    bgFrame->setStyleSheet(
        "QFrame {"
        "   background-color: rgba(22, 38, 64, 240);" // 深蓝背景
        "   border: 2px solid #00e5ff;"               // 青色边框
        "   border-radius: 16px;"
        "}"
        "QLabel#Title {"
        "   color: #00e5ff;"                          // 青色标题
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
    title->setObjectName("Title");
    title->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(title);

    // 技能网格区域
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setSpacing(15);
    contentLayout->addLayout(gridLayout);

    // 【青色按钮样式】
    QString btnStyle =
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 10);"
        "   border: 1px solid rgba(0, 229, 255, 100);" // 青色半透明边框
        "   border-radius: 8px;"
        "   color: white;"
        "   font: bold 12pt 'Microsoft YaHei';"
        "   padding: 10px;"
        "   text-align: center;"
        "}"
        "QPushButton:hover {"
        // 悬停时青色渐变
        "   background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(0, 229, 255, 40), stop:1 rgba(0, 184, 212, 40));"
        "   border: 1px solid #00e5ff;" // 悬停亮边
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(0, 229, 255, 80);"
        "   padding-top: 12px; padding-left: 12px;"
        "}";

    // 5. 循环生成技能卡片
    int row = 0;
    int col = 0;

    for (SkillNode* skill : availableSkills) {
        QPushButton* skillBtn = new QPushButton(skill->name, bgFrame);
        skillBtn->setToolTip(skill->description);
        skillBtn->setMinimumHeight(60);
        skillBtn->setCursor(Qt::PointingHandCursor);
        skillBtn->setStyleSheet(btnStyle);

        gridLayout->addWidget(skillBtn, row, col);
        col++;
        if (col > 1) { col = 0; row++; }

        connect(skillBtn, &QPushButton::clicked, skillDialog, [this, skill, skillDialog, wasRunning]() {
            skillDialog->accept();

            int scoreToAdd = 0;
            int timeToAdd = 0;
            QString skillMessage;

            // --- 技能逻辑 (保持原有逻辑不变) ---
            if (skill->id == "row_clear") {
                int row = QRandomGenerator::global()->bounded(ROW);
                playSpecialEffect(RowBomb, QPoint(row, 0), 0);
                QSet<QPoint> pts;
                for (int c = 0; c < COL; ++c) { if (m_board->m_grid[row][c].pic != -1) pts.insert(QPoint(row, c)); }
                if (!pts.isEmpty()) playEliminateAnim(pts);

            } else if (skill->id == "time_extend") {
                timeToAdd = 5;
                skillMessage = "TIME +5s";

            } else if (skill->id == "rainbow_bomb") {
                int color = QRandomGenerator::global()->bounded(6);
                playSpecialEffect(ColorClear, QPoint(ROW/2, COL/2), color);
                QSet<QPoint> pts;
                for (int r=0; r<ROW; ++r) { for (int c=0; c<COL; ++c) { if (m_board->m_grid[r][c].pic == color) pts.insert(QPoint(r, c)); }}
                if (!pts.isEmpty()) playEliminateAnim(pts);

            } else if (skill->id == "cross_clear") {
                int cR = QRandomGenerator::global()->bounded(ROW);
                int cC = QRandomGenerator::global()->bounded(COL);
                playSpecialEffect(RowBomb, QPoint(cR, cC), 0);
                playSpecialEffect(ColBomb, QPoint(cR, cC), 0);
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
                playSpecialEffect(ColorClear, QPoint(0,0), 0);
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
                flash->setStyleSheet("background-color: rgba(0, 229, 255, 100);"); // 青色冰冻
                flash->setGeometry(ui->boardWidget->rect());
                flash->show(); flash->setAttribute(Qt::WA_TransparentForMouseEvents);
                QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(flash); flash->setGraphicsEffect(eff);
                QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
                fade->setDuration(1000); fade->setStartValue(1.0); fade->setEndValue(0.0);
                connect(fade, &QAbstractAnimation::finished, flash, &QLabel::deleteLater); fade->start();

            } else if (skill->id == "ultimate_burst") {
                QSet<QPoint> pts;
                m_ultimateBurstActive = true;
                playSpecialEffect(ColorClear, QPoint(0,0), 0);
                for (int r=0; r<ROW; ++r) for (int c=0; c<COL; ++c) if (m_board->m_grid[r][c].pic != -1) pts.insert(QPoint(r, c));
                int sc = 3200; if (m_scoreDoubleActive) sc*=2;
                m_score += sc; ui->labelScore_2->setText(QString("分数 %1").arg(m_score));
                if (!pts.isEmpty()) playEliminateAnim(pts);
            }

            if (!skillMessage.isEmpty()) showTempMessage(skillMessage, QColor(0, 229, 255)); // 青色文字
            if (scoreToAdd > 0) { if (m_scoreDoubleActive) scoreToAdd *= 2; addScore(scoreToAdd); }
            if (timeToAdd > 0) { m_totalTime += timeToAdd; ui->labelCountdown->setText(QString("%1:%2").arg(m_totalTime/60, 2, 10, QChar('0')).arg(m_totalTime%60, 2, 10, QChar('0'))); }

            skill->used = true;
            if (wasRunning && !m_isPaused) m_gameTimer->start();
        });
    }

    contentLayout->addStretch();

    // 底部取消按钮
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
        "   background-color: rgba(0, 229, 255, 40);" // 悬停变青色微光
        "   border: 1px solid #00e5ff;"
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

// 【新增】显示临时消息函数（Mode_2专用，青色主题）
void Mode_2::showTempMessage(const QString& message, const QColor& color)
{
    if (!ui->boardWidget) return;

    QLabel* lbl = new QLabel(ui->boardWidget);
    lbl->setText(message);
    lbl->setAlignment(Qt::AlignCenter);

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

    QSequentialAnimationGroup* seq = new QSequentialAnimationGroup(this);

    QPropertyAnimation* fadeIn = new QPropertyAnimation(eff, "opacity");
    fadeIn->setDuration(300);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutQuad);
    seq->addAnimation(fadeIn);

    seq->addPause(1500);

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

void Mode_2::playSpecialEffect(EffectType type, QPoint center, int colorCode)
{
    if (type == None || type == Normal) return;

    // 重新计算坐标偏移 (确保特效位置准确)
    int cellSize = 48; int gap = 2;
    QRect cr = ui->boardWidget->contentsRect();
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;

    int centerX = ox + center.y() * (cellSize + gap) + cellSize / 2;
    int centerY = oy + center.x() * (cellSize + gap) + cellSize / 2;

    // 1. 行/列 激光炮 (改为青蓝色系)
    if (type == RowBomb || type == ColBomb) {
        QLabel *beam = new QLabel(ui->boardWidget);
        beam->setAttribute(Qt::WA_TransparentForMouseEvents);
        beam->show();

        // 样式：中间青白，两边透明
        QString qss = "background: qlineargradient(x1:0, y1:0, x2:%1, y2:%2, "
                      "stop:0 rgba(0,229,255,0), stop:0.5 rgba(200,255,255,230), stop:1 rgba(0,229,255,0));";

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

        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(beam);
        beam->setGraphicsEffect(eff);
        QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
        fade->setDuration(400);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->start(QAbstractAnimation::DeleteWhenStopped);

        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QAbstractAnimation::finished, beam, &QLabel::deleteLater);
    }

    // 2. 区域炸弹 (冲击波 - 改为风暴青色)
    else if (type == AreaBomb) {
        QLabel *shockwave = new QLabel(ui->boardWidget);
        shockwave->setAttribute(Qt::WA_TransparentForMouseEvents);
        // 旋风风格：青色径向渐变
        shockwave->setStyleSheet(
            "background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, "
            "stop:0 rgba(0,0,0,0), stop:0.6 rgba(0, 229, 255, 180), stop:1 rgba(0,0,0,0));"
            "border-radius: 100px;");
        shockwave->show();

        QPropertyAnimation *anim = new QPropertyAnimation(shockwave, "geometry");
        anim->setDuration(500);
        anim->setEasingCurve(QEasingCurve::OutQuad);

        QRect startRect(centerX, centerY, 0, 0);
        QRect endRect(centerX - 150, centerY - 150, 300, 300);

        anim->setStartValue(startRect);
        anim->setEndValue(endRect);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QAbstractAnimation::finished, shockwave, &QLabel::deleteLater);
    }

    // 3. 全屏闪光 (改为青色闪光)
    else if (type == ColorClear) {
        QLabel *flash = new QLabel(ui->boardWidget);
        flash->setAttribute(Qt::WA_TransparentForMouseEvents);
        flash->setStyleSheet("background-color: rgba(0, 229, 255, 120);"); // 青色半透明
        flash->setGeometry(ui->boardWidget->rect());
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
/* 请替换 mode_2.cpp 中的 onBackButtonClicked */
void Mode_2::onBackButtonClicked()
{
    bool wasRunning = m_gameTimer->isActive();
    m_gameTimer->stop();

    // 【新增】停止技能效果计时器
    if (m_skillEffectTimer && m_skillEffectTimer->isActive()) {
        m_skillEffectTimer->stop();
    }

    QDialog dlg(this);
    dlg.setWindowTitle("确认返回");
    dlg.setFixedSize(360, 220);
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // 【样式】Mode_1 样式复刻，改为青色系
    dlg.setStyleSheet(
        "QDialog {"
        "   background-color: #162640;"
        "   border: none;"
        "   border-radius: 10px;"
        "}"
        "QLabel {"
        "   color: #ffffff;"
        "   font: 14pt 'Microsoft YaHei';"
        "   background: transparent;"
        "   border: none;"
        "}"
        "QPushButton {"
        "   border-radius: 15px;"
        "   color: black;" /* 青色背景配黑字更易读，或者 white 也可以 */
        "   font: bold 12pt 'Microsoft YaHei';"
        "   height: 30px;"
        "   width: 80px;"
        "}"
        );

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    QLabel *lblMsg = new QLabel("若返回，当前对局信息丢失。\n是否确定返回？", &dlg);
    lblMsg->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(lblMsg);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(40);
    btnLayout->addStretch();

    // 确定按钮 (红色系保留，表示危险)
    QPushButton *btnYes = new QPushButton("确定", &dlg);
    btnYes->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff5555, stop:1 #ff7777); color: white; }"
        "QPushButton:hover { background: #ff4444; }"
        );
    connect(btnYes, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(btnYes);

    // 取消按钮 (改为青色系，表示安全/留在本模式)
    QPushButton *btnNo = new QPushButton("取消", &dlg);
    btnNo->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00e5ff, stop:1 #18ffff); }"
        "QPushButton:hover { background: #00b8d4; }"
        );
    connect(btnNo, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnLayout->addWidget(btnNo);

    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    int ret = dlg.exec();

    if (ret == QDialog::Accepted) {
        NetworkManager *networkManager = NetworkManager::instance();
        if (networkManager && networkManager->isConnected()) {
            networkManager->updateUserStatus("在线", "空闲");
        }
        emit gameFinished();
    } else {
        if (wasRunning) m_gameTimer->start();
    }
}

/* mode_2.cpp - 修复后的 gameOver 函数 */
void Mode_2::gameOver()
{
    m_isLocked = true;

    // 【新增】停止技能效果计时器
    if (m_skillEffectTimer && m_skillEffectTimer->isActive()) {
        m_skillEffectTimer->stop();
    }

    // 1. 数据库写入 - 修复：保存到 game_records 表
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            QSqlQuery q(db);

            // A. 插入游戏记录到 game_records 表
            QString insertSql = "INSERT INTO game_records (username, mode, score, time) VALUES (?, ?, ?, NOW())";
            q.prepare(insertSql);
            q.addBindValue(m_username);
            q.addBindValue("旋风模式");  // 完整的模式名称
            q.addBindValue(m_score);

            if (!q.exec()) {
                qDebug() << "DB Error (Insert to game_records):" << q.lastError().text();
                qDebug() << "执行的SQL:" << insertSql;
                qDebug() << "参数: username=" << m_username << ", mode=旋风模式, score=" << m_score;
            } else {
                qDebug() << "游戏记录保存成功: 用户=" << m_username << ", 分数=" << m_score << ", 模式=旋风模式";
            }

            // B. 累加总分到 user 表
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

    // 2. 弹窗 UI (青色风格)
    QDialog dlg(this);
    dlg.setWindowTitle("结算");
    dlg.setFixedSize(320, 240);
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    dlg.setStyleSheet(
        "QDialog {"
        "   background-color: #162640;"
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
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00e5ff, stop:1 #18ffff);"
        "   border-radius: 15px;"
        "   color: black;"
        "   font: bold 14pt 'Microsoft YaHei';"
        "   height: 36px;"
        "}"
        "QPushButton:hover { background: #00b8d4; }"
        "QPushButton:pressed { background: #00838f; }"
        );

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(20);
    layout->setContentsMargins(30, 30, 30, 30);

    // 标题 (青色)
    QLabel *lblTitle = new QLabel("GAME OVER", &dlg);
    lblTitle->setAlignment(Qt::AlignCenter);
    lblTitle->setStyleSheet("font-size: 24pt; font-weight: bold; color: #00e5ff;");
    layout->addWidget(lblTitle);

    QLabel *lblScore = new QLabel(QString("本局得分: %1").arg(m_score), &dlg);
    lblScore->setAlignment(Qt::AlignCenter);
    lblScore->setStyleSheet("font-size: 18pt;");
    layout->addWidget(lblScore);

    QPushButton *btnOk = new QPushButton("返回菜单", &dlg);
    connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(btnOk);

    dlg.exec();
    emit gameFinished(true);
}

void Mode_2::showHint(int r, int c)
{
    m_selR = r; m_selC = c;

    // 计算位置 (保持不变)
    int cellSize = 48; int gap = 2;
    QRect cr = ui->boardWidget->contentsRect();
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top()  + (cr.height() - totalH) / 2;
    int targetX = ox + c * (cellSize + gap) - 2;
    int targetY = oy + r * (cellSize + gap) - 2;
    int frameSize = (cellSize * 2) + gap + 4;

    m_selectorFrame->resize(frameSize, frameSize);
    m_selectorFrame->move(targetX, targetY);

    // 【修改 1】设置样式为青色实线边框
    m_selectorFrame->setStyleSheet("border: 4px solid #00e5ff; border-radius: 15px; background: transparent;");
    m_selectorFrame->show();
    m_selectorFrame->raise(); // 双重保险

    // 动画组
    if (m_hintAnimGroup) m_hintAnimGroup->deleteLater();
    m_hintAnimGroup = new QSequentialAnimationGroup(this);
    m_hintAnimGroup->setLoopCount(-1); // 无限循环

    // 【修改 2】使用 Opacity 动画实现闪烁 (亮 -> 暗 -> 亮)
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(m_selectorFrame);
    m_selectorFrame->setGraphicsEffect(eff);

    QPropertyAnimation *anim = new QPropertyAnimation(eff, "opacity");
    anim->setDuration(800);
    anim->setStartValue(1.0);       // 全亮
    anim->setKeyValueAt(0.5, 0.2);  // 半隐
    anim->setEndValue(1.0);         // 全亮
    anim->setEasingCurve(QEasingCurve::InOutSine);

    m_hintAnimGroup->addAnimation(anim);
    m_hintAnimGroup->start();
}

void Mode_2::stopHint()
{
    if (m_hintAnimGroup) {
        m_hintAnimGroup->stop();
        m_hintAnimGroup->deleteLater();
        m_hintAnimGroup = nullptr;
    }
    // 恢复默认样式 (隐藏光晕/透明度特效)
    m_selectorFrame->setGraphicsEffect(nullptr);
    // 恢复默认边框样式 (非闪烁时的样式，或者直接隐藏)
    m_selectorFrame->setStyleSheet("border: 3px solid #00e5ff; border-radius: 15px; background: transparent;");

    // 如果不是在提示状态下，通常光圈是隐藏的，或者跟随鼠标。
    // 这里我们不需要 hide，因为 eventFilter 会控制它的显示位置。
    // 只要去掉了 OpacityEffect，它就会恢复常亮显示。
}

// mode_2.cpp - 添加这个函数
void Mode_2::resetSkills()
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
void Mode_2::onSkillEffectTimeout()
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
void Mode_2::showSkillEndHint(const QString& message)
{
    if (!ui->boardWidget) return;

    // 创建提示标签
    QLabel* hintLabel = new QLabel(ui->boardWidget);
    hintLabel->setText(message);
    hintLabel->setAlignment(Qt::AlignCenter);

    // 样式：半透明，不遮挡游戏内容
    hintLabel->setStyleSheet(
        "background-color: rgba(0, 0, 0, 180);"
        "color: #00e5ff;"
        "font: bold 16pt 'Microsoft YaHei';"
        "border-radius: 8px;"
        "border: 1px solid #00e5ff;"
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
