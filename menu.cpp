// menu.cpp
#include "menu.h"
#include "ui_menu.h"
#include <QDebug>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QTimer>
#include <QLabel>
#include <QTextEdit>
#include <QScrollBar>
#include <QFrame>
#include "mainwindow.h"
#include "online_menu.h"
#include "online_game.h"
#include "musicsetting.h"

Menu::Menu(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Menu)
    , m_onlineMenu(nullptr)
    , m_onlineGame(nullptr)
    , m_onlinePageIndex(-1)
    , m_networkManager(nullptr)
{
    ui->setupUi(this);
    rightStack = ui->stackRight;

    // 初始化网络管理器
    m_networkManager = NetworkManager::instance();
    setupNetworkConnections();

    // 初始化时播放菜单背景音乐
    MusicManager::instance().playSceneMusic(MusicManager::MusicScene::Menu);
}

Menu::~Menu()
{
    delete ui;
}

void Menu::setupNetworkConnections()
{
    if (!m_networkManager) return;

    // 连接网络信号
    connect(m_networkManager, &NetworkManager::loginResult,
            this, &Menu::onLoginResult);
    connect(m_networkManager, &NetworkManager::onlineListUpdated,
            this, &Menu::onOnlineListUpdated);
    connect(m_networkManager, &NetworkManager::matchResponse,
            this, &Menu::onMatchResponse);
    connect(m_networkManager, &NetworkManager::matchQueued,
            this, &Menu::onMatchQueued);
    connect(m_networkManager, &NetworkManager::matchFound,
            this, &Menu::onMatchFound);
    connect(m_networkManager, &NetworkManager::matchCancelled,
            this, &Menu::onMatchCancelled);
    connect(m_networkManager, &NetworkManager::systemMessage,
            this, &Menu::onSystemMessage);
    connect(m_networkManager, &NetworkManager::serverShutdown,
            this, &Menu::onServerShutdown);
    connect(m_networkManager, &NetworkManager::chatMessageReceived,
            this, &Menu::onChatMessageReceived);
    connect(m_networkManager, &NetworkManager::gameMoveReceived,
            this, &Menu::onGameMoveReceived);
}

QString Menu::getMyUsername() const
{
    // 从主窗口获取当前用户名
    QWidget *mainWindow = this->window();
    MainWindow *mainWin = qobject_cast<MainWindow*>(mainWindow);
    return mainWin ? mainWin->getCurrentUsername() : "Guest";
}

/* 左侧【单机游戏】被点击 */
void Menu::on_btnSingle_clicked()
{
    static int modeIndex = -1;
    if (modeIndex == -1) {
        QWidget *modePage = createModeSelectPage();
        modeIndex = rightStack->addWidget(modePage);
    }
    rightStack->setCurrentIndex(modeIndex);
}

/* 工厂：生成模式选择页 */
QWidget *Menu::createModeSelectPage()
{
    QWidget *page = new QWidget();
    page->setObjectName("modePage");
    page->setStyleSheet("#modePage{background:transparent;}");

    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setSpacing(20);
    lay->setContentsMargins(60, 80, 60, 80);

    // 标题
    QLabel *title = new QLabel("选择模式", page);
    title->setStyleSheet("color:#ff9de0;font:700 28pt 'Microsoft YaHei';");
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    // 三按钮  宽度固定 220 px
    modeGroup = new QButtonGroup(page);
    const QStringList modes = {"闪电", "旋风", "霓虹"};
    for (int i = 0; i < modes.size(); ++i) {
        QPushButton *btn = new QPushButton(modes[i], page);
        btn->setFixedSize(220, 54);
        btn->setCheckable(true);
        btn->setStyleSheet(
            "QPushButton{"
            "color:#fff;font:18pt 'Microsoft YaHei';"
            "border:2px solid #ff9de0;border-radius:12px;"
            "background:rgba(255,157,224,40);"
            "}"
            "QPushButton:hover{"
            "border-color:#ff6bcb;background:rgba(255,107,203,80);"
            "}"
            "QPushButton:checked{"
            "border-color:#ff4fa5;background:rgba(255,79,165,120);"
            "}");

        QHBoxLayout *hLay = new QHBoxLayout();
        hLay->addStretch();
        hLay->addWidget(btn);
        hLay->addStretch();
        lay->addLayout(hLay);

        modeGroup->addButton(btn, i);
    }

    connect(modeGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, &Menu::onModeSelected);

    // 开始游戏按钮  260 px 宽，居中
    QPushButton *startBtn = new QPushButton("开始游戏", page);
    startBtn->setObjectName("startBtn");
    startBtn->setEnabled(false);
    startBtn->setFixedSize(260, 50);
    startBtn->setStyleSheet(
        "QPushButton{"
        "color:#fff;font:700 16pt 'Microsoft YaHei';"
        "border:none;border-radius:12px;"
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #4caf50, stop:1 #66bb6a);"
        "}"
        "QPushButton:disabled{background:#555;}"
        "QPushButton:hover:!disabled{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #66bb6a, stop:1 #81c784);}"
        "QPushButton:pressed:!disabled{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #388e3c, stop:1 #4caf50);}");

    QHBoxLayout *startLay = new QHBoxLayout();
    startLay->addStretch();
    startLay->addWidget(startBtn);
    startLay->addStretch();
    lay->addStretch(2);
    lay->addLayout(startLay);

    // 选中模式后才允许点开始
    connect(modeGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            [startBtn](QAbstractButton *){ startBtn->setEnabled(true); });

    // 正式开始游戏
    connect(startBtn, &QPushButton::clicked, [this](){
        QString mode = modeGroup->checkedButton()->text();
        emit startGameRequested(mode);

        // 切换到游戏场景音乐
        MusicManager::instance().playSceneMusic(MusicManager::MusicScene::Playing);
    });

    return page;
}

void Menu::onModeSelected(QAbstractButton *btn)
{
    qDebug() << "当前选中模式：" << btn->text();
}

/* 左侧【联机对战】被点击 */
void Menu::on_btnOnline_clicked()
{
    // 首次进入才创建联机对战页
    if (m_onlinePageIndex == -1) {
        QWidget *onlinePage = createOnlinePage();
        m_onlinePageIndex = rightStack->addWidget(onlinePage);
    }
    rightStack->setCurrentIndex(m_onlinePageIndex);

    // 请求更新在线列表
    if (m_networkManager && m_networkManager->isConnected()) {
        m_networkManager->requestOnlineList();
    }
}

QWidget *Menu::createOnlinePage()
{
    m_onlineMenu = new OnlineMenu(this);

    // 连接联机菜单的信号
    connect(m_onlineMenu, &OnlineMenu::matchRequested, this, &Menu::onMatchRequested);
    connect(m_onlineMenu, &OnlineMenu::cancelMatchRequested, this, &Menu::onCancelMatchRequested);
    connect(m_onlineMenu, &OnlineMenu::backRequested, this, [this]() {
        rightStack->setCurrentIndex(0);
    });

    return m_onlineMenu;
}

/* 左侧【技能选择】被点击 */
void Menu::on_btnSkill_clicked()
{
    // 获取主窗口指针
    QWidget *mainWindow = this->window();
    MainWindow *mainWin = qobject_cast<MainWindow*>(mainWindow);
    if (mainWin) {
        // 更新状态为"技能选择中"
        NetworkManager *networkManager = NetworkManager::instance();
        if (networkManager && networkManager->isConnected()) {
            networkManager->updateUserStatus("技能选择中", "空闲");
        }
        mainWin->switchToSkillTreePage();
    }
}

/* 左侧【排行榜】被点击 */
void Menu::on_btnRank_clicked()
{
    QWidget *mainWindow = this->window();
    MainWindow *mainWin = qobject_cast<MainWindow*>(mainWindow);
    if (mainWin) {
        mainWin->switchToRankPage();
    }
}

/* 左侧【规则】被点击 */
void Menu::on_btnRule_clicked()
{
    // 使用静态变量缓存页面索引，避免重复创建
    static int rulePageIndex = -1;

    if (rulePageIndex == -1) {
        QWidget *rulePage = createRulePage();
        rulePageIndex = rightStack->addWidget(rulePage);
    }

    // 切换到规则页
    rightStack->setCurrentIndex(rulePageIndex);
}

QWidget *Menu::createRulePage()
{
    QWidget *page = new QWidget();
    page->setObjectName("rulePage");

    // ============================================================
    // 1. 整体容器样式：磨砂黑底 + 霓虹粉微光边框 + 圆角
    // ============================================================
    page->setStyleSheet(
        "#rulePage {"
        "   background-color: rgba(20, 20, 35, 230);" // 深蓝黑，高透明度背景
        "   border: 1px solid rgba(255, 157, 224, 0.3);" // 极细的粉色边框
        "   border-radius: 16px;"
        "}"
        );

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30); // 增加内边距
    layout->setSpacing(20);

    // ============================================================
    // 2. 标题区域 (带渐变装饰线)
    // ============================================================
    QLabel *title = new QLabel("GAME RULES", page);
    title->setStyleSheet(
        "color: #ff9de0;" // 霓虹粉
        "font: 900 24pt 'Arial Black';" // 使用厚重的英文字体更有科技感
        "letter-spacing: 2px;"
        );
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 装饰线：中间青色，两边透明的渐变线条
    QFrame *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 transparent, stop:0.5 #00e5ff, stop:1 transparent); min-height: 2px; border:none;");
    layout->addWidget(line);

    // ============================================================
    // 3. 规则文本区域 (富文本 + 滚动条美化)
    // ============================================================
    QTextEdit *ruleText = new QTextEdit(page);
    ruleText->setReadOnly(true);
    ruleText->setFrameShape(QFrame::NoFrame);

    // 【关键】美化滚动条和文本域背景
    ruleText->setStyleSheet(
        "QTextEdit {"
        "   background: transparent;" // 背景透明，透出底部的磨砂玻璃
        "   color: #e0e0e0;"          // 文字银灰色
        "   font: 13pt 'Microsoft YaHei';"
        "   selection-background-color: #00e5ff;" // 选中文字变青色
        "   selection-color: #000;"
        "}"
        // --- 垂直滚动条美化 ---
        "QScrollBar:vertical {"
        "   background: rgba(0,0,0,50);" // 轨道半透明
        "   width: 8px;"                 // 极细
        "   margin: 0px;"
        "   border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: #555;"           // 滑块深灰
        "   min-height: 20px;"
        "   border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #ff9de0; }" // 悬停变粉
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }" // 隐藏上下箭头
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        );

    // 使用 HTML 设置多彩文字
    QString htmlContent =
        "<div style='line-height:160%;'>"

        "<p><span style='font-size:15pt; font-weight:bold; color:#ffffff;'>▌ 基础玩法</span><br>"
        "通过交换相邻的方块，使 <span style='color:#ffd700; font-weight:bold;'>3个或更多</span> 相同方块连成一线即可消除。</p>"
        "<br>"

        "<p><span style='font-size:15pt; font-weight:bold; color:#ff9de0;'>▌ 闪电模式 (Lightning)</span><br>"
        "限时 <span style='color:#ff9de0;'>3分钟</span>！这是手速与眼力的极致挑战，连击越高分数加成越高。</p>"
        "<br>"

        "<p><span style='font-size:15pt; font-weight:bold; color:#00e5ff;'>▌ 旋风模式 (Cyclone)</span><br>"
        "右键点击可以 <span style='color:#00e5ff;'>旋转 2x2 区域</span>。策略至上，寻找连锁消除的最佳机会。</p>"
        "<br>"

        "<p><span style='font-size:15pt; font-weight:bold; color:#7cffcb;'>▌ 霓虹模式 (Shapeshift)</span><br>"
        "特殊方块会随机变换颜色。抓住它们变色的瞬间，打出意想不到的 <span style='color:#7cffcb;'>Combo</span>！</p>"
        "<br>"

        "<p><span style='font-size:15pt; font-weight:bold; color:#ffeb3b;'>▌ 技能系统</span><br>"
        "积累消除数，释放强力技能！<br>"
        "• <span style='color:#00e5ff;'>时光倒流</span>：恢复步数或时间<br>"
        "• <span style='color:#ff9de0;'>全屏爆破</span>：消除屏幕上所有指定颜色的方块</p>"

        "</div>";

    ruleText->setHtml(htmlContent);
    layout->addWidget(ruleText);

    // ============================================================
    // 4. 底部按钮 (全息科技风格)
    // ============================================================
    QPushButton *aiDemoBtn = new QPushButton("启动 AI 演示 [BETA]", page);
    aiDemoBtn->setCursor(Qt::PointingHandCursor);
    aiDemoBtn->setFixedHeight(50); // 稍微矮一点，显得更精致

    aiDemoBtn->setStyleSheet(
        "QPushButton {"
        "   color: #00e5ff;" // 青色文字
        "   font: bold 14pt 'Microsoft YaHei';"
        "   background-color: rgba(0, 229, 255, 15);" // 极淡的青色背景
        "   border: 1px solid #00e5ff;" // 青色边框
        "   border-radius: 8px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(0, 229, 255, 40);" // 悬停稍微变亮
        "   color: #ffffff;" // 文字变白
        "   border: 1px solid #ffffff;" // 边框变白
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(0, 229, 255, 70);"
        "   padding-top: 2px;"
        "}"
        );

    connect(aiDemoBtn, &QPushButton::clicked, this, [this](){
        emit startAIDemoRequested();   // 发送信号
    });

    layout->addWidget(aiDemoBtn);

    return page;
}

/* 左侧【设置】被点击 */
void Menu::on_btnSettings_clicked()
{
    // 创建并切换到音乐设置界面
    static int musicSettingIndex = -1;
    if (musicSettingIndex == -1) {
        MusicSetting *musicSettingPage = new MusicSetting(this);
        musicSettingPage->setObjectName("musicSettingPage");
        musicSettingPage->setStyleSheet("#musicSettingPage{background:transparent;}");
        musicSettingIndex = rightStack->addWidget(musicSettingPage);
    }
    rightStack->setCurrentIndex(musicSettingIndex);
}

/* 网络事件处理函数 */
void Menu::onLoginResult(bool success, const QString &message, const QJsonObject &userData)
{
    qDebug() << "登录结果:" << success << message;
    if (success) {
        // 保存当前用户名
        QWidget *mainWindow = this->window();
        MainWindow *mainWin = qobject_cast<MainWindow*>(mainWindow);
        if (mainWin) {
            m_currentUsername = mainWin->getCurrentUsername();
        }
    }
}

// menu.cpp - 在onOnlineListUpdated函数中更新在线列表显示
void Menu::onOnlineListUpdated(const QJsonArray &users)
{
    qDebug() << "在线列表更新，用户数:" << users.size();

    // 如果在线菜单存在，更新显示
    if (m_onlineMenu) {
        // 也可以简单显示在线用户数量
        QString status = QString("在线玩家: %1人").arg(users.size());
        // 可以通过某种方式传递给online_menu的labelHint显示
    }

    // 详细打印在线用户信息（调试用）
    for (const QJsonValue &userValue : users) {
        QJsonObject user = userValue.toObject();
        qDebug() << "在线用户:" << user["username"].toString()
                 << "状态:" << user["status"].toString()
                 << "模式:" << user["game_mode"].toString();
    }
}

void Menu::onMatchResponse(bool success, const QString &message, int queuePosition)
{
    qDebug() << "匹配响应:" << success << message << "位置:" << queuePosition;
    // 可以传递给online_menu显示
    if (m_onlineMenu) {
        // 调用online_menu的方法更新显示
    }
}

void Menu::onMatchQueued(int queuePosition)
{
    qDebug() << "排队位置:" << queuePosition;
    // 可以传递给online_menu显示排队位置
}

// menu.cpp - 修改onMatchFound函数
void Menu::onMatchFound(const QString &player1, const QString &player2)
{
    qDebug() << "Menu收到匹配成功消息: player1=" << player1 << "player2=" << player2;

    NetworkManager *networkManager = NetworkManager::instance();
    QString myUsername = networkManager ? networkManager->getUsername() : "";

    qDebug() << "我的用户名:" << myUsername;

    if (myUsername.isEmpty()) {
        qDebug() << "无法获取当前用户名";
        return;
    }

    QString opponentName;

    if (player1 == myUsername) {
        opponentName = player2;
    } else if (player2 == myUsername) {
        opponentName = player1;
    } else {
        qDebug() << "匹配成功但用户不在匹配中";
        return;
    }

    qDebug() << "开始联机游戏:" << myUsername << "vs" << opponentName;

    // 【重要】等待0.5秒，确保在线菜单已处理完匹配成功状态
    QTimer::singleShot(500, this, [this, myUsername, opponentName]() {
        onStartOnlineGame(myUsername, opponentName);
    });
}

void Menu::onMatchCancelled()
{
    qDebug() << "匹配取消";
    // 可以传递给online_menu显示
}

void Menu::onSystemMessage(const QString &message)
{
    qDebug() << "系统消息:" << message;
    // 可以显示给用户
    QMessageBox::information(this, "系统消息", message);
}

void Menu::onServerShutdown(const QString &message)
{
    qDebug() << "服务器关闭:" << message;
    QMessageBox::warning(this, "服务器关闭", message);
}

void Menu::onChatMessageReceived(const QString &from, const QString &message, const QString &timestamp)
{
    qDebug() << "收到聊天消息:" << from << ":" << message << "时间:" << timestamp;
    // 可以显示聊天消息
}

void Menu::onGameMoveReceived(const QJsonObject &moveData)
{
    qDebug() << "收到游戏操作:" << moveData;
    // 如果正在联机游戏中，转发给游戏窗口
    if (m_onlineGame) {
        // 这里需要OnlineGame有处理游戏操作的方法
    }
}

/* 联机对战相关函数 */
void Menu::onMatchRequested(const QString &gameMode)
{
    if (m_networkManager && m_networkManager->isConnected()) {
        qDebug() << "发送匹配请求，模式:" << gameMode;
        m_networkManager->sendMatchRequest(gameMode);
    } else {
        QMessageBox::warning(this, "网络错误", "未连接到服务器");
    }
}

void Menu::onCancelMatchRequested()
{
    if (m_networkManager && m_networkManager->isConnected()) {
        m_networkManager->cancelMatchRequest();
    }
}

// 修改onStartOnlineGame函数
void Menu::onStartOnlineGame(const QString &myUsername, const QString &opponentName)
{
    qDebug() << "创建联机游戏窗口:" << myUsername << "vs" << opponentName;

    // 如果已经有游戏窗口，先清理
    if (m_onlineGame) {
        m_onlineGame->deleteLater();
        m_onlineGame = nullptr;
    }

    // 创建联机游戏窗口
    m_onlineGame = new OnlineGame(myUsername, opponentName, this);

    // 【关键修复】从主窗口获取技能树并设置到联机游戏中
    QWidget *mainWindow = this->window();
    MainWindow *mainWin = qobject_cast<MainWindow*>(mainWindow);
    if (mainWin) {
        // 获取技能树并设置到联机游戏中
        SkillTree *skillTree = mainWin->getSkillTree(); // 需要添加这个getter方法
        if (skillTree) {
            m_onlineGame->setMySkillTree(skillTree);
        }
    }

    // 连接游戏结束信号
    connect(m_onlineGame, &OnlineGame::gameFinished, this, [this]() {
        qDebug() << "联机游戏结束";
        if (m_onlineGame) {
            m_onlineGame->deleteLater();
            m_onlineGame = nullptr;
        }
        // 切换回菜单页面
        if (rightStack) {
            rightStack->setCurrentIndex(0);
        }
    });

    connect(m_onlineGame, &OnlineGame::returnToMenu, this, [this]() {
        qDebug() << "返回菜单";
        if (m_onlineGame) {
            m_onlineGame->deleteLater();
            m_onlineGame = nullptr;
        }
        // 切换回菜单页面
        if (rightStack) {
            rightStack->setCurrentIndex(0);
        }
    });

    // 显示游戏窗口
    m_onlineGame->show();

    // 隐藏当前窗口（如果是独立窗口）
    if (this->isWindow()) {
        this->hide();
    }
}
