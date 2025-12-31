#include "musicsetting.h"
#include "ui_musicsetting.h"
#include "musicmanager.h"
#include <QDebug>

MusicSetting::MusicSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MusicSetting)
{
    ui->setupUi(this);

    //设置窗口样式
    this->setStyleSheet("background-color: #2b2b2b;");

    //设置窗口属性
    this->setWindowTitle("音乐设置");
    this->setFixedSize(300, 200);
    this->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);

    //获取UI中的复选框控件
    m_musicCheckBox = ui->musicCheckBox;

    //初始化复选框状态
    bool isEnabled = MusicManager::instance().isMusicEnabled();
    m_musicCheckBox->setChecked(isEnabled);
    qDebug() << "音乐设置界面初始化，音乐状态：" << (isEnabled ? "开启" : "关闭");

    //连接信号槽
    connect(m_musicCheckBox, &QCheckBox::toggled,
            this, &MusicSetting::onMusicToggled);

    //监听音乐设置变化
    connect(&MusicManager::instance(), &MusicManager::musicEnabledChanged,
            m_musicCheckBox, &QCheckBox::setChecked);
}

MusicSetting::~MusicSetting()
{
    delete ui;
}

void MusicSetting::onMusicToggled(bool checked)
{
    qDebug() << "音乐开关状态改变：" << (checked ? "开启" : "关闭");
    MusicManager::instance().setMusicEnabled(checked);
}
