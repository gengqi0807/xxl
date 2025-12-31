#ifndef MUSICMANAGER_H
#define MUSICMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>
#include <QMutex>

//Windows原生音频API所需头文件
#include <Windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "winmm.lib") //链接音频库

class MusicManager : public QObject
{
    Q_OBJECT
public:
    enum class MusicScene {
        Loading,   // 登录界面
        Menu,      // 菜单界面
        Playing    // 游戏界面
    };

    // 单例模式获取实例
    static MusicManager& instance();

    // 音乐总开关
    void setMusicEnabled(bool enabled);
    bool isMusicEnabled() const;

    // 播放指定场景的背景音乐（循环）
    void playSceneMusic(MusicScene scene);
    // 播放消除音效（参数为消除数量）
    void playMatchSound(int matchCount);
    // 停止所有音乐播放
    void stopAll();

signals:
    void musicEnabledChanged(bool enabled);

private:
    // 私有构造函数（单例模式）
    explicit MusicManager(QObject* parent = nullptr);
    // 禁用拷贝/赋值
    MusicManager(const MusicManager&) = delete;
    MusicManager& operator=(const MusicManager&) = delete;

    // 计算音乐文件路径
    QString getMusicFilePath(const QString& fileName) const;
    // 检查文件是否存在
    bool checkFileExists(const QString& path) const;

    // 底层播放函数
    void playBackgroundSound(const QString& filePath); // 循环播放
    void playOneShotSound(const QString& filePath);   // 单次播放

    // 保存/加载音乐设置
    void saveSettings();
    void loadSettings();

private:
    bool m_musicEnabled = true;  // 音乐总开关（默认开启）
    MusicScene m_currentScene = MusicScene::Loading; // 当前场景
    QString m_currentBackgroundPath; // 当前背景音乐路径
    QMutex m_soundMutex;              // 音频播放互斥锁
};

#endif // MUSICMANAGER_H
