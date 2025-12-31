#ifndef MUSICSETTING_H
#define MUSICSETTING_H

#include <QWidget>
#include <QCheckBox>
#include <QVBoxLayout>
#include "musicmanager.h"

namespace Ui {
class MusicSetting;
}

class MusicSetting : public QWidget
{
    Q_OBJECT

public:
    explicit MusicSetting(QWidget *parent = nullptr);
    ~MusicSetting();

private slots:
    void onMusicToggled(bool checked);

private:
    Ui::MusicSetting *ui;
    QCheckBox* m_musicCheckBox;
};

#endif // MUSICSETTING_H
