#include "settingswatcher.h"

#include <KCoreConfigSkeleton>
#include <KSharedConfig>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

using namespace std::chrono_literals;

SettingsWatcher::SettingsWatcher(KCoreConfigSkeleton *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_path(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QDir::separator() + settings->sharedConfig()->name())
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100ms);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        m_settings->sharedConfig()->reparseConfiguration();
        m_settings->read();
        rewatch();
    });

    // KConfig saves by renaming a temporary file over the old one, which drops the
    // file watch, so watch the directory as well and re-add the file.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    m_watcher.addPath(QFileInfo(m_path).absolutePath());
    rewatch();
}

void SettingsWatcher::rewatch()
{
    if (QFileInfo::exists(m_path) && !m_watcher.files().contains(m_path)) {
        m_watcher.addPath(m_path);
    }
}
