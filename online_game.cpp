#include "online_game.h"
#include "ui_online_game.h"
#include "gameboard.h"
#include "skilltree.h"
#include "networkmanager.h"
#include "musicmanager.h"  // 【新增】音乐管理头文件

#include <QGridLayout>
#include <QPushButton>
#include <QDir>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QRandomGenerator>
#include <QLabel>
#include <QDebug>
#include <QDateTime>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QApplication>
#include <QScreen>

OnlineGame::OnlineGame(const QString &myUsername, const QString &opponentUsername,
                       QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OnlineGame)
    , m_myUsername(myUsername)
    , m_opponentUsername(opponentUsername)
    , m_myScore(0)
    , m_opponentScore(0)
    , m_totalTime(180)
    , m_isGameActive(false)
    , m_isGameStarted(false)
    , m_myLocked(false)
    , m_myPaused(false)
    , m_myClickCount(0)
    , m_mySelR(-1)
    , m_mySelC(-1)
    , m_myScoreDoubleActive(false)
    , m_myColorUnifyActive(false)
    , m_myUltimateBurstActive(false)
    , m_mySkillTree(nullptr)
    // 【新增】同步状态初始化
    , m_lastSyncedScore(-1)  // 初始化为-1，确保第一次同步会发送
    , m_hasInitialSync(false)
    , m_lastSyncTime(0)
    , m_gameEnded(false)  // 新增：初始化游戏结束标志
    , m_opponentLocked(false)  // 【新增】初始化对手棋盘锁定
{
    ui->setupUi(this);

    // 设置窗口居中显示
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - 1200) / 2;
    int y = (screenGeometry.height() - 800) / 2;
    move(x, y);

    // 初始化棋盘
    m_myBoard = new GameBoard(this);
    m_opponentBoard = new GameBoard(this);

    // 初始化布局
    m_myGridLayout = new QGridLayout(ui->myBoardContainer);
    m_myGridLayout->setSpacing(2);
    m_myGridLayout->setContentsMargins(4, 4, 4, 4);

    m_opponentGridLayout = new QGridLayout(ui->opponentBoardContainer);
    m_opponentGridLayout->setSpacing(2);
    m_opponentGridLayout->setContentsMargins(4, 4, 4, 4);

    // 初始化动画组
    m_myDropGroup = new QSequentialAnimationGroup(this);
    m_opponentDropGroup = new QSequentialAnimationGroup(this);

    // 初始化技能计时器
    m_mySkillEffectTimer = new QTimer(this);
    m_mySkillEffectTimer->setSingleShot(true);
    connect(m_mySkillEffectTimer, &QTimer::timeout, this, &OnlineGame::onMySkillEffectTimeout);

    // 初始化游戏计时器
    m_gameTimer = new QTimer(this);
    m_gameTimer->setInterval(1000);
    connect(m_gameTimer, &QTimer::timeout, this, &OnlineGame::onGameTimerTick);

    // 初始化同步计时器
    m_syncTimer = new QTimer(this);
    m_syncTimer->setInterval(1000);
    connect(m_syncTimer, &QTimer::timeout, this, &OnlineGame::syncMyBoard);

    // 连接按钮信号
    connect(ui->btnBack, &QPushButton::clicked, this, &OnlineGame::on_btnBack_clicked);
    connect(ui->btnMyUndo, &QPushButton::clicked, this, &OnlineGame::on_btnMyUndo_clicked);
    connect(ui->btnMySkill, &QPushButton::clicked, this, &OnlineGame::on_btnMySkill_clicked);

    // 连接网络消息
    NetworkManager *networkManager = NetworkManager::instance();
    if (networkManager) {
        connect(networkManager, &NetworkManager::gameMoveReceived,
                this, &OnlineGame::onGameMoveReceived);
        connect(networkManager, &NetworkManager::gameStartReceived,
                this, &OnlineGame::onGameStartReceived);
        connect(networkManager, &NetworkManager::gameEndReceived,
                this, &OnlineGame::onGameEndReceived);
        connect(networkManager, &NetworkManager::playerQuitReceived,  // 新增
                this, &OnlineGame::onPlayerQuitReceived);
    }

    // 初始化UI
    updateMyInfo();
    updateOpponentInfo();
    updateCountdown();

    // 初始化棋盘
    initMyBoard();
    initOpponentBoard();

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            m_lastSyncedGrid[r][c].pic = -1; // 初始化为无效值
        }
    }

    // 更新网络状态
    if (networkManager && networkManager->isConnected()) {
        networkManager->updateUserStatus("联机游戏中", "闪电", opponentUsername);
    }

    // 设置窗口标志
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
}

OnlineGame::~OnlineGame()
{
    delete ui;
}

void OnlineGame::setMySkillTree(SkillTree* skillTree)
{
    m_mySkillTree = skillTree;
    resetMySkills();
}

// =============== 我的棋盘初始化 ===============
void OnlineGame::initMyBoard()
{
    m_myBoard->initNoThree();
    rebuildMyGrid();
}

void OnlineGame::rebuildMyGrid()
{
    clearMyGridLayout();
    m_myCells.resize(ROW * COL);
    const Grid &gr = m_myBoard->grid();

    // 【关键修复】使用与 mode_1 完全相同的棋盘布局参数
    QRect cr = ui->myBoardContainer->contentsRect();
    int availW = cr.width();
    int availH = cr.height();

    // 与 mode_1 完全相同的参数
    int cellSize = 48;
    int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;  // 8*50 - 2 = 400 - 2 = 398
    int totalH = ROW * (cellSize + gap) - gap;  // 8*50 - 2 = 400 - 2 = 398

    int ox = cr.left() + (availW - totalW) / 2;
    int oy = cr.top() + (availH - totalH) / 2;

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = new QPushButton(ui->myBoardContainer);
            btn->setFixedSize(cellSize, cellSize);
            btn->setStyleSheet("border:none;");

            // 修复图片路径
            QString path = getCellImagePath(gr[r][c].pic);
            btn->setIcon(QIcon(path));
            btn->setIconSize(QSize(cellSize, cellSize));

            int targetX = ox + c * (cellSize + gap);
            int targetY = oy + r * (cellSize + gap);

            // 初始位置：从顶部上方
            btn->move(targetX, targetY - (ROW - r) * (cellSize + gap));

            // 设置透明度效果
            auto *eff = new QGraphicsOpacityEffect(btn);
            eff->setOpacity(0.0);
            btn->setGraphicsEffect(eff);
            btn->show();

            m_myCells[r * COL + c] = btn;

            // 连接点击信号
            connect(btn, &QPushButton::clicked, this, [this, r, c]() {
                if (!m_isGameActive || m_myLocked || m_myPaused) return;
                handleMyCellClick(r, c);
            });
        }
    }

    // 创建下落动画
    createMyDropAnimation(ox, oy);
}

void OnlineGame::clearMyGridLayout()
{
    // 停止所有动画
    if (m_mySkillEffectTimer && m_mySkillEffectTimer->isActive()) {
        m_mySkillEffectTimer->stop();
    }

    if (m_myDropGroup) {
        m_myDropGroup->stop();
        m_myDropGroup->clear();
    }

    // 清理按钮
    for (QPushButton *b : m_myCells) {
        if (b) {
            delete b;
        }
    }
    m_myCells.clear();

    // 清理布局
    if (m_myGridLayout && ui->myBoardContainer) {
        QLayoutItem *item;
        while ((item = m_myGridLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
    }
}

void OnlineGame::createMyDropAnimation(int left0, int top0)
{
    if (m_myDropGroup->state() == QAbstractAnimation::Running) {
        m_myDropGroup->stop();
    }
    m_myDropGroup->clear();

    int cellSize = 48;
    int gap = 2;

    for (int r = ROW - 1; r >= 0; --r) {
        auto *rowPar = new QParallelAnimationGroup(this);
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = m_myCells[r * COL + c];
            if (!btn) continue;

            auto *eff = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
            if (!eff) {
                eff = new QGraphicsOpacityEffect(btn);
                btn->setGraphicsEffect(eff);
            }

            int targetX = left0 + c * (cellSize + gap);
            int targetY = top0 + r * (cellSize + gap);

            // 下落动画
            auto *drop = new QPropertyAnimation(btn, "pos");
            drop->setDuration(180 + (ROW - 1 - r) * 15);
            drop->setStartValue(QPoint(targetX, targetY - 200));
            drop->setEndValue(QPoint(targetX, targetY));
            drop->setEasingCurve(QEasingCurve::OutBounce);

            // 淡入动画
            auto *fade = new QPropertyAnimation(eff, "opacity");
            fade->setDuration(300);
            fade->setStartValue(0.0);
            fade->setEndValue(1.0);

            auto *cellAnim = new QParallelAnimationGroup(this);
            cellAnim->addAnimation(drop);
            cellAnim->addAnimation(fade);
            rowPar->addAnimation(cellAnim);
        }
        m_myDropGroup->addAnimation(rowPar);
        m_myDropGroup->addPause(1);
    }

    connect(m_myDropGroup, &QSequentialAnimationGroup::finished, this, [this]() {
        // 动画完成后将按钮添加到网格布局
        for (int r = 0; r < ROW; ++r) {
            for (int c = 0; c < COL; ++c) {
                QPushButton *btn = m_myCells[r * COL + c];
                if (btn) m_myGridLayout->addWidget(btn, r, c);
            }
        }
        m_myLocked = false;

        // 如果是第一次启动，播放开始动画
        if (!m_isGameStarted) {
            m_isGameStarted = true;
            startGameSequence();
        }
    });

    m_myDropGroup->start();
}

// =============== 棋盘交互逻辑 ===============
void OnlineGame::handleMyCellClick(int r, int c)
{
    if (m_myLocked || m_myPaused) return;

    QPushButton *btn = m_myCells[r * COL + c];

    if (m_myClickCount == 0) {
        // 第一次点击：选择
        m_mySelR = r;
        m_mySelC = c;
        setMySelected(btn, true);
        m_myClickCount = 1;
        return;
    }

    // 第二次点击：尝试交换
    QPushButton *firstBtn = m_myCells[m_mySelR * COL + m_mySelC];

    // 检查是否相邻
    if (qAbs(m_mySelR - r) + qAbs(m_mySelC - c) != 1) {
        // 不相邻，重新选择
        setMySelected(firstBtn, false);
        m_mySelR = r;
        m_mySelC = c;
        setMySelected(btn, true);
        return;
    }

    // 检查是否可以交换
    bool ok = m_myBoard->trySwap(m_mySelR, m_mySelC, r, c);
    setMySelected(firstBtn, false);
    m_myClickCount = 0;

    if (!ok) {
        // 不可交换，抖动效果
        playMyShake(firstBtn);
        playMyShake(btn);
    } else {
        // 保存状态
        saveMyState();

        // 在逻辑上交换
        std::swap(m_myBoard->m_grid[m_mySelR][m_mySelC].pic,
                  m_myBoard->m_grid[r][c].pic);

        // 处理交互
        processMyInteraction(m_mySelR, m_mySelC, r, c);
    }
}

void OnlineGame::setMySelected(QPushButton *btn, bool on)
{
    if (on) {
        auto *shadow = new QGraphicsDropShadowEffect(btn);
        shadow->setColor(QColor("#00e5ff"));  // 蓝色光晕
        shadow->setBlurRadius(12);
        shadow->setOffset(0, 0);
        btn->setGraphicsEffect(shadow);
    } else {
        btn->setGraphicsEffect(nullptr);
    }
}

void OnlineGame::playMyShake(QPushButton *btn)
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

void OnlineGame::processMyInteraction(int r1, int c1, int r2, int c2)
{
    m_myLocked = true;

    // 获取消除结果
    ElimResult res1 = getMyEliminations(r1, c1);
    ElimResult res2 = getMyEliminations(r2, c2);

    QSet<QPoint> allToRemove = res1.points;
    allToRemove.unite(res2.points);

    // 如果没有消除，回滚
    if (allToRemove.isEmpty()) {
        std::swap(m_myBoard->m_grid[r1][c1].pic, m_myBoard->m_grid[r2][c2].pic);
        playMyShake(m_myCells[r1 * COL + c1]);
        playMyShake(m_myCells[r2 * COL + c2]);
        m_myLocked = false;
        return;
    }

    // 视觉交换动画
    int idx1 = r1 * COL + c1;
    int idx2 = r2 * COL + c2;
    QPushButton *btn1 = m_myCells[idx1];
    QPushButton *btn2 = m_myCells[idx2];

    // 交换单元格指针
    std::swap(m_myCells[idx1], m_myCells[idx2]);

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

    connect(swapGroup, &QAbstractAnimation::finished, this, [this, swapGroup, allToRemove, res1, res2]() {
        swapGroup->deleteLater();

        // 播放特效
        if (res1.type != Normal && res1.type != None) {
            playMySpecialEffect(res1.type, res1.center, 0);
        }
        if (res2.type != Normal && res2.type != None) {
            playMySpecialEffect(res2.type, res2.center, 0);
        }

        // 执行消除动画
        playMyEliminateAnim(allToRemove);
    });

    swapGroup->start();
}

// =============== 消除检测逻辑 ===============
int OnlineGame::countMyDirection(int r, int c, int dR, int dC)
{
    int color = m_myBoard->m_grid[r][c].pic;
    int count = 0;
    int nr = r + dR;
    int nc = c + dC;

    while (nr >= 0 && nr < ROW && nc >= 0 && nc < COL &&
           m_myBoard->m_grid[nr][nc].pic == color) {
        count++;
        nr += dR;
        nc += dC;
    }

    return count;
}

OnlineGame::ElimResult OnlineGame::getMyEliminations(int r, int c)
{
    ElimResult res;
    res.center = QPoint(r, c);

    int color = m_myBoard->m_grid[r][c].pic;
    if (color == -1) return res;

    int up = countMyDirection(r, c, -1, 0);
    int down = countMyDirection(r, c, 1, 0);
    int left = countMyDirection(r, c, 0, -1);
    int right = countMyDirection(r, c, 0, 1);

    // 1. 全屏消除
    if ((up + down >= 4) || (left + right >= 4)) {
        res.type = ColorClear;
        for (int i = 0; i < ROW; ++i) {
            for (int j = 0; j < COL; ++j) {
                if (m_myBoard->m_grid[i][j].pic == color) {
                    res.points.insert(QPoint(i, j));
                }
            }
        }
        return res;
    }

    // 2. 列消除
    if (up + down == 3) {
        res.type = ColBomb;
        for (int i = 0; i < ROW; ++i) {
            res.points.insert(QPoint(i, c));
        }
        return res;
    }

    // 3. 行消除
    if (left + right == 3) {
        res.type = RowBomb;
        for (int j = 0; j < COL; ++j) {
            res.points.insert(QPoint(r, j));
        }
        return res;
    }

    // 4. 区域炸弹
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

    // 5. 普通消除
    if (up + down >= 2) {
        for (int i = r - up; i <= r + down; ++i) {
            res.points.insert(QPoint(i, c));
        }
        res.type = Normal;
    }

    if (left + right >= 2) {
        for (int j = c - left; j <= c + right; ++j) {
            res.points.insert(QPoint(r, j));
        }
        res.type = Normal;
    }

    return res;
}

void OnlineGame::playMySpecialEffect(EffectType type, QPoint center, int colorCode)
{
    if (type == None || type == Normal) return;

    QWidget *boardWidget = ui->myBoardContainer;
    int cellSize = 48;
    int gap = 2;

    // 计算棋盘位置
    QRect cr = boardWidget->contentsRect();
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top() + (cr.height() - totalH) / 2;

    int centerX = ox + center.y() * (cellSize + gap) + cellSize / 2;
    int centerY = oy + center.x() * (cellSize + gap) + cellSize / 2;

    // 行/列激光
    if (type == RowBomb || type == ColBomb) {
        QLabel *beam = new QLabel(boardWidget);
        beam->setAttribute(Qt::WA_TransparentForMouseEvents);
        beam->show();

        QString qss = "background: qlineargradient(x1:0, y1:0, x2:%1, y2:%2, "
                      "stop:0 rgba(255,255,255,0), stop:0.5 rgba(0, 229, 255, 230), stop:1 rgba(255,255,255,0));";

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

        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QAbstractAnimation::finished, beam, &QLabel::deleteLater);
    }
    // 区域炸弹
    else if (type == AreaBomb) {
        QLabel *shockwave = new QLabel(boardWidget);
        shockwave->setAttribute(Qt::WA_TransparentForMouseEvents);
        shockwave->setStyleSheet(
            "background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, "
            "stop:0 rgba(255,255,255,0), stop:0.7 rgba(0, 229, 255, 200), stop:1 rgba(255,255,255,0));"
            "border-radius: 100px;");
        shockwave->show();

        QPropertyAnimation *anim = new QPropertyAnimation(shockwave, "geometry");
        anim->setDuration(500);
        anim->setEasingCurve(QEasingCurve::OutQuad);

        anim->setStartValue(QRect(centerX, centerY, 0, 0));
        anim->setEndValue(QRect(centerX - 75, centerY - 75, 150, 150));

        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QAbstractAnimation::finished, shockwave, &QLabel::deleteLater);
    }
    // 全屏闪光
    else if (type == ColorClear) {
        QLabel *flash = new QLabel(boardWidget);
        flash->setAttribute(Qt::WA_TransparentForMouseEvents);
        flash->setStyleSheet("background-color: rgba(0, 229, 255, 180);");
        flash->setGeometry(boardWidget->rect());
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

void OnlineGame::playMyEliminateAnim(const QSet<QPoint>& points)
{
    // 【新增】播放消除音效
    int elimCount = points.size();
    if (elimCount >= 3) {
        MusicManager::instance().playMatchSound(elimCount);
    }

    // 计分
    if (!points.isEmpty()) {
        addMyScore(points.size());
    }

    QParallelAnimationGroup *elimGroup = new QParallelAnimationGroup(this);

    for (const QPoint &p : points) {
        int idx = p.x() * COL + p.y();
        QPushButton *btn = m_myCells[idx];
        if (!btn) continue;

        // 缩小动画
        QPropertyAnimation *scale = new QPropertyAnimation(btn, "geometry");
        scale->setDuration(250);
        QRect rect = btn->geometry();
        scale->setStartValue(rect);
        scale->setEndValue(rect.adjusted(24, 24, -24, -24));

        // 淡出动画
        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(btn);
        btn->setGraphicsEffect(eff);
        QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
        fade->setDuration(250);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);

        elimGroup->addAnimation(scale);
        elimGroup->addAnimation(fade);

        // 标记为已消除
        m_myBoard->m_grid[p.x()][p.y()].pic = -1;
    }

    connect(elimGroup, &QAbstractAnimation::finished, this, [this, elimGroup, points]() {
        // 删除按钮
        for (const QPoint &p : points) {
            int idx = p.x() * COL + p.y();
            if (m_myCells[idx]) {
                delete m_myCells[idx];
                m_myCells[idx] = nullptr;
            }
        }

        elimGroup->deleteLater();
        m_myUltimateBurstActive = false;

        // 执行下落
        performMyFallAnimation();
    });

    elimGroup->start();
}

void OnlineGame::performMyFallAnimation()
{
    QRect cr = ui->myBoardContainer->contentsRect();
    int cellSize = 48;
    int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top() + (cr.height() - totalH) / 2;

    QParallelAnimationGroup *fallGroup = new QParallelAnimationGroup(this);
    auto *rng = QRandomGenerator::global();

    // 先将所有按钮从布局中移除
    for (QPushButton *b : m_myCells) {
        if (b) {
            m_myGridLayout->removeWidget(b);
        }
    }

    // 对每一列进行处理
    for (int c = 0; c < COL; ++c) {
        // 收集幸存按钮
        struct BlockData {
            QPushButton* btn;
            int color;
        };
        QList<BlockData> survivors;

        for (int r = ROW - 1; r >= 0; --r) {
            int idx = r * COL + c;
            if (m_myCells[idx] != nullptr) {
                survivors.append({m_myCells[idx], m_myBoard->m_grid[r][c].pic});
                m_myCells[idx] = nullptr;
            }
        }

        // 重建这一列
        int survivorIdx = 0;
        for (int r = ROW - 1; r >= 0; --r) {
            QPushButton *btn = nullptr;
            int finalColor = 0;
            bool isExistingBtn = false;

            int destX = ox + c * (cellSize + gap);
            int destY = oy + r * (cellSize + gap);

            if (survivorIdx < survivors.size()) {
                // 使用幸存按钮
                BlockData bd = survivors[survivorIdx];
                btn = bd.btn;
                finalColor = bd.color;
                isExistingBtn = true;
                survivorIdx++;
            } else {
                // 创建新按钮
                finalColor = rng->bounded(6);
                isExistingBtn = false;

                btn = new QPushButton(ui->myBoardContainer);
                btn->setFixedSize(cellSize, cellSize);
                btn->setStyleSheet("border:none;");
                QString path = getCellImagePath(finalColor);
                btn->setIcon(QIcon(path));
                btn->setIconSize(QSize(cellSize, cellSize));
                btn->show();

                // 新按钮从顶部掉落
                int startY = destY - (ROW * cellSize + 100);
                btn->move(destX, startY);
            }

            // 更新状态
            m_myCells[r * COL + c] = btn;
            m_myBoard->m_grid[r][c].pic = finalColor;

            // 重新连接信号
            if (isExistingBtn) btn->disconnect();
            connect(btn, &QPushButton::clicked, this, [this, r, c]() {
                handleMyCellClick(r, c);
            });

            // 创建下落动画
            QPoint startPos = btn->pos();
            QPoint endPos = QPoint(destX, destY);

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

    connect(fallGroup, &QAbstractAnimation::finished, this, [this, fallGroup]() {
        // 重新添加到布局
        for (int r = 0; r < ROW; ++r) {
            for (int c = 0; c < COL; ++c) {
                QPushButton *btn = m_myCells[r * COL + c];
                if (btn) {
                    m_myGridLayout->addWidget(btn, r, c);
                }
            }
        }

        fallGroup->deleteLater();

        // 检查连击
        checkMyComboMatches();
    });

    fallGroup->start();
}

void OnlineGame::checkMyComboMatches()
{
    QSet<QPoint> allMatches;
    QSet<QPoint> processedCenters;

    // 扫描全盘
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            ElimResult res = getMyEliminations(r, c);
            if (!res.points.isEmpty()) {
                allMatches.unite(res.points);

                // 播放特效
                if (res.type != Normal && res.type != None) {
                    if (!processedCenters.contains(res.center)) {
                        playMySpecialEffect(res.type, res.center, 0);
                        processedCenters.insert(res.center);
                    }
                }
            }
        }
    }

    if (!allMatches.isEmpty()) {
        // 有连击，继续消除
        m_myLocked = true;
        playMyEliminateAnim(allMatches);
    } else {
        // 无连击，检查死局
        if (m_myBoard->isDead(m_myBoard->grid())) {
            handleMyDeadlock();
        } else {
            m_myLocked = false;
            // 同步棋盘状态
            syncMyBoard();
        }
    }
}

void OnlineGame::handleMyDeadlock()
{
    if (m_myLocked) return;
    m_myLocked = true;

    QLabel *lbl = new QLabel(ui->myBoardContainer);
    lbl->setText("死局！重新洗牌");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(
        "background-color: rgba(0, 0, 0, 180);"
        "color: #00e5ff;"
        "font: bold 26pt 'Microsoft YaHei';"
        "border-radius: 15px;"
        "border: 2px solid #00e5ff;"
        );
    lbl->adjustSize();
    lbl->resize(lbl->width() + 60, lbl->height() + 40);

    QRect cr = ui->myBoardContainer->rect();
    lbl->move((cr.width() - lbl->width()) / 2, (cr.height() - lbl->height()) / 2);
    lbl->show();
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(lbl);
    lbl->setGraphicsEffect(eff);

    QPropertyAnimation *anim = new QPropertyAnimation(eff, "opacity");
    anim->setDuration(2000);
    anim->setKeyValueAt(0.0, 0.0);
    anim->setKeyValueAt(0.1, 1.0);
    anim->setKeyValueAt(0.8, 1.0);
    anim->setKeyValueAt(1.0, 0.0);

    connect(anim, &QAbstractAnimation::finished, this, [this, lbl]() {
        lbl->deleteLater();
        m_myBoard->initNoThree();
        rebuildMyGrid();
        syncMyBoard();
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void OnlineGame::addMyScore(int count)
{
    if (m_myUltimateBurstActive) return;

    int points = count * 50;
    if (m_myScoreDoubleActive) {
        points *= 2;
    }

    m_myScore += points;
    updateMyInfo();
}

void OnlineGame::saveMyState()
{
    GameStateSnapshot snapshot;
    snapshot.grid = m_myBoard->grid();
    snapshot.score = m_myScore;
    m_myUndoStack.push(snapshot);
}

// =============== 对手棋盘方法 ===============
// =============== 对手棋盘方法 ===============
void OnlineGame::initOpponentBoard()
{
    m_opponentBoard->initNoThree();
    rebuildOpponentGrid();
}

void OnlineGame::rebuildOpponentGrid()
{
    clearOpponentGridLayout();
    m_opponentCells.resize(ROW * COL);
    const Grid &gr = m_opponentBoard->grid();
    QRect cr = ui->opponentBoardContainer->contentsRect();
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
            QPushButton *btn = new QPushButton(ui->opponentBoardContainer);
            btn->setFixedSize(cellSize, cellSize);
            btn->setStyleSheet("border:none;");
            btn->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setStyleSheet(
                "QPushButton {"
                "   border: none;"
                "   background: transparent;"
                "}"
                "QPushButton:disabled {"
                "   background: transparent;"
                "}"
                );

            QString path = getCellImagePath(gr[r][c].pic);
            btn->setIcon(QIcon(path));
            btn->setIconSize(QSize(cellSize, cellSize));

            int targetX = ox + c * (cellSize + gap);
            int targetY = oy + r * (cellSize + gap);

            // 初始位置
            btn->move(targetX, targetY - (ROW - r) * (cellSize + gap));
            auto *eff = new QGraphicsOpacityEffect(btn);
            eff->setOpacity(0.0);
            btn->setGraphicsEffect(eff);
            btn->show();

            m_opponentCells[r * COL + c] = btn;
        }
    }

    createOpponentDropAnimation(ox, oy);
}

void OnlineGame::clearOpponentGridLayout()
{
    if (m_opponentDropGroup) {
        m_opponentDropGroup->stop();
        m_opponentDropGroup->clear();
    }

    for (QPushButton *b : m_opponentCells) {
        if (b) delete b;
    }
    m_opponentCells.clear();

    if (m_opponentGridLayout && ui->opponentBoardContainer) {
        QLayoutItem *item;
        while ((item = m_opponentGridLayout->takeAt(0)) != nullptr) {
            if (item->widget()) delete item->widget();
            delete item;
        }
    }
}

void OnlineGame::createOpponentDropAnimation(int left0, int top0)
{
    if (m_opponentDropGroup->state() == QAbstractAnimation::Running) {
        m_opponentDropGroup->stop();
    }
    m_opponentDropGroup->clear();

    int cellSize = 48;
    int gap = 2;

    for (int r = ROW - 1; r >= 0; --r) {
        auto *rowPar = new QParallelAnimationGroup(this);
        for (int c = 0; c < COL; ++c) {
            QPushButton *btn = m_opponentCells[r * COL + c];
            if (!btn) continue;

            auto *eff = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
            if (!eff) {
                eff = new QGraphicsOpacityEffect(btn);
                btn->setGraphicsEffect(eff);
            }

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
        m_opponentDropGroup->addAnimation(rowPar);
        m_opponentDropGroup->addPause(1);
    }

    connect(m_opponentDropGroup, &QSequentialAnimationGroup::finished, this, [this]() {
        for (int r = 0; r < ROW; ++r) {
            for (int c = 0; c < COL; ++c) {
                QPushButton *btn = m_opponentCells[r * COL + c];
                if (btn) m_opponentGridLayout->addWidget(btn, r, c);
            }
        }
        m_opponentLocked = false;  // 解锁对手棋盘
    });

    m_opponentDropGroup->start();
}

// 【新增】查找棋盘差异
void OnlineGame::findDifferences(const Grid& oldGrid, const Grid& newGrid,
                                 QSet<QPoint>& eliminated, QSet<QPoint>& newCells)
{
    eliminated.clear();
    newCells.clear();

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            int oldColor = oldGrid[r][c].pic;
            int newColor = newGrid[r][c].pic;

            if (oldColor != -1 && newColor == -1) {
                // 方块被消除
                eliminated.insert(QPoint(r, c));
            } else if (oldColor == -1 && newColor != -1) {
                // 新方块出现（通常是下落填充的）
                newCells.insert(QPoint(r, c));
            } else if (oldColor != -1 && newColor != -1 && oldColor != newColor) {
                // 方块颜色变化（技能效果）
                eliminated.insert(QPoint(r, c));
                newCells.insert(QPoint(r, c));
            }
        }
    }
}

// 【新增】对手消除动画
void OnlineGame::playOpponentEliminateAnim(const QSet<QPoint>& points)
{
    if (points.isEmpty()) return;

    QParallelAnimationGroup *elimGroup = new QParallelAnimationGroup(this);

    for (const QPoint &p : points) {
        int idx = p.x() * COL + p.y();
        QPushButton *btn = m_opponentCells[idx];
        if (!btn) continue;

        // 缩小动画
        QPropertyAnimation *scale = new QPropertyAnimation(btn, "geometry");
        scale->setDuration(250);
        QRect rect = btn->geometry();
        scale->setStartValue(rect);
        scale->setEndValue(rect.adjusted(24, 24, -24, -24));

        // 淡出动画
        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(btn);
        btn->setGraphicsEffect(eff);
        QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
        fade->setDuration(250);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);

        elimGroup->addAnimation(scale);
        elimGroup->addAnimation(fade);
    }

    connect(elimGroup, &QAbstractAnimation::finished, this, [this, elimGroup, points]() {
        // 物理删除按钮
        for (const QPoint &p : points) {
            int idx = p.x() * COL + p.y();
            if (m_opponentCells[idx]) {
                delete m_opponentCells[idx];
                m_opponentCells[idx] = nullptr;
            }
        }

        elimGroup->deleteLater();

        // 执行下落动画
        performOpponentFallAnimation();
    });

    elimGroup->start();
}

// 【新增】对手下落动画
void OnlineGame::performOpponentFallAnimation()
{
    QRect cr = ui->opponentBoardContainer->contentsRect();
    int cellSize = 48;
    int gap = 2;
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top() + (cr.height() - totalH) / 2;

    QParallelAnimationGroup *fallGroup = new QParallelAnimationGroup(this);

    // 先将所有按钮从布局中移除
    for (QPushButton *b : m_opponentCells) {
        if (b) {
            m_opponentGridLayout->removeWidget(b);
        }
    }

    // 对每一列进行处理
    for (int c = 0; c < COL; ++c) {
        // 收集幸存按钮
        struct BlockData {
            QPushButton* btn;
            int color;
        };
        QList<BlockData> survivors;

        for (int r = ROW - 1; r >= 0; --r) {
            int idx = r * COL + c;
            if (m_opponentCells[idx] != nullptr) {
                survivors.append({m_opponentCells[idx], m_opponentBoard->m_grid[r][c].pic});
                m_opponentCells[idx] = nullptr;
            }
        }

        // 重建这一列
        int survivorIdx = 0;
        for (int r = ROW - 1; r >= 0; --r) {
            QPushButton *btn = nullptr;
            int finalColor = m_opponentBoard->m_grid[r][c].pic;
            bool isExistingBtn = false;

            int destX = ox + c * (cellSize + gap);
            int destY = oy + r * (cellSize + gap);

            if (survivorIdx < survivors.size()) {
                // 使用幸存按钮
                BlockData bd = survivors[survivorIdx];
                btn = bd.btn;
                isExistingBtn = true;
                survivorIdx++;

                // 更新幸存按钮的图标
                QString path = getCellImagePath(finalColor);
                btn->setIcon(QIcon(path));
                btn->setIconSize(QSize(cellSize, cellSize));
            } else if (finalColor != -1) {
                // 创建新按钮（填充空缺）
                btn = new QPushButton(ui->opponentBoardContainer);
                btn->setFixedSize(cellSize, cellSize);
                btn->setStyleSheet("border:none;");
                btn->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                btn->setFocusPolicy(Qt::NoFocus);

                QString path = getCellImagePath(finalColor);
                btn->setIcon(QIcon(path));
                btn->setIconSize(QSize(cellSize, cellSize));

                // 新按钮从顶部掉落
                int startY = destY - (ROW * cellSize + 100);
                btn->move(destX, startY);
                btn->show();

                // 添加淡入效果
                QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(btn);
                eff->setOpacity(0.0);
                btn->setGraphicsEffect(eff);
            }

            // 更新状态
            m_opponentCells[r * COL + c] = btn;

            // 创建下落动画
            if (btn && btn->pos() != QPoint(destX, destY)) {
                QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos");
                anim->setDuration(500);
                anim->setStartValue(btn->pos());
                anim->setEndValue(QPoint(destX, destY));
                anim->setEasingCurve(QEasingCurve::OutBounce);
                fallGroup->addAnimation(anim);

                // 如果是新按钮，添加淡入动画
                if (!isExistingBtn) {
                    QGraphicsOpacityEffect *eff = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
                    if (eff) {
                        QPropertyAnimation *fadeIn = new QPropertyAnimation(eff, "opacity");
                        fadeIn->setDuration(500);
                        fadeIn->setStartValue(0.0);
                        fadeIn->setEndValue(1.0);
                        fallGroup->addAnimation(fadeIn);
                    }
                }
            }
        }
    }

    connect(fallGroup, &QAbstractAnimation::finished, this, [this, fallGroup]() {
        // 重新添加到布局
        for (int r = 0; r < ROW; ++r) {
            for (int c = 0; c < COL; ++c) {
                QPushButton *btn = m_opponentCells[r * COL + c];
                if (btn) {
                    m_opponentGridLayout->addWidget(btn, r, c);
                }
            }
        }

        fallGroup->deleteLater();
        m_opponentLocked = false;

        // 播放完成音效
        MusicManager::instance().playMatchSound(1); // 轻微音效提示
    });

    fallGroup->start();
}

// 【新增】对手特效动画
void OnlineGame::playOpponentSpecialEffect(EffectType type, QPoint center, int colorCode)
{
    if (type == None || type == Normal) return;

    QWidget *boardWidget = ui->opponentBoardContainer;
    int cellSize = 48;
    int gap = 2;

    // 计算棋盘位置
    QRect cr = boardWidget->contentsRect();
    int totalW = COL * (cellSize + gap) - gap;
    int totalH = ROW * (cellSize + gap) - gap;
    int ox = cr.left() + (cr.width() - totalW) / 2;
    int oy = cr.top() + (cr.height() - totalH) / 2;

    int centerX = ox + center.y() * (cellSize + gap) + cellSize / 2;
    int centerY = oy + center.x() * (cellSize + gap) + cellSize / 2;

    // 行/列激光
    if (type == RowBomb || type == ColBomb) {
        QLabel *beam = new QLabel(boardWidget);
        beam->setAttribute(Qt::WA_TransparentForMouseEvents);
        beam->show();

        QString qss = "background: qlineargradient(x1:0, y1:0, x2:%1, y2:%2, "
                      "stop:0 rgba(255,255,255,0), stop:0.5 rgba(255, 100, 100, 230), stop:1 rgba(255,255,255,0));";

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

        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QAbstractAnimation::finished, beam, &QLabel::deleteLater);
    }
    // 区域炸弹
    else if (type == AreaBomb) {
        QLabel *shockwave = new QLabel(boardWidget);
        shockwave->setAttribute(Qt::WA_TransparentForMouseEvents);
        shockwave->setStyleSheet(
            "background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, "
            "stop:0 rgba(255,255,255,0), stop:0.7 rgba(255, 100, 100, 200), stop:1 rgba(255,255,255,0));"
            "border-radius: 100px;");
        shockwave->show();

        QPropertyAnimation *anim = new QPropertyAnimation(shockwave, "geometry");
        anim->setDuration(500);
        anim->setEasingCurve(QEasingCurve::OutQuad);

        anim->setStartValue(QRect(centerX, centerY, 0, 0));
        anim->setEndValue(QRect(centerX - 75, centerY - 75, 150, 150));

        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QAbstractAnimation::finished, shockwave, &QLabel::deleteLater);
    }
    // 全屏闪光
    else if (type == ColorClear) {
        QLabel *flash = new QLabel(boardWidget);
        flash->setAttribute(Qt::WA_TransparentForMouseEvents);
        flash->setStyleSheet("background-color: rgba(255, 100, 100, 180);");
        flash->setGeometry(boardWidget->rect());
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

// 【新增】对手抖动效果
void OnlineGame::playOpponentCellShake(QPushButton *btn)
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

// 【新增】处理对手棋盘更新（完整动画版）
void OnlineGame::updateOpponentFromNetwork(const QJsonArray &boardArray, int score)
{
    if (boardArray.size() != ROW * COL) {
        qDebug() << "错误: 棋盘数组大小不正确" << boardArray.size();
        return;
    }

    // 保存旧棋盘状态
    Grid oldGrid = m_opponentBoard->grid();

    // 更新棋盘数据
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            int idx = r * COL + c;
            int color = boardArray[idx].toInt();
            m_opponentBoard->m_grid[r][c].pic = color;
        }
    }

    // 更新分数
    m_opponentScore = score;
    updateOpponentInfo();

    // 锁定对手棋盘，防止动画冲突
    m_opponentLocked = true;

    // 分析棋盘变化
    QSet<QPoint> eliminated;
    QSet<QPoint> newCells;
    findDifferences(oldGrid, m_opponentBoard->grid(), eliminated, newCells);

    // 如果没有变化，直接解锁
    if (eliminated.isEmpty() && newCells.isEmpty()) {
        m_opponentLocked = false;
        return;
    }

    // 播放消除动画
    if (!eliminated.isEmpty()) {
        playOpponentEliminateAnim(eliminated);
    } else {
        // 如果没有消除，直接执行下落动画
        performOpponentFallAnimation();
    }

    qDebug() << "对手棋盘更新: 消除" << eliminated.size()
             << "个, 新生成" << newCells.size() << "个, 分数:" << m_opponentScore;
}
// =============== 游戏控制 ===============
void OnlineGame::startGameSequence()
{
    m_myLocked = true;
    resetMySkills();

    // 【新增】播放联机游戏背景音乐
    MusicManager::instance().playSceneMusic(MusicManager::MusicScene::Playing);

    QLabel *lbl = new QLabel(ui->myBoardContainer);
    lbl->setText("Ready Go!");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(
        "color: #00e5ff;"
        "font: bold 40pt 'Microsoft YaHei';"
        "background: transparent;"
        );
    lbl->adjustSize();
    QRect cr = ui->myBoardContainer->rect();
    lbl->move((cr.width() - lbl->width()) / 2, (cr.height() - lbl->height()) / 2);
    lbl->show();
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(lbl);
    lbl->setGraphicsEffect(eff);

    QSequentialAnimationGroup *seq = new QSequentialAnimationGroup(this);
    seq->addPause(500);

    QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
    fade->setDuration(1000);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    seq->addAnimation(fade);

    connect(seq, &QAbstractAnimation::finished, this, [this, lbl]() {
        lbl->deleteLater();
        m_myLocked = false;
        m_isGameActive = true;
        m_gameTimer->start();
        m_syncTimer->start();
    });

    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void OnlineGame::onGameTimerTick()
{
    // 如果游戏已经不活动，直接返回
    if (!m_isGameActive) {
        if (m_gameTimer && m_gameTimer->isActive()) {
            m_gameTimer->stop();
        }
        return;
    }

    if (m_totalTime > 0) {
        m_totalTime--;
        updateCountdown();
    } else {
        // 停止计时器再调用gameOver
        if (m_gameTimer && m_gameTimer->isActive()) {
            m_gameTimer->stop();
        }
        gameOver();
    }
}

void OnlineGame::gameOver()
{
    // 检查是否已经结束
    if (m_gameEnded) {
        qDebug() << "游戏已经结束，忽略重复的gameOver调用";
        return;
    }

    m_gameEnded = true;
    m_isGameActive = false;
    m_myLocked = true;

    // 停止所有计时器
    if (m_mySkillEffectTimer && m_mySkillEffectTimer->isActive()) {
        m_mySkillEffectTimer->stop();
    }

    if (m_gameTimer && m_gameTimer->isActive()) {
        m_gameTimer->stop();
    }

    if (m_syncTimer && m_syncTimer->isActive()) {
        m_syncTimer->stop();
    }

    // 发送游戏结束消息
    NetworkManager *networkManager = NetworkManager::instance();
    if (networkManager && networkManager->isConnected()) {
        QJsonObject endData;
        endData["type"] = "game_end";
        endData["room_id"] = m_roomId;
        endData["player1_score"] = m_myScore;
        endData["player2_score"] = m_opponentScore;
        endData["player1"] = m_myUsername;
        endData["player2"] = m_opponentUsername;

        // 判断胜负
        if (m_myScore > m_opponentScore) {
            endData["winner"] = m_myUsername;
        } else if (m_opponentScore > m_myScore) {
            endData["winner"] = m_opponentUsername;
        } else {
            endData["winner"] = "draw";
        }

        networkManager->sendGameMove(m_opponentUsername, endData);
        networkManager->updateUserStatus("在线", "空闲", "");
    }

    // 显示结果
    bool isWinner = m_myScore > m_opponentScore;
    bool isDraw = m_myScore == m_opponentScore;

    QString result;
    if (isDraw) {
        result = "平局！";
    } else if (isWinner) {
        result = "胜利！";
    } else {
        result = "失败！";
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("游戏结束");
    msgBox.setText(QString("%1\n\n我的分数: %2\n对手分数: %3")
                       .arg(result)
                       .arg(m_myScore)
                       .arg(m_opponentScore));
    msgBox.setStyleSheet(
        "QMessageBox { background-color: #162640; border: 2px solid #00e5ff; border-radius: 10px; }"
        "QLabel { color: white; font: 14pt 'Microsoft YaHei'; }"
        "QPushButton {"
        "   background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00e5ff, stop:1 #0097a7);"
        "   border-radius: 8px;"
        "   color: white;"
        "   font: bold 12pt 'Microsoft YaHei';"
        "   padding: 8px 16px;"
        "   margin: 5px;"
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0097a7, stop:1 #006978); }"
        );
    msgBox.setStandardButtons(QMessageBox::Ok);

    // 禁用主窗口
    this->setEnabled(false);
    msgBox.exec();
    this->setEnabled(true);

    emit gameFinished();
}

// =============== UI更新 ===============
void OnlineGame::updateUI()
{
    updateMyInfo();
    updateOpponentInfo();
    updateCountdown();
}

void OnlineGame::updateMyInfo()
{
    QString info = QString("%1\n分数: %2")
                       .arg(m_myUsername)
                       .arg(m_myScore);
    ui->labelMyInfo->setText(info);
}

void OnlineGame::updateOpponentInfo()
{
    QString info = QString("%1\n分数: %2")
                       .arg(m_opponentUsername)
                       .arg(m_opponentScore);
    ui->labelOpponentInfo->setText(info);
}

void OnlineGame::updateCountdown()
{
    int minutes = m_totalTime / 60;
    int seconds = m_totalTime % 60;
    QString timeStr = QString("%1:%2")
                          .arg(minutes, 2, 10, QChar('0'))
                          .arg(seconds, 2, 10, QChar('0'));
    ui->labelCountdown->setText(timeStr);
}

// =============== 按钮事件 ===============
void OnlineGame::on_btnBack_clicked()
{
    // 如果游戏已经结束，直接返回主菜单
    if (m_gameEnded) {
        emit returnToMenu();
        return;
    }

    if (m_isGameActive) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("确认返回");
        msgBox.setText("退出将判负，是否确定返回？");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setStyleSheet(
            "QMessageBox { background-color: #162640; border: 2px solid #ff5555; border-radius: 10px; }"
            "QLabel { color: white; font: 14pt 'Microsoft YaHei'; }"
            "QPushButton {"
            "   background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #ff5555, stop:1 #cc0000);"
            "   border-radius: 8px;"
            "   color: white;"
            "   font: bold 12pt 'Microsoft YaHei';"
            "   padding: 8px 16px;"
            "   margin: 5px;"
            "}"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #cc0000, stop:1 #990000); }"
            );

        if (msgBox.exec() == QMessageBox::Yes) {
            m_gameTimer->stop();
            m_syncTimer->stop();
            m_isGameActive = false;
            m_myLocked = true;

            // 发送玩家退出游戏的消息 - 根据用户名查找房间
            NetworkManager *networkManager = NetworkManager::instance();
            if (networkManager && networkManager->isConnected()) {
                QJsonObject quitData;
                quitData["type"] = "player_quit";
                // 不发送房间ID，让服务器根据用户名查找
                quitData["quitter"] = m_myUsername;
                quitData["opponent"] = m_opponentUsername;
                quitData["player1_score"] = m_myScore;
                quitData["player2_score"] = m_opponentScore;
                quitData["player1"] = m_myUsername;
                quitData["player2"] = m_opponentUsername;

                qDebug() << "发送退出消息，退出者:" << m_myUsername
                         << "，对手:" << m_opponentUsername
                         << "，房间ID为空:" << m_roomId.isEmpty();

                networkManager->sendRawJson(quitData);

                // 更新状态
                networkManager->updateUserStatus("在线", "空闲", "");
            }

            emit gameFinished();
            close();
        }
    } else {
        emit returnToMenu();
    }
}

void OnlineGame::on_btnMyUndo_clicked()
{
    if (m_myLocked || m_myPaused || m_myUndoStack.isEmpty()) return;

    GameStateSnapshot snapshot = m_myUndoStack.pop();
    m_myBoard->m_grid = snapshot.grid;
    m_myScore = snapshot.score;

    updateMyInfo();
    rebuildMyGrid();
    syncMyBoard();
}

// =============== 技能系统 ===============
// online_game.cpp - 修改on_btnMySkill_clicked函数
void OnlineGame::on_btnMySkill_clicked()
{
    if (m_myLocked || m_myPaused || !m_isGameActive) return;

    if (!m_mySkillTree) {
        showTempMessage("技能树未初始化", QColor(255, 157, 224));
        return;
    }

    QList<SkillNode*> availableSkills;
    QList<SkillNode*> equippedSkills = m_mySkillTree->getEquippedSkills();
    for (SkillNode* skill : equippedSkills) {
        if (!skill->used) {
            availableSkills.append(skill);
        }
    }

    if (availableSkills.isEmpty()) {
        showTempMessage("没有可用技能", QColor(255, 157, 224));
        return;
    }

    // 创建技能选择对话框
    QDialog* skillDialog = new QDialog(this);
    skillDialog->setWindowTitle("技能选择");
    skillDialog->setFixedSize(500, 400);
    skillDialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    skillDialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* mainLayout = new QVBoxLayout(skillDialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFrame* bgFrame = new QFrame(skillDialog);
    bgFrame->setStyleSheet(
        "QFrame {"
        "   background-color: rgba(22, 38, 64, 240);"
        "   border: 2px solid #00e5ff;"
        "   border-radius: 16px;"
        "}"
        );
    mainLayout->addWidget(bgFrame);

    QVBoxLayout* contentLayout = new QVBoxLayout(bgFrame);
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* title = new QLabel("选择技能", bgFrame);
    title->setStyleSheet("color: #00e5ff; font: bold 20pt 'Microsoft YaHei';");
    title->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(title);

    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setSpacing(15);
    contentLayout->addLayout(gridLayout);

    QString btnStyle =
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 10);"
        "   border: 1px solid rgba(0, 229, 255, 100);"
        "   border-radius: 8px;"
        "   color: white;"
        "   font: bold 12pt 'Microsoft YaHei';"
        "   padding: 10px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(0, 229, 255, 40);"
        "   border: 1px solid #00e5ff;"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(0, 229, 255, 80);"
        "}";

    int row = 0, col = 0;
    for (SkillNode* skill : availableSkills) {
        QPushButton* skillBtn = new QPushButton(skill->name, bgFrame);
        skillBtn->setToolTip(skill->description);
        skillBtn->setMinimumHeight(60);
        skillBtn->setCursor(Qt::PointingHandCursor);
        skillBtn->setStyleSheet(btnStyle);

        gridLayout->addWidget(skillBtn, row, col);
        col++;
        if (col > 1) {
            col = 0;
            row++;
        }

        connect(skillBtn, &QPushButton::clicked, skillDialog, [this, skill, skillDialog]() {
            skillDialog->accept();

            // 使用技能
            // 【联机对战特殊处理】技能释放后需要同步状态
            if (skill->id == "row_clear") {
                int row = QRandomGenerator::global()->bounded(ROW);
                playMySpecialEffect(RowBomb, QPoint(row, 0), 0);
                QSet<QPoint> pts;
                for (int c = 0; c < COL; ++c) {
                    if (m_myBoard->m_grid[row][c].pic != -1) {
                        pts.insert(QPoint(row, c));
                    }
                }
                if (!pts.isEmpty()) playMyEliminateAnim(pts);

            } else if (skill->id == "time_extend") {
                m_totalTime += 5;
                updateCountdown();
                showTempMessage("TIME+5s", QColor(0, 229, 255));
            } else if (skill->id == "rainbow_bomb") {
                int color = QRandomGenerator::global()->bounded(6);
                playMySpecialEffect(ColorClear, QPoint(ROW/2, COL/2), color);
                QSet<QPoint> pts;
                for (int r = 0; r < ROW; ++r) {
                    for (int c = 0; c < COL; ++c) {
                        if (m_myBoard->m_grid[r][c].pic == color) {
                            pts.insert(QPoint(r, c));
                        }
                    }
                }
                if (!pts.isEmpty()) playMyEliminateAnim(pts);

            } else if (skill->id == "cross_clear") {
                int cR = QRandomGenerator::global()->bounded(ROW);
                int cC = QRandomGenerator::global()->bounded(COL);
                playMySpecialEffect(RowBomb, QPoint(cR, cC), 0);
                playMySpecialEffect(ColBomb, QPoint(cR, cC), 0);
                QSet<QPoint> pts;
                for (int c=0; c<COL; ++c) if (m_myBoard->m_grid[cR][c].pic != -1) pts.insert(QPoint(cR, c));
                for (int r=0; r<ROW; ++r) if (m_myBoard->m_grid[r][cC].pic != -1) pts.insert(QPoint(r, cC));
                if (!pts.isEmpty()) playMyEliminateAnim(pts);

            } else if (skill->id == "score_double") {
                m_myScoreDoubleActive = true;
                m_mySkillEffectTimer->start(8000);
                showTempMessage("DOUBLE SCORE (8s)", QColor(0, 229, 255));

            } else if (skill->id == "color_unify") {
                m_myColorUnifyActive = true;
                m_mySkillEffectTimer->start(6000);
                playMySpecialEffect(ColorClear, QPoint(0,0), 0);
                int c1 = QRandomGenerator::global()->bounded(6);
                int c2 = QRandomGenerator::global()->bounded(6);
                int c3 = QRandomGenerator::global()->bounded(6);
                for (int r = 0; r < ROW; ++r) {
                    for (int c = 0; c < COL; ++c) {
                        if (m_myBoard->m_grid[r][c].pic != -1) {
                            int ch = QRandomGenerator::global()->bounded(3);
                            m_myBoard->m_grid[r][c].pic = (ch == 0 ? c1 : (ch == 1 ? c2 : c3));
                        }
                    }
                }
                showTempMessage("UNIFY COLOR (6s)", QColor(0, 229, 255));
                rebuildMyGrid();

            } else if (skill->id == "time_freeze") {
                m_totalTime += 15;
                updateCountdown();
                showTempMessage("TIME+15s", QColor(0, 229, 255));

            } else if (skill->id == "ultimate_burst") {
                QSet<QPoint> pts;
                m_myUltimateBurstActive = true;
                playMySpecialEffect(ColorClear, QPoint(0,0), 0);
                for (int r = 0; r < ROW; ++r) {
                    for (int c = 0; c < COL; ++c) {
                        if (m_myBoard->m_grid[r][c].pic != -1) {
                            pts.insert(QPoint(r, c));
                        }
                    }
                }
                int sc = 3200;
                if (m_myScoreDoubleActive) sc *= 2;
                m_myScore += sc;
                updateMyInfo();
                if (!pts.isEmpty()) playMyEliminateAnim(pts);
            }

            skill->used = true;
            syncMyBoard();
        });
    }

    contentLayout->addStretch();

    QPushButton* cancelBtn = new QPushButton("取消", bgFrame);
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
    connect(cancelBtn, &QPushButton::clicked, skillDialog, &QDialog::reject);

    skillDialog->exec();
}

void OnlineGame::resetMySkills()
{
    if (!m_mySkillTree) return;

    const auto& skills = m_mySkillTree->getAllSkills();
    for (SkillNode* skill : skills) {
        skill->used = false;
    }

    m_myScoreDoubleActive = false;
    m_myColorUnifyActive = false;
}

void OnlineGame::onMySkillEffectTimeout()
{
    if (m_myScoreDoubleActive) {
        m_myScoreDoubleActive = false;
        showSkillEndHint("得分翻倍效果结束");
    }

    if (m_myColorUnifyActive) {
        m_myColorUnifyActive = false;
        showSkillEndHint("颜色统一效果结束");
    }
}

void OnlineGame::showSkillEndHint(const QString& message)
{
    QLabel* hintLabel = new QLabel(ui->myBoardContainer);
    hintLabel->setText(message);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet(
        "background-color: rgba(0, 0, 0, 180);"
        "color: #00e5ff;"
        "font: bold 16pt 'Microsoft YaHei';"
        "border-radius: 8px;"
        "border: 1px solid #00e5ff;"
        "padding: 10px;"
        );

    QRect boardRect = ui->myBoardContainer->rect();
    int labelWidth = boardRect.width() * 0.8;
    int labelHeight = 50;
    int labelX = boardRect.left() + (boardRect.width() - labelWidth) / 2;
    int labelY = boardRect.top() + 20;

    hintLabel->setGeometry(labelX, labelY, labelWidth, labelHeight);
    hintLabel->show();
    hintLabel->raise();
    hintLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(hintLabel);
    hintLabel->setGraphicsEffect(effect);

    QSequentialAnimationGroup* animGroup = new QSequentialAnimationGroup(this);

    QPropertyAnimation* fadeIn = new QPropertyAnimation(effect, "opacity");
    fadeIn->setDuration(500);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::InOutQuad);
    animGroup->addAnimation(fadeIn);

    animGroup->addPause(2000);

    QPropertyAnimation* fadeOut = new QPropertyAnimation(effect, "opacity");
    fadeOut->setDuration(500);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    animGroup->addAnimation(fadeOut);

    connect(animGroup, &QAbstractAnimation::finished, this, [hintLabel, animGroup]() {
        hintLabel->deleteLater();
        animGroup->deleteLater();
    });

    animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void OnlineGame::showTempMessage(const QString& message, const QColor& color)
{
    if (!ui->myBoardContainer) return;

    QLabel* lbl = new QLabel(ui->myBoardContainer);
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
    QRect cr = ui->myBoardContainer->rect();
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

    connect(seq, &QAbstractAnimation::finished, this, [lbl, seq]() {
        lbl->deleteLater();
        seq->deleteLater();
    });

    seq->start();
}

// =============== 网络同步 ===============
void OnlineGame::syncMyBoard()
{
    // 基础检查：游戏未激活或锁定时不发送
    if (!m_isGameActive || m_myLocked || m_myPaused) {
        qDebug() << "跳过同步: 游戏未激活或棋盘锁定";
        return;
    }

    // 发送频率限制（最小300ms间隔）
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - m_lastSyncTime < 300) {
        qDebug() << "跳过同步: 发送频率过高";
        return;
    }

    NetworkManager *networkManager = NetworkManager::instance();
    if (!networkManager || !networkManager->isConnected()) {
        qDebug() << "跳过同步: 网络未连接";
        return;
    }

    // 检查棋盘是否有有效方块
    bool hasValidTiles = false;
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            if (m_myBoard->m_grid[r][c].pic >= 0 && m_myBoard->m_grid[r][c].pic < 6) {
                hasValidTiles = true;
                break;
            }
        }
        if (hasValidTiles) break;
    }

    if (!hasValidTiles) {
        qDebug() << "跳过同步: 棋盘无有效方块";
        return;
    }

    // 【核心优化】检查分数是否有增加
    if (!m_hasInitialSync) {
        // 首次同步总是发送
        m_hasInitialSync = true;
        qDebug() << "首次同步: 发送初始状态";
    } else if (m_myScore <= m_lastSyncedScore) {
        // 分数没有增加，检查棋盘是否有变化
        bool boardChanged = false;
        QJsonArray currentBoardArray = boardToJsonArray(m_myBoard->grid());

        // 比较当前棋盘和上次同步的棋盘
        if (currentBoardArray.size() == m_lastBoardArray.size()) {
            for (int i = 0; i < currentBoardArray.size(); ++i) {
                if (currentBoardArray[i].toInt() != m_lastBoardArray[i].toInt()) {
                    boardChanged = true;
                    break;
                }
            }
        } else {
            boardChanged = true;
        }

        if (!boardChanged) {
            qDebug() << "跳过同步: 分数未增加且棋盘无变化";
            return;
        }
        qDebug() << "棋盘发生变化，同步: 分数" << m_myScore << "->" << m_lastSyncedScore;
    }

    // 记录当前状态
    m_lastSyncedScore = m_myScore;
    m_lastSyncedGrid = m_myBoard->grid();  // 假设Grid有拷贝构造函数
    m_lastBoardArray = boardToJsonArray(m_myBoard->grid());
    m_lastSyncTime = currentTime;

    // 构建同步消息
    QJsonObject syncData;
    syncData["type"] = "game_move";
    if (!m_roomId.isEmpty()) {
        syncData["room_id"] = m_roomId;
    }
    syncData["board"] = m_lastBoardArray;
    syncData["score"] = m_myScore;
    syncData["player"] = m_myUsername;
    syncData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 添加调试信息
    static int syncCounter = 0;
    syncCounter++;
    qDebug() << "=== 发送同步 #" << syncCounter << " ===";
    qDebug() << "分数:" << m_myScore;
    qDebug() << "分数增加:" << (m_myScore > m_lastSyncedScore);

    // 发送消息
    networkManager->sendRawJson(syncData);
}

QJsonArray OnlineGame::boardToJsonArray(const Grid &grid)
{
    QJsonArray boardArray;

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            boardArray.append(grid[r][c].pic);
        }
    }

    return boardArray;
}

void OnlineGame::jsonArrayToBoard(const QJsonArray &array, Grid &grid)
{
    int index = 0;
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            if (index < array.size()) {
                grid[r][c].pic = array[index].toInt();
                index++;
            }
        }
    }
}

// =============== 网络消息处理 ===============
void OnlineGame::onServerMessage(const QString &type, const QJsonObject &data)
{
    qDebug() << "OnlineGame received message:" << type;

    if (type == "game_start") {
        onGameStartReceived(data);
    } else if (type == "game_move") {
        onGameMoveReceived(data);
    } else if (type == "game_end") {
        onGameEndReceived(data);
    }
}

void OnlineGame::onGameStartReceived(const QJsonObject &data)
{
    qDebug() << "Game started, room ID:" << data["room_id"].toString();
    m_roomId = data["room_id"].toString();
    m_isGameActive = true;

    // 延迟发送初始状态，确保连接稳定
    QTimer::singleShot(500, this, [this]() {
        syncMyBoard();
    });
}

void OnlineGame::onGameMoveReceived(const QJsonObject &messageData)
{
    qDebug() << "=== OnlineGame收到game_move消息 ===";

    QJsonObject dataToCheck = messageData;

    // 【关键修复】检查是否存在嵌套的data对象
    if (messageData.contains("data") && messageData["data"].isObject()) {
        // 如果有data对象，则使用data对象的内容
        dataToCheck = messageData["data"].toObject();
        qDebug() << "找到嵌套的data对象，使用内部数据";
    }

    // 现在检查board字段
    if (dataToCheck.contains("board") && dataToCheck["board"].isArray()) {
        QJsonArray boardArray = dataToCheck["board"].toArray();
        qDebug() << "成功获取board字段，数组大小:" << boardArray.size();

        // 检查是否是对手的消息
        QString player;
        if (dataToCheck.contains("player")) {
            player = dataToCheck["player"].toString();
            qDebug() << "消息中的玩家:" << player;
        }

        QString opponent;
        if (dataToCheck.contains("opponent")) {
            opponent = dataToCheck["opponent"].toString();
            qDebug() << "消息中的对手:" << opponent;
        }

        // 判断消息来源
        QString sourcePlayer = !player.isEmpty() ? player : opponent;

        // 如果是自己的消息，忽略
        if (sourcePlayer == m_myUsername) {
            qDebug() << "这是自己的消息，忽略";
            return;
        }

        // 如果是对手的消息，更新棋盘
        if (sourcePlayer == m_opponentUsername) {
            int score = 0;
            if (dataToCheck.contains("score")) {
                score = dataToCheck["score"].toInt();
            }

            qDebug() << "更新对手棋盘，分数:" << score;
            updateOpponentFromNetwork(boardArray, score);
        } else {
            qDebug() << "不是目标对手的消息，当前对手:" << m_opponentUsername
                     << "，消息来源:" << sourcePlayer;
        }
    } else {
        qDebug() << "消息中没有board字段或格式不正确";
        qDebug() << "messageData.keys():" << messageData.keys();
        qDebug() << "dataToCheck.keys():" << dataToCheck.keys();

        // 尝试打印整个消息以便调试
        QJsonDocument doc(messageData);
        qDebug() << "完整消息:" << doc.toJson(QJsonDocument::Compact);
    }
}

// 添加玩家退出处理函数
void OnlineGame::onPlayerQuitReceived(const QJsonObject &data)
{
    QString quitter = data["quitter"].toString();
    QString winner = data["winner"].toString();
    QString reason = data["reason"].toString();

    qDebug() << "收到退出消息，退出者:" << quitter
             << "，获胜者:" << winner
             << "，原因:" << reason;

    // 如果是对手退出，则我方获胜
    if (quitter == m_opponentUsername) {
        // 立即停止所有计时器和游戏活动
        m_isGameActive = false;
        m_myLocked = true;

        // 停止所有计时器
        if (m_gameTimer && m_gameTimer->isActive()) {
            m_gameTimer->stop();
        }

        if (m_syncTimer && m_syncTimer->isActive()) {
            m_syncTimer->stop();
        }

        if (m_mySkillEffectTimer && m_mySkillEffectTimer->isActive()) {
            m_mySkillEffectTimer->stop();
        }

        // 更新分数
        if (data.contains("player1_score") && data.contains("player2_score")) {
            m_myScore = data["player1_score"].toInt();
            m_opponentScore = data["player2_score"].toInt();
            updateMyInfo();
            updateOpponentInfo();
        }

        // 显示胜利消息 - 立即执行，不等待
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("游戏结束");
        msgBox.setText(QString("对手已退出，你获得了胜利！\n\n原因: %1\n\n我的分数: %2\n对手分数: %3")
                           .arg(reason)
                           .arg(m_myScore)
                           .arg(m_opponentScore));
        msgBox.setStyleSheet(
            "QMessageBox { background-color: #162640; border: 2px solid #00e5ff; border-radius: 10px; }"
            "QLabel { color: white; font: 14pt 'Microsoft YaHei'; }"
            "QPushButton {"
            "   background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00e5ff, stop:1 #0097a7);"
            "   border-radius: 8px;"
            "   color: white;"
            "   font: bold 12pt 'Microsoft YaHei';"
            "   padding: 8px 16px;"
            "   margin: 5px;"
            "}"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0097a7, stop:1 #006978); }"
            );
        msgBox.setStandardButtons(QMessageBox::Ok);

        // 使用非阻塞方式显示消息框，并在关闭后立即结束游戏
        msgBox.setModal(true);  // 设置为模态
        msgBox.exec();  // 阻塞执行，直到用户关闭

        qDebug() << "胜利弹窗已关闭，准备结束游戏";

        // 更新网络状态
        NetworkManager *networkManager = NetworkManager::instance();
        if (networkManager) {
            networkManager->updateUserStatus("在线", "空闲", "");
        }

        // 立即发射游戏结束信号并关闭窗口
        emit gameFinished();

        // 立即关闭窗口
        this->close();

        return;  // 立即返回，不再执行后续代码
    }
    // 如果是我方退出，已经处理过了，忽略
    else if (quitter == m_myUsername) {
        qDebug() << "收到自己的退出确认，忽略";
    }
}

void OnlineGame::onGameEndReceived(const QJsonObject &data)
{
    qDebug() << "收到游戏结束消息";

    // 如果游戏已经不活动，直接返回
    if (!m_isGameActive) {
        qDebug() << "游戏已经结束，忽略重复的结束消息";
        return;
    }

    // 停止所有计时器
    if (m_gameTimer && m_gameTimer->isActive()) {
        m_gameTimer->stop();
    }

    if (m_syncTimer && m_syncTimer->isActive()) {
        m_syncTimer->stop();
    }

    if (m_mySkillEffectTimer && m_mySkillEffectTimer->isActive()) {
        m_mySkillEffectTimer->stop();
    }

    m_isGameActive = false;
    m_myLocked = true;

    int player1Score = data["player1_score"].toInt();
    int player2Score = data["player2_score"].toInt();
    QString winner = data["winner"].toString();

    bool isWinner = (winner == m_myUsername);
    bool isDraw = (winner == "draw");

    QString result;
    if (isDraw) {
        result = "平局！";
    } else if (isWinner) {
        result = "胜利！";
    } else {
        result = "失败！";
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("游戏结束");
    msgBox.setText(QString("%1\n\n我的分数: %2\n对手分数: %3")
                       .arg(result)
                       .arg(player1Score)
                       .arg(player2Score));
    msgBox.setStyleSheet(
        "QMessageBox { background-color: #162640; border: 2px solid #00e5ff; border-radius: 10px; }"
        "QLabel { color: white; font: 14pt 'Microsoft YaHei'; }"
        "QPushButton {"
        "   background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00e5ff, stop:1 #0097a7);"
        "   border-radius: 8px;"
        "   color: white;"
        "   font: bold 12pt 'Microsoft YaHei';"
        "   padding: 8px 16px;"
        "}"
        );
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setModal(true);  // 设置为模态

    // 显示并等待用户关闭
    msgBox.exec();

    qDebug() << "游戏结束弹窗已关闭";

    NetworkManager *networkManager = NetworkManager::instance();
    if (networkManager) {
        networkManager->updateUserStatus("在线", "空闲", "");
    }

    emit gameFinished();
    this->close();  // 关闭窗口
}

void OnlineGame::endGameWithResult(bool isWinner)
{
    emit gameFinished();
}

QString OnlineGame::getCellImagePath(int colorIndex) const
{
    if (colorIndex < 0 || colorIndex >= 6) {
        colorIndex = 0;
    }
    return QString("%1%2.png").arg(QDir::currentPath() + "/").arg(colorIndex);
}
