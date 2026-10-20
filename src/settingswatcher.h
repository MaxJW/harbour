#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class KCoreConfigSkeleton;

/**
 * Re-reads the settings whenever harbourrc changes on disk, whoever wrote it
 * (the System Settings module, a text editor, kwriteconfig6), so every change
 * applies live. KConfigWatcher would only see writes made with KConfig::Notify.
 */
class SettingsWatcher : public QObject
{
    Q_OBJECT

public:
    explicit SettingsWatcher(KCoreConfigSkeleton *settings, QObject *parent = nullptr);

private:
    void rewatch();

    KCoreConfigSkeleton *m_settings;
    QString m_path;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
