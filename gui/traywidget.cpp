#include "./traywidget.h"
#include "./traymenu.h"
#include "./settingsdialog.h"
#include "./webviewdialog.h"
#include "./textviewdialog.h"

#include "../application/settings.h"

#include "resources/config.h"
#include "ui_traywidget.h"

#include <qtutilities/resources/qtconfigarguments.h>
#include <qtutilities/resources/resources.h>
#include <qtutilities/settingsdialog/qtsettings.h>
#include <qtutilities/aboutdialog/aboutdialog.h>
#include <qtutilities/misc/dialogutils.h>
#include <qtutilities/misc/desktoputils.h>

#include <c++utilities/conversion/stringconversion.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QClipboard>
#include <QDir>
#include <QTextBrowser>
#include <QStringBuilder>
#include <QFontDatabase>

#include <functional>

using namespace ApplicationUtilities;
using namespace ConversionUtilities;
using namespace ChronoUtilities;
using namespace Dialogs;
using namespace Data;
using namespace std;

namespace QtGui {

/*!
 * \brief Instantiates a new tray widget.
 */
TrayWidget::TrayWidget(TrayMenu *parent) :
    QWidget(parent),
    m_menu(parent),
    m_ui(new Ui::TrayWidget),
    m_settingsDlg(nullptr),
    m_aboutDlg(nullptr),
#ifndef SYNCTHINGTRAY_NO_WEBVIEW
    m_webViewDlg(nullptr),
#endif
    m_dirModel(m_connection),
    m_devModel(m_connection),
    m_dlModel(m_connection),
    m_selectedConnection(nullptr)
{
    m_ui->setupUi(this);

    // setup model and view
    m_ui->dirsTreeView->setModel(&m_dirModel);
    m_ui->devsTreeView->setModel(&m_devModel);
    m_ui->downloadsTreeView->setModel(&m_dlModel);

    // setup sync-all button
    m_cornerFrame = new QFrame(this);
    auto *cornerFrameLayout = new QHBoxLayout(m_cornerFrame);
    cornerFrameLayout->setSpacing(0), cornerFrameLayout->setMargin(0);
    //cornerFrameLayout->addStretch();
    m_cornerFrame->setLayout(cornerFrameLayout);
    auto *viewIdButton = new QPushButton(m_cornerFrame);
    viewIdButton->setToolTip(tr("View own device ID"));
    viewIdButton->setIcon(QIcon::fromTheme(QStringLiteral("view-barcode"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/view-barcode.svg"))));
    viewIdButton->setFlat(true);
    cornerFrameLayout->addWidget(viewIdButton);
    auto *restartButton = new QPushButton(m_cornerFrame);
    restartButton->setToolTip(tr("Restart Syncthing"));
    restartButton->setIcon(QIcon::fromTheme(QStringLiteral("system-reboot"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/view-refresh.svg"))));
    restartButton->setFlat(true);
    cornerFrameLayout->addWidget(restartButton);
    auto *showLogButton = new QPushButton(m_cornerFrame);
    showLogButton->setToolTip(tr("Show Syncthing log"));
    showLogButton->setIcon(QIcon::fromTheme(QStringLiteral("text-x-generic"), QIcon(QStringLiteral(":/icons/hicolor/scalable/mimetypes/text-x-generic.svg"))));
    showLogButton->setFlat(true);
    cornerFrameLayout->addWidget(showLogButton);
    auto *scanAllButton = new QPushButton(m_cornerFrame);
    scanAllButton->setToolTip(tr("Rescan all directories"));
    scanAllButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-sync"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/folder-sync.svg"))));
    scanAllButton->setFlat(true);
    cornerFrameLayout->addWidget(scanAllButton);
    m_ui->tabWidget->setCornerWidget(m_cornerFrame, Qt::BottomRightCorner);

    // setup connection menu
    m_connectionsActionGroup = new QActionGroup(m_connectionsMenu = new QMenu(tr("Connection"), this));
    m_connectionsMenu->setIcon(QIcon::fromTheme(QStringLiteral("network-connect"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/network-connect.svg"))));
    m_ui->connectionsPushButton->setText(Settings::primaryConnectionSettings().label);
    m_ui->connectionsPushButton->setMenu(m_connectionsMenu);

    // apply settings, this also establishes the connection to Syncthing
    applySettings();

    // setup other widgets
    m_ui->notificationsPushButton->setHidden(true);
    m_ui->trafficIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("network-card"), QIcon(QStringLiteral(":/icons/hicolor/scalable/devices/network-card.svg"))).pixmap(32));

    // connect signals and slots
    connect(m_ui->statusPushButton, &QPushButton::clicked, this, &TrayWidget::changeStatus);
    connect(m_ui->closePushButton, &QPushButton::clicked, &QCoreApplication::quit);
    connect(m_ui->aboutPushButton, &QPushButton::clicked, this, &TrayWidget::showAboutDialog);
    connect(m_ui->webUiPushButton, &QPushButton::clicked, this, &TrayWidget::showWebUi);
    connect(m_ui->settingsPushButton, &QPushButton::clicked, this, &TrayWidget::showSettingsDialog);
    connect(&m_connection, &SyncthingConnection::statusChanged, this, &TrayWidget::handleStatusChanged);
    connect(&m_connection, &SyncthingConnection::trafficChanged, this, &TrayWidget::updateTraffic);
    connect(&m_connection, &SyncthingConnection::newNotification, this, &TrayWidget::handleNewNotification);
    connect(m_ui->dirsTreeView, &DirView::openDir, this, &TrayWidget::openDir);
    connect(m_ui->dirsTreeView, &DirView::scanDir, this, &TrayWidget::scanDir);
    connect(m_ui->devsTreeView, &DevView::pauseResumeDev, this, &TrayWidget::pauseResumeDev);
    connect(m_ui->downloadsTreeView, &DownloadView::openDir, this, &TrayWidget::openDir);
    connect(m_ui->downloadsTreeView, &DownloadView::openItemDir, this, &TrayWidget::openItemDir);
    connect(scanAllButton, &QPushButton::clicked, &m_connection, &SyncthingConnection::rescanAllDirs);
    connect(viewIdButton, &QPushButton::clicked, this, &TrayWidget::showOwnDeviceId);
    connect(showLogButton, &QPushButton::clicked, this, &TrayWidget::showLog);
    connect(m_ui->notificationsPushButton, &QPushButton::clicked, this, &TrayWidget::showNotifications);
    connect(restartButton, &QPushButton::clicked, &m_connection, &SyncthingConnection::restart);
    connect(m_connectionsActionGroup, &QActionGroup::triggered, this, &TrayWidget::handleConnectionSelected);
}

TrayWidget::~TrayWidget()
{}

void TrayWidget::showSettingsDialog()
{
    if(!m_settingsDlg) {
        m_settingsDlg = new SettingsDialog(&m_connection, this);
        connect(m_settingsDlg, &SettingsDialog::applied, this, &TrayWidget::applySettings);
    }
    centerWidget(m_settingsDlg);
    showDialog(m_settingsDlg);
}

void TrayWidget::showAboutDialog()
{
    if(!m_aboutDlg) {
        m_aboutDlg = new AboutDialog(this, QString(), QStringLiteral(APP_AUTHOR "\nfallback icons from KDE/Breeze project\nSyncthing icons from Syncthing project"), QString(), QString(), QStringLiteral(APP_DESCRIPTION), QImage(QStringLiteral(":/icons/hicolor/scalable/app/syncthingtray.svg")));
        m_aboutDlg->setWindowTitle(tr("About") + QStringLiteral(" - " APP_NAME));
        m_aboutDlg->setWindowIcon(QIcon(QStringLiteral(":/icons/hicolor/scalable/app/syncthingtray.svg")));
    }
    centerWidget(m_aboutDlg);
    showDialog(m_aboutDlg);
}

void TrayWidget::showWebUi()
{
#ifndef SYNCTHINGTRAY_NO_WEBVIEW
    if(Settings::webViewDisabled()) {
#endif
        QDesktopServices::openUrl(m_connection.syncthingUrl());
#ifndef SYNCTHINGTRAY_NO_WEBVIEW
    } else {
        if(!m_webViewDlg) {
            m_webViewDlg = new WebViewDialog(this);
            if(m_selectedConnection) {
                m_webViewDlg->applySettings(*m_selectedConnection);
            }
            connect(m_webViewDlg, &WebViewDialog::destroyed, this, &TrayWidget::handleWebViewDeleted);
        }
        showDialog(m_webViewDlg);
    }
#endif
}

void TrayWidget::showOwnDeviceId()
{
    auto *dlg = new QWidget(this, Qt::Window);
    dlg->setWindowTitle(tr("Own device ID") + QStringLiteral(" - " APP_NAME));
    dlg->setWindowIcon(QIcon(QStringLiteral(":/icons/hicolor/scalable/app/syncthingtray.svg")));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setBackgroundRole(QPalette::Background);
    auto *layout = new QVBoxLayout(dlg);
    layout->setAlignment(Qt::AlignCenter);
    auto *pixmapLabel = new QLabel(dlg);
    pixmapLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(pixmapLabel);
    auto *textLabel = new QLabel(dlg);
    textLabel->setText(m_connection.myId().isEmpty() ? tr("device ID is unknown") : m_connection.myId());
    QFont defaultFont = textLabel->font();
    defaultFont.setBold(true);
    defaultFont.setPointSize(defaultFont.pointSize() + 2);
    textLabel->setFont(defaultFont);
    textLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(textLabel);
    auto *copyPushButton = new QPushButton(dlg);
    copyPushButton->setText(tr("Copy to clipboard"));
    connect(copyPushButton, &QPushButton::clicked, bind(&QClipboard::setText, QGuiApplication::clipboard(), m_connection.myId(), QClipboard::Clipboard));
    layout->addWidget(copyPushButton);
    connect(dlg, &QWidget::destroyed,
            bind(static_cast<bool(*)(const QMetaObject::Connection &)>(&QObject::disconnect),
                 m_connection.requestQrCode(m_connection.myId(), bind(&QLabel::setPixmap, pixmapLabel, placeholders::_1))
                 ));
    dlg->setLayout(layout);
    centerWidget(dlg);
    showDialog(dlg);
}

void TrayWidget::showLog()
{
    auto *dlg = new TextViewDialog(tr("Log"), this);
    connect(dlg, &QWidget::destroyed,
            bind(static_cast<bool(*)(const QMetaObject::Connection &)>(&QObject::disconnect),
                 m_connection.requestLog([dlg] (const std::vector<SyncthingLogEntry> &entries) {
                    for(const SyncthingLogEntry &entry : entries) {
                        dlg->browser()->append(entry.when % QChar(':') % QChar(' ') % QChar('\n') % entry.message % QChar('\n'));
                    }
        })
    ));
    showDialog(dlg);
}

void TrayWidget::showNotifications()
{
    auto *dlg = new TextViewDialog(tr("New notifications"), this);
    for(const SyncthingLogEntry &entry : m_notifications) {
        dlg->browser()->append(entry.when % QChar(':') % QChar(' ') % QChar('\n') % entry.message % QChar('\n'));
    }
    m_notifications.clear();
    showDialog(dlg);
    m_connection.notificationsRead();
    m_ui->notificationsPushButton->setHidden(true);
}

void TrayWidget::handleStatusChanged(SyncthingStatus status)
{
    switch(status) {
    case SyncthingStatus::Disconnected:
        m_ui->statusPushButton->setText(tr("Connect"));
        m_ui->statusPushButton->setToolTip(tr("Not connected to Syncthing, click to connect"));
        m_ui->statusPushButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/view-refresh.svg"))));
        m_ui->statusPushButton->setHidden(false);
        updateTraffic(); // ensure previous traffic statistics are no longer shown
        break;
    case SyncthingStatus::Reconnecting:
        m_ui->statusPushButton->setHidden(true);
        break;
    case SyncthingStatus::Idle:
    case SyncthingStatus::Scanning:
    case SyncthingStatus::NotificationsAvailable:
    case SyncthingStatus::Synchronizing:
        m_ui->statusPushButton->setText(tr("Pause"));
        m_ui->statusPushButton->setToolTip(tr("Syncthing is running, click to pause all devices"));
        m_ui->statusPushButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/media-playback-pause.svg"))));
        m_ui->statusPushButton->setHidden(false);
        break;
    case SyncthingStatus::Paused:
        m_ui->statusPushButton->setText(tr("Continue"));
        m_ui->statusPushButton->setToolTip(tr("At least one device is paused, click to resume"));
        m_ui->statusPushButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/media-playback-resume.svg"))));
        m_ui->statusPushButton->setHidden(false);
        break;
    }
}

void TrayWidget::applySettings()
{
    // update connections menu
    int connectionIndex = 0;
    const int connectionCount = static_cast<int>(1 + Settings::secondaryConnectionSettings().size());
    const QList<QAction *> connectionActions = m_connectionsActionGroup->actions();
    m_selectedConnection = nullptr;
    for(; connectionIndex < connectionCount; ++connectionIndex) {
        Settings::ConnectionSettings &connectionSettings = (connectionIndex == 0 ? Settings::primaryConnectionSettings() : Settings::secondaryConnectionSettings()[static_cast<size_t>(connectionIndex - 1)]);
        if(connectionIndex < connectionActions.size()) {
            QAction *action = connectionActions.at(connectionIndex);
            action->setText(connectionSettings.label);
            if(action->isChecked()) {
                m_selectedConnection = &connectionSettings;
            }
        } else {
            QAction *action = m_connectionsMenu->addAction(connectionSettings.label);
            action->setCheckable(true);
            m_connectionsActionGroup->addAction(action);
        }
    }
    for(; connectionIndex < connectionActions.size(); ++connectionIndex) {
        m_connectionsActionGroup->removeAction(connectionActions.at(connectionIndex));
    }
    if(!m_selectedConnection) {
        m_selectedConnection = &Settings::primaryConnectionSettings();
        m_connectionsMenu->actions().at(0)->setChecked(true);
    }

    m_connection.reconnect(*m_selectedConnection);

    // web view
#ifndef SYNCTHINGTRAY_NO_WEBVIEW
    if(m_webViewDlg) {
        m_webViewDlg->applySettings(*m_selectedConnection);
    }
#endif

    // update visual appearance
    m_ui->trafficFormWidget->setVisible(Settings::showTraffic());
    if(Settings::showTraffic()) {
        updateTraffic();
    }
    m_ui->infoFrame->setFrameStyle(Settings::frameStyle());
    m_ui->buttonsFrame->setFrameStyle(Settings::frameStyle());
    if(QApplication::style() && !QApplication::style()->objectName().compare(QLatin1String("adwaita"), Qt::CaseInsensitive)) {
        m_cornerFrame->setFrameStyle(QFrame::NoFrame);
    } else {
        m_cornerFrame->setFrameStyle(Settings::frameStyle());
    }
}

void TrayWidget::openDir(const SyncthingDir &dir)
{
    if(QDir(dir.path).exists()) {
        DesktopUtils::openLocalFileOrDir(dir.path);
    } else {
        QMessageBox::warning(this, QCoreApplication::applicationName(), tr("The directory <i>%1</i> does not exist on the local machine.").arg(dir.path));
    }
}

void TrayWidget::openItemDir(const SyncthingItemDownloadProgress &item)
{
    if(item.fileInfo.exists()) {
        DesktopUtils::openLocalFileOrDir(item.fileInfo.path());
    } else {
        QMessageBox::warning(this, QCoreApplication::applicationName(), tr("The file <i>%1</i> does not exist on the local machine.").arg(item.fileInfo.filePath()));
    }
}

void TrayWidget::scanDir(const SyncthingDir &dir)
{
    m_connection.rescan(dir.id);
}

void TrayWidget::pauseResumeDev(const SyncthingDev &dev)
{
    if(dev.paused) {
        m_connection.resume(dev.id);
    } else {
        m_connection.pause(dev.id);
    }
}

void TrayWidget::changeStatus()
{
    switch(m_connection.status()) {
    case SyncthingStatus::Disconnected:
        m_connection.connect();
        break;
    case SyncthingStatus::Reconnecting:
        break;
    case SyncthingStatus::Idle:
    case SyncthingStatus::Scanning:
    case SyncthingStatus::NotificationsAvailable:
    case SyncthingStatus::Synchronizing:
        m_connection.pauseAllDevs();
        break;
    case SyncthingStatus::Paused:
        m_connection.resumeAllDevs();
        break;
    }
}

void TrayWidget::updateTraffic()
{
    if(m_ui->trafficFormWidget->isHidden()) {
        return;
    }
    static const QString unknownStr(tr("unknown"));
    if(m_connection.isConnected()) {
        if(m_connection.totalIncomingRate() != 0.0) {
            m_ui->inTrafficLabel->setText(m_connection.totalIncomingTraffic() >= 0
                                          ? QStringLiteral("%1 (%2)").arg(QString::fromUtf8(bitrateToString(m_connection.totalIncomingRate(), true).data()), QString::fromUtf8(dataSizeToString(m_connection.totalIncomingTraffic()).data()))
                                          : QString::fromUtf8(bitrateToString(m_connection.totalIncomingRate(), true).data()));
        } else {
            m_ui->inTrafficLabel->setText(m_connection.totalIncomingTraffic() >= 0 ? QString::fromUtf8(dataSizeToString(m_connection.totalIncomingTraffic()).data()) : unknownStr);
        }
        if(m_connection.totalOutgoingRate() != 0.0) {
            m_ui->outTrafficLabel->setText(m_connection.totalIncomingTraffic() >= 0
                                          ? QStringLiteral("%1 (%2)").arg(QString::fromUtf8(bitrateToString(m_connection.totalOutgoingRate(), true).data()), QString::fromUtf8(dataSizeToString(m_connection.totalOutgoingTraffic()).data()))
                                          : QString::fromUtf8(bitrateToString(m_connection.totalOutgoingRate(), true).data()));
        } else {
            m_ui->outTrafficLabel->setText(m_connection.totalOutgoingTraffic() >= 0 ? QString::fromUtf8(dataSizeToString(m_connection.totalOutgoingTraffic()).data()) : unknownStr);
        }
    } else {
        m_ui->inTrafficLabel->setText(unknownStr);
        m_ui->outTrafficLabel->setText(unknownStr);
    }

}

#ifndef SYNCTHINGTRAY_NO_WEBVIEW
void TrayWidget::handleWebViewDeleted()
{
    m_webViewDlg = nullptr;
}
#endif

void TrayWidget::handleNewNotification(DateTime when, const QString &msg)
{
    m_notifications.emplace_back(QString::fromLocal8Bit(when.toString(DateTimeOutputFormat::DateAndTime, true).data()), msg);
    m_ui->notificationsPushButton->setHidden(false);
}

void TrayWidget::handleConnectionSelected(QAction *connectionAction)
{
    int index = m_connectionsMenu->actions().indexOf(connectionAction);
    if(index >= 0) {
        m_selectedConnection = (index == 0)
                ? &Settings::primaryConnectionSettings()
                : &Settings::secondaryConnectionSettings()[static_cast<size_t>(index - 1)];
        m_ui->connectionsPushButton->setText(m_selectedConnection->label);
        m_connection.reconnect(*m_selectedConnection);
#ifndef SYNCTHINGTRAY_NO_WEBVIEW
        if(m_webViewDlg) {
            m_webViewDlg->applySettings(*m_selectedConnection);
        }
#endif
    }
}

void TrayWidget::showDialog(QWidget *dlg)
{
    if(m_menu) {
        m_menu->close();
    }
    dlg->show();
    dlg->activateWindow();
}

}
