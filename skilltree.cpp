// skilltree.cpp
#include "skilltree.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

SkillTree::SkillTree(QObject *parent) : QObject(parent) {
    initialize();
}

void SkillTree::initialize() {
    // 清空现有技能
    qDeleteAll(skills);
    skills.clear();

    // 按您的要求创建技能树
    // 根节点
    skills["row_clear"] = new SkillNode("row_clear", "一行消除",
                                        "消除方块所在行的所有方块", 2, "");

    // 第一层子节点
    skills["time_extend"] = new SkillNode("time_extend", "时间延长",
                                          "暂停倒计时5秒", 4, "row_clear");
    skills["rainbow_bomb"] = new SkillNode("rainbow_bomb", "彩虹炸弹",
                                           "点击位置消除所有与其相同的方块", 4, "row_clear");

    // 第二层子节点
    skills["cross_clear"] = new SkillNode("cross_clear", "十字消除",
                                          "以点击处为中心，消除十字方向所有方块", 6, "rainbow_bomb");
    skills["score_double"] = new SkillNode("score_double", "得分翻倍",
                                           "8秒内得分翻倍", 6, "time_extend");
    skills["color_unify"] = new SkillNode("color_unify", "颜色统一",
                                          "将所有方块转为3种颜色，持续6秒", 8, "rainbow_bomb");
    skills["time_freeze"] = new SkillNode("time_freeze", "时间冻结",
                                          "暂停倒计时15秒", 8, "time_extend");

    // 第三层子节点（终极技能）
    skills["ultimate_burst"] = new SkillNode("ultimate_burst", "终极爆发",
                                             "消除屏幕内所有方块，获得3200加分", 10, "color_unify");
}

bool SkillTree::unlockSkill(const QString& skillId) {
    if (!skills.contains(skillId)) return false;

    SkillNode* skill = skills[skillId];

    // 检查是否已解锁
    if (skill->unlocked) return true;

    // 检查前置条件
    if (!checkPrerequisites(skillId)) {
        qDebug() << "Prerequisites not met for skill:" << skillId;
        return false;
    }

    // 检查技能点是否足够
    if (skillPoints < skill->cost) {
        qDebug() << "Not enough skill points. Required:" << skill->cost << "Available:" << skillPoints;
        return false;
    }

    // 解锁技能
    skill->unlocked = true;
    skillPoints -= skill->cost;

    emit skillUnlocked(skillId);
    emit skillPointsChanged(skillPoints);

    return true;
}

bool SkillTree::checkPrerequisites(const QString& skillId) {
    if (!skills.contains(skillId)) return false;

    SkillNode* skill = skills[skillId];

    // 如果没有父节点，可以直接解锁
    if (skill->parentId.isEmpty()) return true;

    // 检查父节点是否已解锁
    if (!skills.contains(skill->parentId)) return false;
    if (!skills[skill->parentId]->unlocked) return false;

    return true;
}

bool SkillTree::equipSkill(const QString& skillId) {
    if (!skills.contains(skillId)) return false;

    SkillNode* skill = skills[skillId];

    // 检查是否已解锁
    if (!skill->unlocked) return false;

    // 检查是否已装备
    if (skill->equipped) return true;

    // 检查装备数量限制（最多3个）
    if (equippedSkills.size() >= 3) {
        qDebug() << "Cannot equip more than 3 skills";
        return false;
    }

    // 装备技能
    skill->equipped = true;
    equippedSkills.insert(skillId);

    emit skillEquipped(skillId);
    return true;
}

bool SkillTree::unequipSkill(const QString& skillId) {
    if (!skills.contains(skillId)) return false;

    SkillNode* skill = skills[skillId];

    // 检查是否已装备
    if (!skill->equipped) return true;

    // 卸下技能
    skill->equipped = false;
    equippedSkills.remove(skillId);

    emit skillUnequipped(skillId);
    return true;
}

SkillNode* SkillTree::getSkill(const QString& skillId) {
    return skills.value(skillId, nullptr);
}

QList<SkillNode*> SkillTree::getEquippedSkills() const {
    QList<SkillNode*> equipped;
    for (const QString& skillId : equippedSkills) {
        if (skills.contains(skillId)) {
            equipped.append(skills[skillId]);
        }
    }
    return equipped;
}

void SkillTree::addSkillPoints(int points) {
    if (points <= 0) return;

    skillPoints += points;
    emit skillPointsChanged(skillPoints);
}

void SkillTree::saveToDatabase(const QString& username) {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return;

    QSqlQuery q(db);

    // 先删除旧记录
    QString deleteSql = QString("DELETE FROM skill_tree WHERE username = ?");
    q.prepare(deleteSql);
    q.addBindValue(username);
    q.exec();

    // 保存技能状态
    QString insertSql = QString(
        "INSERT INTO skill_tree (username, skill_id, unlocked, equipped) "
        "VALUES (?, ?, ?, ?)");

    for (SkillNode* skill : skills) {
        q.prepare(insertSql);
        q.addBindValue(username);
        q.addBindValue(skill->id);
        q.addBindValue(skill->unlocked ? 1 : 0);
        q.addBindValue(skill->equipped ? 1 : 0);
        q.exec();
    }

    // 保存技能点
    QString updateSql = QString(
        "UPDATE user SET skill_points = ? WHERE username = ?");
    q.prepare(updateSql);
    q.addBindValue(skillPoints);
    q.addBindValue(username);
    q.exec();
}

void SkillTree::loadFromDatabase(const QString& username) {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return;

    // 重置所有技能状态
    for (SkillNode* skill : skills) {
        skill->unlocked = false;
        skill->equipped = false;
    }
    equippedSkills.clear();

    // 加载技能状态
    QSqlQuery q(db);
    QString selectSql = QString(
        "SELECT skill_id, unlocked, equipped FROM skill_tree WHERE username = ?");
    q.prepare(selectSql);
    q.addBindValue(username);

    if (q.exec()) {
        while (q.next()) {
            QString skillId = q.value(0).toString();
            bool unlocked = q.value(1).toBool();
            bool equipped = q.value(2).toBool();

            if (skills.contains(skillId)) {
                SkillNode* skill = skills[skillId];
                skill->unlocked = unlocked;
                if (equipped) {
                    skill->equipped = true;
                    equippedSkills.insert(skillId);
                }
            }
        }
    }

    // 加载技能点
    QString pointsSql = QString(
        "SELECT skill_points FROM user WHERE username = ?");
    q.prepare(pointsSql);
    q.addBindValue(username);

    if (q.exec() && q.next()) {
        skillPoints = q.value(0).toInt();
    }

    emit skillPointsChanged(skillPoints);
}
