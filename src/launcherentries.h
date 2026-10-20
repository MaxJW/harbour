#pragma once

#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

/**
 * Badge counts and progress bars apps publish through the Unity LauncherEntry
 * D-Bus API (com.canonical.Unity.LauncherEntry.Update), which Firefox, Chromium,
 * Discord clients, Thunderbird and many KDE apps use.
 *
 * Keyed by desktop file id ("firefox.desktop"). QML reads through the functions
 * with `revision` as an extra argument so bindings re-evaluate on updates.
 */
class LauncherEntries : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit LauncherEntries(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    /// Badge number, or 0 for none. `launcherUrl` is a TasksModel launcher URL.
    Q_INVOKABLE int count(const QUrl &launcherUrl, int revision = 0) const;
    /// Progress 0–1, or a negative number for none.
    Q_INVOKABLE qreal progress(const QUrl &launcherUrl, int revision = 0) const;
    Q_INVOKABLE bool urgent(const QUrl &launcherUrl, int revision = 0) const;

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void onUpdate(const QString &appUri, const QVariantMap &properties);

private:
    struct Entry {
        qint64 count = 0;
        bool countVisible = false;
        qreal progress = 0;
        bool progressVisible = false;
        bool urgent = false;
    };

    static QString keyFor(const QUrl &launcherUrl);

    QHash<QString, Entry> m_entries;
    int m_revision = 0;
};
