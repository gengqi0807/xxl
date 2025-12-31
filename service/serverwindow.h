#ifndef SERVERWINDOW_H
#define SERVERWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QSystemTrayIcon>

QT_BEGIN_NAMESPACE
namespace Ui {
class ServerWindow;
}
QT_END_NAMESPACE

class ServerCore;
class OnlineManager;

class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ServerWindow(QWidget *parent = nullptr);
    ~ServerWindow();

private slots:
    void onStartServer();
    void onStopServer();
    void onClearLog();
    void onExportLog();

    void updateStats();
    void refreshOnlineList();

    void onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onMessageReceived(const QString &from, const QString &type, const QJsonObject &data);
    void onUserLoggedIn(const QString &username, const QString &ipAddress);
    void onClientDisconnected(const QString &username);
    void showServerInfo();

private:
    void setupUI();
    void setupSystemTray();
    void loadStyles();

    void addLog(const QString &message, const QColor &color = Qt::white);
    void updateStatusBar();

    Ui::ServerWindow *ui;
    ServerCore *m_serverCore;
    OnlineManager *m_onlineManager;
    QTimer *m_statsTimer;
    QSystemTrayIcon *m_trayIcon;

    int m_totalConnections;
    int m_activeMatches;
    int m_totalMatches;
};

#endif // SERVERWINDOW_H
