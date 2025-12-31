#include "serverwindow.h"
#include "ui_serverwindow.h"
#include "servercore.h"
#include "onlinemanager.h"
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>

// serverwindow.cpp - 修改构造函数，确保所有信号都正确连接
ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ServerWindow)
    , m_serverCore(nullptr)
    , m_onlineManager(nullptr)
    , m_totalConnections(0)
    , m_activeMatches(0)
    , m_totalMatches(0)
{
    ui->setupUi(this);
    setWindowTitle("Match3 Game Server");
    setFixedSize(900, 700);

    setupUI();
    loadStyles();
    setupSystemTray();

    // 初始化服务器核心和在线管理器
    m_serverCore = new ServerCore(this);
    m_onlineManager = new OnlineManager(this);

    // 连接服务器核心信号
    connect(m_serverCore, &ServerCore::logMessage, this, &ServerWindow::addLog);
    connect(m_serverCore, &ServerCore::clientConnected, this, [this]() {
        m_totalConnections++;
        updateStats();
    });
    connect(m_serverCore, &ServerCore::clientDisconnected, this, &ServerWindow::onClientDisconnected);
    connect(m_serverCore, &ServerCore::userLoggedIn, this, &ServerWindow::onUserLoggedIn);
    connect(m_serverCore, &ServerCore::messageReceived, this, &ServerWindow::onMessageReceived);

    // 注意：确保matchStarted和matchEnded信号也存在
    connect(m_serverCore, &ServerCore::matchStarted, this, [this](const QString &player1, const QString &player2) {
        m_activeMatches++;
        m_totalMatches++;
        updateStats();
        addLog(QString("对局开始: %1 vs %2").arg(player1).arg(player2), QColor("#4CAF50"));
    });

    connect(m_serverCore, &ServerCore::matchEnded, this, [this](const QString &player1, const QString &player2) {
        m_activeMatches--;
        updateStats();
        addLog(QString("对局结束: %1 vs %2").arg(player1).arg(player2), QColor("#FF9800"));
    });

    // 设置统计更新定时器
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(2000);
    connect(m_statsTimer, &QTimer::timeout, this, &ServerWindow::updateStats);

    // 连接按钮信号
    connect(ui->btnStart, &QPushButton::clicked, this, &ServerWindow::onStartServer);
    connect(ui->btnStop, &QPushButton::clicked, this, &ServerWindow::onStopServer);
    connect(ui->btnClearLog, &QPushButton::clicked, this, &ServerWindow::onClearLog);
    connect(ui->btnExportLog, &QPushButton::clicked, this, &ServerWindow::onExportLog);
    connect(ui->btnRefresh, &QPushButton::clicked, this, &ServerWindow::refreshOnlineList);
    connect(ui->btnInfo, &QPushButton::clicked, this, &ServerWindow::showServerInfo);

    // 初始化UI状态
    onStopServer();
}


void ServerWindow::setupUI()
{
    // 设置表格
    ui->tableOnline->setColumnCount(5);
    ui->tableOnline->setHorizontalHeaderLabels({"用户名", "IP地址", "连接时间", "状态", "游戏模式"});
    ui->tableOnline->horizontalHeader()->setStretchLastSection(true);
    ui->tableOnline->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 设置日志区域只读
    ui->textLog->setReadOnly(true);

    // 初始化状态栏
    updateStatusBar();
}

void ServerWindow::loadStyles()
{
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        setStyleSheet(style);
        styleFile.close();
    } else {
        // 默认样式
        setStyleSheet(R"(
            QMainWindow {
                background-color: #1a1a2e;
            }
            QGroupBox {
                color: #fff;
                font: bold 12pt;
                border: 2px solid #16213e;
                border-radius: 10px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
            }
            QTableWidget {
                background-color: #16213e;
                color: #fff;
                border: none;
                gridline-color: #0f3460;
                font: 10pt;
            }
            QTableWidget::item {
                padding: 5px;
            }
            QTableWidget::item:selected {
                background-color: #e94560;
            }
            QHeaderView::section {
                background-color: #0f3460;
                color: #fff;
                padding: 5px;
                border: none;
                font: bold;
            }
            QTextEdit {
                background-color: #16213e;
                color: #fff;
                border: 1px solid #0f3460;
                font: 10pt "Consolas";
            }
            QPushButton {
                background-color: #0f3460;
                color: white;
                border: none;
                padding: 8px 16px;
                border-radius: 5px;
                font: bold 11pt;
            }
            QPushButton:hover {
                background-color: #16213e;
            }
            QPushButton:pressed {
                background-color: #e94560;
            }
            QPushButton:disabled {
                background-color: #2c3e50;
                color: #7f8c8d;
            }
            QLabel {
                color: #fff;
                font: 10pt;
            }
        )");
    }
}

void ServerWindow::setupSystemTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icon.png"));
    m_trayIcon->setToolTip("Match3 Server");

    QMenu *trayMenu = new QMenu(this);
    trayMenu->addAction("显示窗口", this, &QWidget::showNormal);
    trayMenu->addSeparator();
    trayMenu->addAction("退出", qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &ServerWindow::onSystemTrayActivated);
}

void ServerWindow::addLog(const QString &message, const QColor &color)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString html = QString("<font color=\"%1\">[%2] %3</font>")
                       .arg(color.name())
                       .arg(timestamp)
                       .arg(message.toHtmlEscaped());

    ui->textLog->append(html);

    // 限制日志行数
    if (ui->textLog->document()->lineCount() > 1000) {
        QTextCursor cursor = ui->textLog->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
        cursor.removeSelectedText();
    }
}

void ServerWindow::updateStatusBar()
{
    QString status = QString("服务器: %1 | 在线用户: %2 | 活跃对局: %3 | 总对局: %4")
                         .arg(ui->btnStart->isEnabled() ? "已停止" : "运行中")
                         .arg(ui->tableOnline->rowCount())
                         .arg(m_activeMatches)
                         .arg(m_totalMatches);

    ui->statusBar->showMessage(status);
}

void ServerWindow::updateStats()
{
    // 更新统计标签
    ui->labelConnections->setText(QString::number(m_totalConnections));
    ui->labelOnline->setText(QString::number(ui->tableOnline->rowCount()));
    ui->labelMatches->setText(QString::number(m_activeMatches));

    updateStatusBar();
}

// serverwindow.cpp - 确保refreshOnlineList函数正确显示在线用户
// serverwindow.cpp - 修改refreshOnlineList函数
void ServerWindow::refreshOnlineList()
{
    if (!m_onlineManager) {
        addLog("OnlineManager未初始化，无法刷新在线列表", QColor("#F44336"));
        return;
    }

    ui->tableOnline->setRowCount(0);
    auto onlineUsers = m_onlineManager->getOnlineUsers();

    qDebug() << "刷新在线列表，用户数量:" << onlineUsers.size();

    for (const auto &user : onlineUsers) {
        int row = ui->tableOnline->rowCount();
        ui->tableOnline->insertRow(row);

        ui->tableOnline->setItem(row, 0, new QTableWidgetItem(user.username));
        ui->tableOnline->setItem(row, 1, new QTableWidgetItem(user.ipAddress));
        ui->tableOnline->setItem(row, 2, new QTableWidgetItem(user.connectTime.toString("yyyy-MM-dd HH:mm:ss")));
        ui->tableOnline->setItem(row, 3, new QTableWidgetItem(user.status));
        ui->tableOnline->setItem(row, 4, new QTableWidgetItem(user.gameMode));

        // 根据状态设置行颜色
        if (user.status == "游戏中") {
            for (int col = 0; col < 5; col++) {
                ui->tableOnline->item(row, col)->setBackground(QColor(33, 150, 243, 50)); // 蓝色背景
            }
        } else if (user.status == "联机游戏中") {
            for (int col = 0; col < 5; col++) {
                ui->tableOnline->item(row, col)->setBackground(QColor(255, 152, 0, 50)); // 橙色背景
            }
        } else if (user.status == "匹配中") {
            for (int col = 0; col < 5; col++) {
                ui->tableOnline->item(row, col)->setBackground(QColor(0, 188, 212, 50)); // 青色背景
            }
        }
    }

    updateStats();
}

void ServerWindow::onStartServer()
{
    if (m_serverCore->startServer(12345)) {
        ui->btnStart->setEnabled(false);
        ui->btnStop->setEnabled(true);
        addLog("服务器启动成功，监听端口: 12345", QColor("#4CAF50"));
        m_statsTimer->start();

        m_trayIcon->showMessage("服务器启动", "Match3游戏服务器已启动",
                                QSystemTrayIcon::Information, 3000);
    } else {
        addLog("服务器启动失败", QColor("#F44336"));
    }
}

// serverwindow.cpp - 修改onStopServer函数
void ServerWindow::onStopServer()
{
    m_serverCore->stopServer();
    ui->btnStart->setEnabled(true);
    ui->btnStop->setEnabled(false);
    addLog("服务器已停止", QColor("#FF9800"));
    m_statsTimer->stop();

    // 清空在线管理器
    if (m_onlineManager) {
        // 获取所有在线用户并逐个移除
        auto onlineUsers = m_onlineManager->getOnlineUsers();
        for (const auto &user : onlineUsers) {
            m_onlineManager->removeUser(user.username);
        }
    }

    // 清空在线列表显示
    ui->tableOnline->setRowCount(0);
    updateStats();
}

void ServerWindow::onClearLog()
{
    ui->textLog->clear();
    addLog("日志已清空", QColor("#2196F3"));
}

void ServerWindow::onExportLog()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出日志",
                                                    QString("server_log_%1.txt")
                                                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
                                                    "文本文件 (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        file.write(ui->textLog->toPlainText().toUtf8());
        file.close();
        addLog(QString("日志已导出到: %1").arg(fileName), QColor("#4CAF50"));
    } else {
        QMessageBox::warning(this, "错误", "无法保存文件");
    }
}

void ServerWindow::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        if (isHidden()) {
            showNormal();
            activateWindow();
        } else {
            hide();
        }
    }
}

void ServerWindow::showServerInfo()
{
    QString info = QString(
        "Match3 Game Server v1.0.0\n\n"
        "作者: Match3开发团队\n"
        "协议: TCP\n"
        "端口: 12345\n"
        "数据库: MySQL\n\n"
        "功能特性:\n"
        "• 实时在线状态管理\n"
        "• 智能匹配系统\n"
        "• 游戏数据同步\n"
        "• 战绩记录统计"
        );

    QMessageBox::information(this, "服务器信息", info);
}

// 【新增】处理用户登录成功
void ServerWindow::onUserLoggedIn(const QString &username, const QString &ipAddress)
{
    if (m_onlineManager) {
        // 添加到在线管理器
        m_onlineManager->addUser(username, ipAddress);

        // 更新在线列表显示
        refreshOnlineList();
    }
}

// 【新增】处理服务器消息
// serverwindow.cpp - 修改onMessageReceived函数
void ServerWindow::onMessageReceived(const QString &from, const QString &type, const QJsonObject &data)
{
    qDebug() << "ServerWindow收到消息:" << from << "类型:" << type << "数据:" << data;

    if (type == "user_status") {
        QString status = data["status"].toString();
        QString gameMode = data["game_mode"].toString();
        QString opponent = data["opponent"].toString();

        // 更新在线管理器
        if (m_onlineManager) {
            m_onlineManager->updateUserStatus(from, status, gameMode, opponent);
            refreshOnlineList();
        }

        // 记录状态变化日志
        QString logMsg;
        if (opponent.isEmpty()) {
            logMsg = QString("用户状态更新: %1 -> %2").arg(from).arg(status);
        } else {
            logMsg = QString("用户状态更新: %1 -> %2 (对手: %3)").arg(from).arg(status).arg(opponent);
        }

        // 根据状态设置不同颜色
        QColor color = Qt::white;
        if (status == "在线") color = QColor("#4CAF50"); // 绿色
        else if (status == "游戏中") color = QColor("#2196F3"); // 蓝色
        else if (status == "联机游戏中") color = QColor("#FF9800"); // 橙色
        else if (status == "技能选择中") color = QColor("#9C27B0"); // 紫色
        else if (status == "匹配中") color = QColor("#00BCD4"); // 青色

        addLog(logMsg, color);
    }
}


// 【新增】处理客户端断开连接
void ServerWindow::onClientDisconnected(const QString &username)
{
    if (m_onlineManager && !username.isEmpty()) {
        // 从在线管理器中移除用户
        m_onlineManager->removeUser(username);

        // 更新在线列表显示
        refreshOnlineList();
    }
}

ServerWindow::~ServerWindow()
{
    delete ui;
}
