// skilltreepage.cpp
#include "skilltreepage.h"
#include "ui_skilltreepage.h"
#include <QPainter>
#include <QStyleOption>
#include <QMessageBox>
#include <QMouseEvent>

              void SkillTreePage::SkillTreeContainer::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 清空之前的矩形记录
    skillRects.clear();
    equipRects.clear();
    unequipRects.clear();

    // 绘制技能树连线
    painter.setPen(QPen(QColor(255, 157, 224, 180), 4));

    // 绘制所有父子关系的连线
    for (auto it = skillStates.begin(); it != skillStates.end(); ++it) {
        SkillNode* childSkill = it.value();

        if (!childSkill->parentId.isEmpty() && skillStates.contains(childSkill->parentId)) {
            // 计算父节点和子节点的位置
            SkillNode* parentSkill = skillStates[childSkill->parentId];

            // 根据技能ID确定位置（这里使用固定布局，但可以根据需要调整）
            int parentX = 0, parentY = 0, childX = 0, childY = 0;

            // 定义技能位置（根据原来的布局）
            if (parentSkill->id == "row_clear") { parentX = 300; parentY = 50; }
            else if (parentSkill->id == "time_extend") { parentX = 150; parentY = 150; }
            else if (parentSkill->id == "rainbow_bomb") { parentX = 450; parentY = 150; }
            else if (parentSkill->id == "cross_clear") { parentX = 300; parentY = 250; }
            else if (parentSkill->id == "score_double") { parentX = 150; parentY = 250; }
            else if (parentSkill->id == "color_unify") { parentX = 450; parentY = 250; }
            else if (parentSkill->id == "time_freeze") { parentX = 50; parentY = 250; }
            else if (parentSkill->id == "ultimate_burst") { parentX = 450; parentY = 350; }

            if (childSkill->id == "time_extend") { childX = 150; childY = 150; }
            else if (childSkill->id == "rainbow_bomb") { childX = 450; childY = 150; }
            else if (childSkill->id == "cross_clear") { childX = 300; childY = 250; }
            else if (childSkill->id == "score_double") { childX = 150; childY = 250; }
            else if (childSkill->id == "color_unify") { childX = 450; childY = 250; }
            else if (childSkill->id == "time_freeze") { childX = 50; childY = 250; }
            else if (childSkill->id == "ultimate_burst") { childX = 450; childY = 350; }

            // 绘制连线
            painter.drawLine(parentX + 40, parentY + 15, childX + 40, childY + 15);
        }
    }

    // 绘制所有技能节点
    for (auto it = skillStates.begin(); it != skillStates.end(); ++it) {
        QString skillId = it.key();
        SkillNode* skill = it.value();

        // 根据技能ID确定位置
        int x = 0, y = 0;

        if (skillId == "row_clear") { x = 300; y = 50; }
        else if (skillId == "time_extend") { x = 150; y = 150; }
        else if (skillId == "rainbow_bomb") { x = 450; y = 150; }
        else if (skillId == "cross_clear") { x = 300; y = 250; }
        else if (skillId == "score_double") { x = 150; y = 250; }
        else if (skillId == "color_unify") { x = 450; y = 250; }
        else if (skillId == "time_freeze") { x = 50; y = 250; }
        else if (skillId == "ultimate_burst") { x = 450; y = 350; }

        drawSkillNode(painter, skillId, x, y);
    }
}

void SkillTreePage::SkillTreeContainer::drawSkillNode(QPainter& painter, const QString& skillId, int x, int y) {
    if (!skillStates.contains(skillId)) return;

    SkillNode* skill = skillStates[skillId];

    // 绘制技能按钮
    QRect skillRect(x, y, 80, 30);
    skillRects[skillId] = skillRect;

    // 设置按钮颜色
    QColor buttonColor;
    if (skill->equipped) {
        buttonColor = QColor(76, 175, 80);  // 已装备 - 绿色
    } else if (skill->unlocked) {
        buttonColor = QColor(255, 157, 224);  // 已解锁 - 粉色
    } else if (skillTree->getSkillPoints() >= skill->cost &&
               skillTree->checkPrerequisites(skillId)) {
        buttonColor = QColor(33, 150, 243);  // 可解锁 - 蓝色
    } else {
        buttonColor = QColor(117, 117, 117);  // 不可解锁 - 灰色
    }

    // 绘制圆角矩形
    painter.setPen(Qt::NoPen);
    painter.setBrush(buttonColor);
    painter.drawRoundedRect(skillRect, 8, 8);

    // 绘制文字
    painter.setPen(QColor(255, 255, 255));
    painter.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    painter.drawText(skillRect, Qt::AlignCenter, skill->name);

    // 绘制消耗
    painter.setPen(QColor(255, 235, 59));
    painter.setFont(QFont("Microsoft YaHei", 8));
    painter.drawText(x, y + 35, 80, 20, Qt::AlignCenter,
                     QString("消耗: %1").arg(skill->cost));

    // 绘制装备/卸下按钮（如果已解锁）
    if (skill->unlocked) {
        if (!skill->equipped) {
            // 绘制装备按钮
            QRect equipRect(x, y + 60, 35, 20);
            equipRects[skillId] = equipRect;

            painter.setBrush(QColor(76, 175, 80));  // 绿色
            painter.drawRoundedRect(equipRect, 4, 4);

            painter.setPen(QColor(255, 255, 255));
            painter.setFont(QFont("Microsoft YaHei", 8));
            painter.drawText(equipRect, Qt::AlignCenter, "装");
        } else {
            // 绘制卸下按钮
            QRect unequipRect(x + 45, y + 60, 35, 20);
            unequipRects[skillId] = unequipRect;

            painter.setBrush(QColor(244, 67, 54));  // 红色
            painter.drawRoundedRect(unequipRect, 4, 4);

            painter.setPen(QColor(255, 255, 255));
            painter.setFont(QFont("Microsoft YaHei", 8));
            painter.drawText(unequipRect, Qt::AlignCenter, "卸");
        }
    }
}

void SkillTreePage::SkillTreeContainer::mousePressEvent(QMouseEvent* event) {
    QWidget::mousePressEvent(event);

    QPoint pos = event->pos();

    // 检查是否点击了技能按钮
    QString clickedSkillId = getSkillAtPosition(pos);
    if (!clickedSkillId.isEmpty()) {
        page->handleSkillClick(clickedSkillId);
        return;
    }

    // 检查是否点击了装备按钮
    QString equipSkillId = getEquipButtonAtPosition(pos);
    if (!equipSkillId.isEmpty()) {
        page->handleEquipClick(equipSkillId);
        return;
    }

    // 检查是否点击了卸下按钮
    QString unequipSkillId = getUnequipButtonAtPosition(pos);
    if (!unequipSkillId.isEmpty()) {
        page->handleUnequipClick(unequipSkillId);
        return;
    }
}

QString SkillTreePage::SkillTreeContainer::getSkillAtPosition(const QPoint& pos) {
    for (auto it = skillRects.begin(); it != skillRects.end(); ++it) {
        if (it.value().contains(pos)) {
            return it.key();
        }
    }
    return QString();
}

QString SkillTreePage::SkillTreeContainer::getEquipButtonAtPosition(const QPoint& pos) {
    for (auto it = equipRects.begin(); it != equipRects.end(); ++it) {
        if (it.value().contains(pos)) {
            return it.key();
        }
    }
    return QString();
}

QString SkillTreePage::SkillTreeContainer::getUnequipButtonAtPosition(const QPoint& pos) {
    for (auto it = unequipRects.begin(); it != unequipRects.end(); ++it) {
        if (it.value().contains(pos)) {
            return it.key();
        }
    }
    return QString();
}

SkillTreePage::SkillTreePage(SkillTree* skillTree, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SkillTreePage),
    skillTree(skillTree),
    skillTreeContainer(nullptr)
{
    ui->setupUi(this);

    connect(ui->btnBack, &QPushButton::clicked, this, &SkillTreePage::onBackButtonClicked);

    setupSkillTreeUI();
    refreshUI();
}

SkillTreePage::~SkillTreePage() {
    delete ui;
}

void SkillTreePage::setupSkillTreeUI() {
    // 清空现有UI元素
    QLayoutItem* item;
    while ((item = ui->skillTreeLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // 创建技能树容器
    skillTreeContainer = new SkillTreeContainer(skillTree, this);
    skillTreeContainer->setObjectName("skillTreeContainer");
    skillTreeContainer->setStyleSheet("background:transparent;");

    // 设置容器的最小大小
    skillTreeContainer->setMinimumSize(600, 500);

    ui->skillTreeLayout->addWidget(skillTreeContainer);
}

void SkillTreePage::refreshUI() {
    // 更新技能点显示
    ui->labelSkillPoints->setText(QString("技能点: %1").arg(skillTree->getSkillPoints()));

    // 获取所有技能状态
    QMap<QString, SkillNode*> skillStates;
    const QMap<QString, SkillNode*>& allSkills = skillTree->getAllSkills();
    for (auto it = allSkills.begin(); it != allSkills.end(); ++it) {
        skillStates[it.key()] = it.value();
    }

    // 更新容器绘制
    if (skillTreeContainer) {
        skillTreeContainer->setSkillStates(skillStates);
        skillTreeContainer->update();
    }
}

void SkillTreePage::onSkillButtonClicked() {
    // 这个函数现在通过handleSkillClick处理
}

void SkillTreePage::onEquipButtonClicked() {
    // 这个函数现在通过handleEquipClick处理
}

void SkillTreePage::onUnequipButtonClicked() {
    // 这个函数现在通过handleUnequipClick处理
}

void SkillTreePage::handleSkillClick(const QString& skillId) {
    // 尝试解锁技能
    if (skillTree->unlockSkill(skillId)) {
        QMessageBox::information(this, "成功", QString("已解锁技能: %1").arg(skillTree->getSkill(skillId)->name));
        refreshUI();
    } else {
        SkillNode* skill = skillTree->getSkill(skillId);
        if (skill && !skill->unlocked) {
            QString message = QString("无法解锁技能:\n");
            if (skillTree->getSkillPoints() < skill->cost) {
                message += QString("技能点不足 (需要: %1, 当前: %2)\n").arg(skill->cost).arg(skillTree->getSkillPoints());
            }
            if (!skillTree->checkPrerequisites(skillId)) {
                message += "前置技能未解锁\n";
            }
            QMessageBox::warning(this, "解锁失败", message);
        }
    }
}

void SkillTreePage::handleEquipClick(const QString& skillId) {
    if (skillTree->equipSkill(skillId)) {
        refreshUI();
    } else {
        // 检查是否已装备满3个技能
        QList<SkillNode*> equipped = skillTree->getEquippedSkills();
        if (equipped.size() >= 3) {
            QMessageBox::warning(this, "装备失败", "最多只能装备3个技能！");
        } else {
            QMessageBox::warning(this, "装备失败", "技能未解锁！");
        }
    }
}

void SkillTreePage::handleUnequipClick(const QString& skillId) {
    if (skillTree->unequipSkill(skillId)) {
        refreshUI();
    }
}

void SkillTreePage::onBackButtonClicked() {
    // 保存技能状态到数据库
    emit backRequested();
}
