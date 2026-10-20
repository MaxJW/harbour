#include "launcherentries.h"

#include <QDBusConnection>
#include <QFileInfo>
#include <QUrl>

using namespace Qt::StringLiterals;

LauncherEntries::LauncherEntries(QObject *parent)
    : QObject(parent)
{
    // Apps broadcast the signal from their own object paths; listen to all of them.
    QDBusConnection::sessionBus().connect(QString(),
                                          QString(),
                                          u"com.canonical.Unity.LauncherEntry"_s,
                                          u"Update"_s,
                                          this,
                                          SLOT(onUpdate(QString, QVariantMap)));
}

QString LauncherEntries::keyFor(const QUrl &launcherUrl)
{
    // applications:firefox.desktop, file:///…/firefox.desktop and application://firefox.desktop
    // all come down to the desktop file name.
    QString path = launcherUrl.scheme() == u"applications" ? launcherUrl.path() : launcherUrl.toString();
    return QFileInfo(path).fileName();
}

void LauncherEntries::onUpdate(const QString &appUri, const QVariantMap &properties)
{
    const QString key = keyFor(QUrl(appUri));
    if (key.isEmpty()) {
        return;
    }
    Entry &entry = m_entries[key];
    // Each update carries only the properties that changed.
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        if (it.key() == u"count") {
            entry.count = it.value().toLongLong();
        } else if (it.key() == u"count-visible") {
            entry.countVisible = it.value().toBool();
        } else if (it.key() == u"progress") {
            entry.progress = std::clamp(it.value().toDouble(), 0.0, 1.0);
        } else if (it.key() == u"progress-visible") {
            entry.progressVisible = it.value().toBool();
        } else if (it.key() == u"urgent") {
            entry.urgent = it.value().toBool();
        }
    }
    ++m_revision;
    Q_EMIT changed();
}

int LauncherEntries::count(const QUrl &launcherUrl, int) const
{
    const auto it = m_entries.constFind(keyFor(launcherUrl));
    return it != m_entries.constEnd() && it->countVisible ? int(it->count) : 0;
}

qreal LauncherEntries::progress(const QUrl &launcherUrl, int) const
{
    const auto it = m_entries.constFind(keyFor(launcherUrl));
    return it != m_entries.constEnd() && it->progressVisible ? it->progress : -1;
}

bool LauncherEntries::urgent(const QUrl &launcherUrl, int) const
{
    const auto it = m_entries.constFind(keyFor(launcherUrl));
    return it != m_entries.constEnd() && it->urgent;
}
