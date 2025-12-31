// skilltree.h
#ifndef SKILLTREE_H
#define SKILLTREE_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>

class SkillNode {
public:
    QString id;             // 技能ID
    QString name;          // 技能名称
    QString description;   // 技能描述
    int cost;              // 解锁所需技能点
    QString parentId;      // 父节点ID

    bool unlocked = false; // 是否已解锁
    bool equipped = false; // 是否已装备
    bool used = false; //本局是否已使用

    SkillNode(const QString& id, const QString& name, const QString& desc, int cost,
              const QString& parentId = "")
        : id(id), name(name), description(desc), cost(cost), parentId(parentId) {}
};

class SkillTree : public QObject {
    Q_OBJECT

public:
    explicit SkillTree(QObject *parent = nullptr);

    // 初始化技能树
    void initialize();

    // 解锁技能
    bool unlockSkill(const QString& skillId);

    // 装备/卸下技能
    bool equipSkill(const QString& skillId);
    bool unequipSkill(const QString& skillId);

    // 获取技能
    SkillNode* getSkill(const QString& skillId);

    // 获取已装备的技能
    QList<SkillNode*> getEquippedSkills() const;

    // 技能点相关
    void addSkillPoints(int points);
    int getSkillPoints() const { return skillPoints; }

    // 获取所有技能
    const QMap<QString, SkillNode*>& getAllSkills() const { return skills; }

    // 保存/加载
    void saveToDatabase(const QString& username);
    void loadFromDatabase(const QString& username);

    // 检查前置条件是否满足
    bool checkPrerequisites(const QString& skillId);

signals:
    void skillUnlocked(const QString& skillId);
    void skillEquipped(const QString& skillId);
    void skillUnequipped(const QString& skillId);
    void skillPointsChanged(int points);

private:
    QMap<QString, SkillNode*> skills;  // 技能ID -> 技能节点
    QSet<QString> equippedSkills;      // 已装备的技能ID
    int skillPoints = 0;               // 当前技能点
};

#endif // SKILLTREE_H
