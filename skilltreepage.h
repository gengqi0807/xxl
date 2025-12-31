// skilltreepage.h
#ifndef SKILLTREEPAGE_H
#define SKILLTREEPAGE_H

#include <QWidget>
#include <QMap>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include "skilltree.h"

              namespace Ui {
    class SkillTreePage;
}

class SkillTreePage : public QWidget {
    Q_OBJECT

public:
    explicit SkillTreePage(SkillTree* skillTree, QWidget *parent = nullptr);
    ~SkillTreePage();

    void refreshUI();

signals:
    void backRequested();

private slots:
    void onSkillButtonClicked();  // 保持无参数版本
    void onEquipButtonClicked();  // 保持无参数版本
    void onUnequipButtonClicked();  // 保持无参数版本
    void onBackButtonClicked();

private:
    class SkillTreeContainer : public QWidget {
    public:
        SkillTreeContainer(SkillTree* skillTree, SkillTreePage* page, QWidget* parent = nullptr)
            : QWidget(parent), skillTree(skillTree), page(page) {}

        void setSkillStates(const QMap<QString, SkillNode*>& states) {
            skillStates = states;
            update();
        }

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:
        SkillTree* skillTree;
        SkillTreePage* page;
        QMap<QString, SkillNode*> skillStates;
        QMap<QString, QRect> skillRects; // 存储技能按钮的位置和大小
        QMap<QString, QRect> equipRects; // 存储装备按钮的位置和大小
        QMap<QString, QRect> unequipRects; // 存储卸下按钮的位置和大小

        void drawSkillNode(QPainter& painter, const QString& skillId, int x, int y);
        QString getSkillAtPosition(const QPoint& pos);
        QString getEquipButtonAtPosition(const QPoint& pos);
        QString getUnequipButtonAtPosition(const QPoint& pos);
    };

    Ui::SkillTreePage *ui;
    SkillTree* skillTree;
    SkillTreeContainer* skillTreeContainer;

    void setupSkillTreeUI();

    // 添加处理点击的辅助函数
    void handleSkillClick(const QString& skillId);
    void handleEquipClick(const QString& skillId);
    void handleUnequipClick(const QString& skillId);
};

#endif // SKILLTREEPAGE_H
