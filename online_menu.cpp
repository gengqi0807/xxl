// online_menu.cpp
#include "online_menu.h"
#include "ui_online_menu.h"
#include <QDebug>

OnlineMenu::OnlineMenu(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OnlineMenu)
    , m_matchSeconds(0)
    , m_isMatching(false)
    , m_networkManager(nullptr)
{
    ui->setupUi(this);

    // 获取网络管理器实例
    m_networkManager = NetworkManager::instance();

    // 连接网络信号
    if (m_networkManager) {
        connect(m_networkManager, &NetworkManager::matchResponse,
                this, &OnlineMenu::onMatchResponse);
        connect(m_networkManager, &NetworkManager::matchQueued,
                this, &OnlineMenu::onMatchQueued);
        connect(m_networkManager, &NetworkManager::matchFound,
                this, &OnlineMenu::onMatchFound);
        connect(m_networkManager, &NetworkManager::matchCancelled,
                this, &OnlineMenu::onMatchCancelled);
        connect(m_networkManager, &NetworkManager::onlineListUpdated,
                this, &OnlineMenu::onOnlineListUpdated);

        // 初始请求在线列表
        if (m_networkManager->isConnected()) {
            m_networkManager->requestOnlineList();
        }
    }

    // 初始化计时器
    m_matchTimer = new QTimer(this);
    m_matchTimer->setInterval(1000); // 1秒触发一次
    connect(m_matchTimer, &QTimer::timeout, this, &OnlineMenu::onMatchTimerTick);

    // 初始状态
    updateMatchStatus("等待开始匹配...");

    // 更新在线用户数显示
    updateOnlineUserCount(0);
}

OnlineMenu::~OnlineMenu()
{
    delete ui;
}

void OnlineMenu::on_btnStartMatch_clicked()
{
    if (!m_isMatching) {
        // 检查网络连接
        if (!m_networkManager || !m_networkManager->isConnected()) {
            updateMatchStatus("未连接到服务器");
            QTimer::singleShot(2000, [this]() {
                updateMatchStatus("等待开始匹配...");
            });
            return;
        }

        // 开始匹配
        m_isMatching = true;
        m_matchSeconds = 0;

        // 更新UI
        ui->btnStartMatch->setVisible(false);
        ui->btnCancelMatch->setVisible(true);
        updateMatchStatus("正在寻找对手...");

        // 启动计时器
        m_matchTimer->start();
        ui->labelTime->setText("00:00");

        // 发射匹配请求信号（闪电模式）
        emit matchRequested("闪电");

        qDebug() << "开始匹配请求...";
    }
}

void OnlineMenu::on_btnCancelMatch_clicked()
{
    if (m_isMatching) {
        // 取消匹配
        m_isMatching = false;

        // 更新UI
        ui->btnStartMatch->setVisible(true);
        ui->btnCancelMatch->setVisible(false);
        updateMatchStatus("匹配已取消");

        // 停止计时器
        m_matchTimer->stop();

        // 发射取消匹配信号
        emit cancelMatchRequested();
    }
}

void OnlineMenu::onMatchTimerTick()
{
    m_matchSeconds++;

    // 格式化时间显示 mm:ss
    int minutes = m_matchSeconds / 60;
    int seconds = m_matchSeconds % 60;
    QString timeStr = QString("%1:%2")
                          .arg(minutes, 2, 10, QChar('0'))
                          .arg(seconds, 2, 10, QChar('0'));

    ui->labelTime->setText(timeStr);

    // 如果匹配时间过长（超过30秒），显示提示
    if (m_isMatching && m_matchSeconds > 30) {
        updateMatchStatus("匹配时间较长，请耐心等待...");
    }
}

void OnlineMenu::updateMatchStatus(const QString &status)
{
    ui->labelStatus->setText(status);

    // 根据状态更新颜色
    if (status.contains("成功")) {
        ui->labelStatus->setStyleSheet("color:#4caf50;font:16pt 'Microsoft YaHei';");
    } else if (status.contains("取消") || status.contains("失败")) {
        ui->labelStatus->setStyleSheet("color:#ff5555;font:16pt 'Microsoft YaHei';");
    } else if (status.contains("等待")) {
        ui->labelStatus->setStyleSheet("color:#aaa;font:16pt 'Microsoft YaHei';");
    } else {
        ui->labelStatus->setStyleSheet("color:#ff9de0;font:16pt 'Microsoft YaHei';");
    }
}

void OnlineMenu::updateQueuePosition(int position)
{
    if (position > 0) {
        QString status = QString("排队中... 位置: %1").arg(position);
        updateMatchStatus(status);
        QString timeStr = QString("位置: %1").arg(position);
        ui->labelTime->setText(timeStr);
    }
}

void OnlineMenu::updateOnlineUserCount(int count)
{
    QString hintText = QString("在线玩家: %1人 | 闪电模式 | 3分钟对战").arg(count);
    ui->labelHint->setText(hintText);
}

/* 网络事件处理函数 */
void OnlineMenu::onMatchResponse(bool success, const QString &message, int queuePosition)
{
    if (success) {
        updateMatchStatus(message);
        if (queuePosition > 0) {
            updateQueuePosition(queuePosition);
        }
    } else {
        updateMatchStatus("匹配失败: " + message);
        m_isMatching = false;
        ui->btnStartMatch->setVisible(true);
        ui->btnCancelMatch->setVisible(false);
        m_matchTimer->stop();
    }
}

void OnlineMenu::onMatchQueued(int queuePosition)
{
    updateQueuePosition(queuePosition);
}

// online_menu.cpp - 修复onMatchFound函数
void OnlineMenu::onMatchFound(const QString &player1, const QString &player2)
{
    qDebug() << "匹配成功详情: player1=" << player1 << "player2=" << player2;

    NetworkManager *networkManager = NetworkManager::instance();
    if (!networkManager) {
        qDebug() << "网络管理器不可用";
        return;
    }

    QString myUsername = networkManager->getUsername();
    qDebug() << "我的用户名:" << myUsername;

    if (myUsername.isEmpty()) {
        qDebug() << "用户名获取失败";
        updateMatchStatus("匹配错误: 用户未登录");
        return;
    }

    QString opponentName;
    bool isMatched = false;

    if (player1 == myUsername) {
        opponentName = player2;
        isMatched = true;
    } else if (player2 == myUsername) {
        opponentName = player1;
        isMatched = true;
    }

    if (!isMatched) {
        qDebug() << "匹配成功但用户不在匹配中";
        updateMatchStatus("匹配错误: 用户不匹配");
        return;
    }

    // 【重要】停止计时器，更新UI状态
    m_matchTimer->stop();
    m_isMatching = false;

    ui->btnStartMatch->setVisible(true);
    ui->btnCancelMatch->setVisible(false);

    updateMatchStatus(QString("匹配成功! 对手: %1").arg(opponentName));

    // 【关键修改】不发射matchRequested信号，而是等待服务器发送游戏开始消息
    // 服务器应该发送game_start消息来触发游戏开始

    qDebug() << "匹配完成，等待游戏开始...";
}

void OnlineMenu::onMatchCancelled()
{
    updateMatchStatus("匹配已取消");
    m_isMatching = false;
    ui->btnStartMatch->setVisible(true);
    ui->btnCancelMatch->setVisible(false);
    m_matchTimer->stop();
}

void OnlineMenu::onOnlineListUpdated(const QJsonArray &users)
{
    updateOnlineUserCount(users.size());
}
