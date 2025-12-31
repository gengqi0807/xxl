#include "musicmanager.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QApplication>

MusicManager& MusicManager::instance()
{
    static MusicManager instance;
    return instance;
}

MusicManager::MusicManager(QObject* parent)
    : QObject(parent)
{
    //加载本地音乐
    loadSettings();
}

//获取音乐文件路径
QString MusicManager::getMusicFilePath(const QString& fileName) const
{
    //检查文件名是否正确
    if (fileName.isEmpty()) {
        qDebug() << "音乐文件名为空";
        return "";
    }

    //首先检查当前目录下的build文件夹
    QDir currentDir = QDir::current();
    if (currentDir.dirName() == "build") {
        //如果已经在build目录中
        QString buildPath = currentDir.absoluteFilePath(fileName);
        if (checkFileExists(buildPath)) {
            qDebug() << "找到音乐文件（当前build目录）:" << buildPath;
            return buildPath;
        }
    }

    if (currentDir.cd("build")) {
        QString buildPath = currentDir.absoluteFilePath(fileName);
        if (checkFileExists(buildPath)) {
            qDebug() << "找到音乐文件（当前目录下的build）:" << buildPath;
            return buildPath;
        }
        currentDir.cdUp();
    }

    //检查应用程序目录
    QString appDirPath = QCoreApplication::applicationDirPath();
    QDir appDir(appDirPath);

    if (appDir.dirName() == "build") {
        QString appPath = appDir.absoluteFilePath(fileName);
        if (checkFileExists(appPath)) {
            qDebug() << "找到音乐文件（应用程序build目录）:" << appPath;
            return appPath;
        }
    }

    //检查应用程序目录下的build文件夹
    if (appDir.cd("build")) {
        QString buildPath = appDir.absoluteFilePath(fileName);
        if (checkFileExists(buildPath)) {
            qDebug() << "找到音乐文件（应用程序下的build）:" << buildPath;
            return buildPath;
        }
        appDir.cdUp();
    }

    //尝试向上查找项目根目录下的build
    QString projectBuildPath = "";
    QDir projectDir = QDir::current();

    //向上查找最多3层目录
    for (int i = 0; i < 3; ++i) {
        if (projectDir.cd("build")) {
            QString tryPath = projectDir.absoluteFilePath(fileName);
            if (checkFileExists(tryPath)) {
                qDebug() << "找到音乐文件（项目build目录）:" << tryPath;
                return tryPath;
            }
            projectDir.cdUp();
        }

        //检查当前目录是否有build文件夹
        QDir tempDir = projectDir;
        if (tempDir.cd("build")) {
            QString tryPath = tempDir.absoluteFilePath(fileName);
            if (checkFileExists(tryPath)) {
                qDebug() << "找到音乐文件（上级build目录）:" << tryPath;
                return tryPath;
            }
        }

        if (!projectDir.cdUp()) {
            break;
        }
    }

    QString absolutePath = "F:/TEAM/game000/game_xxl-master/build/" + fileName;
    if (checkFileExists(absolutePath)) {
        qDebug() << "找到音乐文件（小组绝对路径）:" << absolutePath;
        return absolutePath;
    }

    //如果都没找到，输出调试信息
    qWarning() << "无法找到音乐文件：" << fileName;
    qWarning() << "当前目录：" << QDir::currentPath();
    qWarning() << "应用程序目录：" << QCoreApplication::applicationDirPath();

    return "";
}

// 检查文件是否存在
bool MusicManager::checkFileExists(const QString& path) const
{
    QFileInfo fileInfo(path);
    bool exists = fileInfo.exists() && fileInfo.isFile();
    if (!exists) {
        qDebug() << "文件不存在：" << path;
    }
    return exists;
}

// 播放循环背景音乐
void MusicManager::playBackgroundSound(const QString& filePath)
{
    QMutexLocker locker(&m_soundMutex);

    // 如果音乐已关闭，不播放
    if (!m_musicEnabled) {
        qDebug() << "音乐已关闭，跳过背景音乐播放";
        return;
    }

    // 如果文件路径为空，不播放
    if (filePath.isEmpty()) {
        qWarning() << "背景音乐文件路径为空";
        return;
    }

    // 检查文件是否存在
    if (!checkFileExists(filePath)) {
        qWarning() << "背景音乐文件不存在：" << filePath;
        return;
    }

    // 如果当前正在播放相同的音乐，不重复播放
    if (m_currentBackgroundPath == filePath) {
        qDebug() << "相同的背景音乐已在播放，跳过：" << filePath;
        return;
    }

    // 将文件路径转换为Windows API需要的格式
    std::wstring widePath = filePath.toStdWString();
    LPCWSTR soundPath = widePath.c_str();

    // 停止当前正在播放的音乐
    PlaySoundW(NULL, NULL, 0);

    // 播放背景音乐（循环播放）
    BOOL success = PlaySoundW(soundPath, NULL, SND_FILENAME | SND_LOOP | SND_ASYNC);
    if (!success) {
        DWORD error = GetLastError();
        qWarning() << "播放背景音乐失败，文件：" << filePath;
        qWarning() << "错误代码：" << error;

        // 错误信息翻译
        LPWSTR errorMsg = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            0,
            (LPWSTR)&errorMsg,
            0,
            NULL
            );

        if (errorMsg) {
            qWarning() << "错误信息：" << QString::fromWCharArray(errorMsg);
            LocalFree(errorMsg);
        }
    } else {
        m_currentBackgroundPath = filePath;
        qDebug() << "成功播放背景音乐：" << filePath;
    }
}

// 播放单次音效
void MusicManager::playOneShotSound(const QString& filePath)
{
    // 如果音乐已关闭，不播放
    if (!m_musicEnabled) {
        qDebug() << "音乐已关闭，跳过音效播放";
        return;
    }

    QMutexLocker locker(&m_soundMutex);

    if (filePath.isEmpty()) {
        qWarning() << "音效文件路径为空";
        return;
    }

    // 检查文件是否存在
    if (!checkFileExists(filePath)) {
        qWarning() << "音效文件不存在：" << filePath;
        return;
    }

    // 将文件路径转换为Windows API需要的格式
    std::wstring widePath = filePath.toStdWString();
    LPCWSTR soundPath = widePath.c_str();

    // 播放音效（单次播放）
    BOOL success = PlaySoundW(soundPath, NULL, SND_FILENAME | SND_ASYNC);
    if (!success) {
        DWORD error = GetLastError();
        qWarning() << "播放音效失败，文件：" << filePath;
        qWarning() << "错误代码：" << error;

        // 错误信息翻译
        LPWSTR errorMsg = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            0,
            (LPWSTR)&errorMsg,
            0,
            NULL
            );

        if (errorMsg) {
            qWarning() << "错误信息：" << QString::fromWCharArray(errorMsg);
            LocalFree(errorMsg);
        }
    } else {
        qDebug() << "成功播放音效：" << filePath;
    }
}

// 音乐总开关
void MusicManager::setMusicEnabled(bool enabled)
{
    if (m_musicEnabled == enabled) return;

    m_musicEnabled = enabled;
    emit musicEnabledChanged(enabled);

    if (!enabled) {
        // 关闭所有音乐
        QMutexLocker locker(&m_soundMutex);
        PlaySoundW(NULL, NULL, 0);
        m_currentBackgroundPath.clear();
        qDebug() << "音乐已关闭";
    } else {
        // 开启则重新播放当前场景音乐
        qDebug() << "音乐已开启，重新播放当前场景音乐";
        playSceneMusic(m_currentScene);
    }

    saveSettings();
}

bool MusicManager::isMusicEnabled() const
{
    return m_musicEnabled;
}

// 播放指定场景的背景音乐
void MusicManager::playSceneMusic(MusicScene scene)
{
    m_currentScene = scene;

    if (!m_musicEnabled) {
        qDebug() << "音乐已关闭，不播放场景音乐";
        return;
    }

    // 根据场景选择背景音乐文件
    QString fileName;
    switch (scene) {
    case MusicScene::Loading:
        fileName = "loadingmusic.wav";
        qDebug() << "播放登录界面音乐：" << fileName;
        break;
    case MusicScene::Menu:
        fileName = "menumusic.wav";
        qDebug() << "播放菜单界面音乐：" << fileName;
        break;
    case MusicScene::Playing:
        fileName = "playmusic.wav";
        qDebug() << "播放游戏过程音乐：" << fileName;
        break;
    default:
        return;
    }

    // 获取文件路径并播放
    QString musicPath = getMusicFilePath(fileName);
    if (!musicPath.isEmpty()) {
        playBackgroundSound(musicPath);
    } else {
        qWarning() << "无法获取音乐文件路径：" << fileName;
    }
}

// 播放消除音效 - 这是关键修复部分
// 【修复】播放消除音效 - 修正匹配条件
void MusicManager::playMatchSound(int matchCount)
{
    if (!m_musicEnabled) {
        qDebug() << "音乐已关闭，跳过消除音效播放";
        return;
    }

    QString fileName;
    // 根据消除数量选择音效文件
    // 【修复】修正条件判断逻辑 - 确保四连播放正确音效
    if (matchCount == 3) {
        fileName = "good.wav";
        qDebug() << "播放三连消除音效：" << fileName;
    } else if (matchCount == 4) {
        fileName = "excellent.wav";
        qDebug() << "播放四连消除音效：" << fileName;
    } else if (matchCount >= 5) {
        fileName = "unbelievable.wav";
        qDebug() << "播放五连及以上消除音效：" << fileName;
    } else {
        qDebug() << "消除数量" << matchCount << "，不播放音效";
        return; // 少于3连不播放音效
    }

    // 获取文件路径并播放
    QString soundPath = getMusicFilePath(fileName);
    if (!soundPath.isEmpty()) {
        playOneShotSound(soundPath);
    } else {
        qWarning() << "无法获取音效文件路径：" << fileName;
        // 列出所有搜索过的路径
        qDebug() << "当前搜索目录：" << QDir::currentPath();
    }
}

// 停止所有音乐
void MusicManager::stopAll()
{
    QMutexLocker locker(&m_soundMutex);
    PlaySoundW(NULL, NULL, 0);
    m_currentBackgroundPath.clear();
    qDebug() << "停止所有音乐播放";
}

// 保存音乐设置到本地配置文件
void MusicManager::saveSettings()
{
    QSettings settings("GameMusic.ini", QSettings::IniFormat);
    settings.beginGroup("Music");
    settings.setValue("MusicEnabled", m_musicEnabled);
    settings.endGroup();
    qDebug() << "音乐设置已保存：音乐总开关 = " << m_musicEnabled;
}

// 加载本地音乐设置
void MusicManager::loadSettings()
{
    QSettings settings("GameMusic.ini", QSettings::IniFormat);
    settings.beginGroup("Music");
    m_musicEnabled = settings.value("MusicEnabled", true).toBool();
    settings.endGroup();

    qDebug() << "加载音乐设置：音乐总开关 = " << m_musicEnabled;
}
