#include "rankpage.h"
#include "ui_rankpage.h"
#include <QHeaderView>
#include <QPushButton>
#include <algorithm>
#include <QDebug>
#include <QStyle>
#include <QFont>
#include <QGraphicsOpacityEffect>
// 别忘了引入滚动条头文件
#include <QScrollBar>

RankPage::RankPage(const QString& username, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RankPage),
    m_rankManager(new RankManager(this)),
    m_username(username)
{
    ui->setupUi(this);

    // ============================================================
    // 1. 背景设置 (保留你原本满意的深紫渐变)
    // ============================================================
    this->setAutoFillBackground(true);
    this->setAttribute(Qt::WA_StyledBackground, true);

    this->setStyleSheet(
        "RankPage {"
        "   background: qradialgradient(cx:0.5, cy:0.5, radius:1.0, "
        "   fx:0.5, fy:0.5, "
        "   stop:0 #1a0b2e, "   // 中心深紫
        "   stop:0.6 #110520, "
        "   stop:1 #000000); "  // 边缘黑
        "}"
        );

    // 标题样式
    ui->titleLabel->setStyleSheet(
        "QLabel {"
        "   color: #ff9de0;"
        "   font: bold 28pt 'Microsoft YaHei';"
        "   background: transparent;"
        "   padding-bottom: 10px;"
        "}"
        );

    // ============================================================
    // 2. TabWidget (保留之前的青色边框风格)
    // ============================================================
    ui->tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "    border: 2px solid #00e5ff;" // 之前的青色边框
        "    border-radius: 8px;"
        "    background: transparent;"   // 保持透明
        "}"
        "QTabBar::tab {"
        "    background: #0f1c2e;"
        "    color: #888888;"
        "    padding: 10px 25px;"
        "    margin-right: 4px;"
        "    border-top-left-radius: 6px;"
        "    border-top-right-radius: 6px;"
        "    font: bold 11pt 'Microsoft YaHei';"
        "}"
        "QTabBar::tab:selected {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #00e5ff, stop:1 #162640);"
        "    color: white;"
        "    border: 1px solid #00e5ff;"
        "    border-bottom: none;"
        "}"
        "QTabBar::tab:hover {"
        "    background: #1e3a5f;"
        "    color: white;"
        "}"
        );

    // ============================================================
    // 【修改点 1】返回按钮：去掉原本的红色，改为深紫色胶囊风格
    // ============================================================
    ui->backButton->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4a148c, stop:1 #7b1fa2);" // 紫色渐变
        "    color: white;"
        "    border: 1px solid #7b1fa2;"
        "    border-radius: 20px;"
        "    font: bold 12pt 'Microsoft YaHei';"
        "    padding: 8px 30px;"
        "}"
        "QPushButton:hover {"
        "    background: #8e24aa;"
        "    border: 1px solid #ff9de0;" // 悬停变粉框
        "}"
        "QPushButton:pressed {"
        "    background: #4a148c;"
        "    padding-top: 5px;"
        "}"
        );

    // 初始化表格
    initTable(ui->mode1Table, 4);
    initTable(ui->mode2Table, 4);
    initTable(ui->mode3Table, 4);
    initTable(ui->personalTable, 4);

    setTableHeaders();
    refreshRankings();

    connect(ui->backButton, &QPushButton::clicked, this, &RankPage::on_backButton_clicked);
}

RankPage::~RankPage()
{
    delete ui;
}

void RankPage::on_backButton_clicked()
{
    emit backToMenu();
}

void RankPage::refreshRankings()
{
    fillModeTable(ui->mode1Table, m_rankManager->getTopRecords(10, "闪电模式"));
    fillModeTable(ui->mode2Table, m_rankManager->getTopRecords(10, "旋风模式"));
    fillModeTable(ui->mode3Table, m_rankManager->getTopRecords(10, "霓虹模式"));

    QVector<RankInfo> personalRecords = m_rankManager->getUserRecords(m_username, 20);
    std::sort(personalRecords.begin(), personalRecords.end(),
              [](const RankInfo& a, const RankInfo& b) {
                  return a.time > b.time;
              });
    fillPersonalTable(personalRecords);
}

void RankPage::setTableHeaders()
{
    QStringList commonHeaders; commonHeaders << "排名" << "玩家" << "得分" << "记录时间";
    ui->mode1Table->setHorizontalHeaderLabels(commonHeaders);
    ui->mode2Table->setHorizontalHeaderLabels(commonHeaders);
    ui->mode3Table->setHorizontalHeaderLabels(commonHeaders);

    QStringList personalHeaders; personalHeaders << "玩家" << "得分" << "游戏模式" << "记录时间";
    ui->personalTable->setHorizontalHeaderLabels(personalHeaders);
}

void RankPage::initTable(QTableWidget* table, int columnCount)
{
    table->setColumnCount(columnCount);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::NoFocus);

    table->setStyleSheet(
        "QTableWidget {"
        "   background-color: transparent;"
        "   border: none;"
        "   color: white;"
        "   font-size: 11pt;"
        "}"
        "QTableWidget::item {"
        "   padding: 5px;"
        "   border-bottom: 1px solid rgba(255, 255, 255, 30);"
        "}"
        // ============================================================
        // 【修改点 2】表头颜色加深：之前是 20 的透明度太浅，改为 200 的黑底
        // ============================================================
        "QHeaderView::section {"
        "   background-color: rgba(0, 0, 0, 200);" // 变深
        "   color: #00e5ff;" // 青色文字
        "   padding: 8px;"
        "   border: none;"
        "   border-bottom: 2px solid #00e5ff;"
        "   font: bold 12pt 'Microsoft YaHei';"
        "}"
        // ============================================================
        // 【修改点 3】滚动条美化 (解决丑的问题)
        // ============================================================
        "QScrollBar:vertical {"
        "   background: rgba(0,0,0,50);"
        "   width: 8px;"
        "   margin: 0px;"
        "   border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(255, 255, 255, 0.2);"
        "   min-height: 20px;"
        "   border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #00e5ff; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        );
}

// 下面的 fillModeTable 和 fillPersonalTable 保持原样，无需改动
void RankPage::fillModeTable(QTableWidget* table, const QVector<RankInfo>& records)
{
    table->setRowCount(records.size());

    for (int i = 0; i < records.size(); ++i) {
        const RankInfo& info = records[i];

        QTableWidgetItem* rankItem = new QTableWidgetItem(QString::number(info.rank));
        QTableWidgetItem* userItem = new QTableWidgetItem(info.username);
        QTableWidgetItem* scoreItem = new QTableWidgetItem(QString::number(info.score));
        QTableWidgetItem* timeItem = new QTableWidgetItem(info.time.toString("MM-dd HH:mm"));

        rankItem->setTextAlignment(Qt::AlignCenter);
        userItem->setTextAlignment(Qt::AlignCenter);
        scoreItem->setTextAlignment(Qt::AlignCenter);
        timeItem->setTextAlignment(Qt::AlignCenter);

        QColor textColor = Qt::white;
        QFont font("Microsoft YaHei", 10);

        if (info.rank == 1) {
            textColor = QColor("#FFD700");
            font.setBold(true); font.setPointSize(12);
            rankItem->setText("🏆 " + rankItem->text());
        } else if (info.rank == 2) {
            textColor = QColor("#E0E0E0");
            font.setBold(true);
            rankItem->setText("🥈 " + rankItem->text());
        } else if (info.rank == 3) {
            textColor = QColor("#CD7F32");
            font.setBold(true);
            rankItem->setText("🥉 " + rankItem->text());
        }

        rankItem->setForeground(textColor); rankItem->setFont(font);
        userItem->setForeground(textColor); userItem->setFont(font);
        scoreItem->setForeground(textColor); scoreItem->setFont(font);
        timeItem->setForeground(QColor("#aaaaaa"));

        table->setItem(i, 0, rankItem);
        table->setItem(i, 1, userItem);
        table->setItem(i, 2, scoreItem);
        table->setItem(i, 3, timeItem);
    }
}

void RankPage::fillPersonalTable(const QVector<RankInfo>& records)
{
    ui->personalTable->setRowCount(records.size());

    for (int i = 0; i < records.size(); ++i) {
        const RankInfo& info = records[i];

        QTableWidgetItem* userItem = new QTableWidgetItem(info.username);
        QTableWidgetItem* scoreItem = new QTableWidgetItem(QString::number(info.score));
        QTableWidgetItem* modeItem = new QTableWidgetItem(info.mode);
        QTableWidgetItem* timeItem = new QTableWidgetItem(info.time.toString("yyyy-MM-dd HH:mm"));

        userItem->setTextAlignment(Qt::AlignCenter);
        scoreItem->setTextAlignment(Qt::AlignCenter);
        modeItem->setTextAlignment(Qt::AlignCenter);
        timeItem->setTextAlignment(Qt::AlignCenter);

        QColor textColor = Qt::white;
        QColor modeColor = Qt::white;

        if (info.mode == "闪电模式") modeColor = QColor("#ff9de0");
        else if (info.mode == "旋风模式") modeColor = QColor("#00e5ff");
        else if (info.mode == "霓虹模式") modeColor = QColor("#7cffcb");

        userItem->setForeground(textColor);
        scoreItem->setForeground(QColor("#FFD700"));
        modeItem->setForeground(modeColor);
        timeItem->setForeground(QColor("#aaaaaa"));

        if (info.username == m_username) {
            QColor highlightBg = QColor(255, 255, 255, 20);
            userItem->setBackground(highlightBg);
            scoreItem->setBackground(highlightBg);
            modeItem->setBackground(highlightBg);
            timeItem->setBackground(highlightBg);
        }

        ui->personalTable->setItem(i, 0, userItem);
        ui->personalTable->setItem(i, 1, scoreItem);
        ui->personalTable->setItem(i, 2, modeItem);
        ui->personalTable->setItem(i, 3, timeItem);
    }
}
