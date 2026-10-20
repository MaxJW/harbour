#include "docktools.h"
#include "blurbehind.h"

#include <KDesktopFile>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CopyJob>
#include <KIO/Global>
#include <KIO/OpenUrlJob>
#include <KService>

#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QCursor>
#include <QGuiApplication>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QHash>
#include <QIcon>
#include <QQuickWindow>

using namespace Qt::StringLiterals;

namespace
{
// Icons that have no theme name, keyed by QIcon::cacheKey(). Only ever grows by the
// number of distinct nameless window icons seen, which is small.
QHash<qint64, QIcon> &iconCache()
{
    static QHash<qint64, QIcon> cache;
    return cache;
}

KService::Ptr serviceForLauncher(const QUrl &launcherUrl)
{
    if (launcherUrl.scheme() == u"applications") {
        return KService::serviceByMenuId(launcherUrl.path());
    }
    if (launcherUrl.isLocalFile() && KDesktopFile::isDesktopFile(launcherUrl.toLocalFile())) {
        return KService::serviceByDesktopPath(launcherUrl.toLocalFile());
    }
    return {};
}
}

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString kind = id.section(u'/', 0, 0);
    const QString key = id.section(u'/', 1);

    QIcon icon;
    if (kind == u"theme") {
        icon = QIcon::fromTheme(key);
    } else if (kind == u"cache") {
        icon = iconCache().value(key.toLongLong());
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(u"application-x-executable"_s);
    }

    const QSize target = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    QImage image = icon.pixmap(target).toImage();
    // Bitmap-only icons may come back smaller than asked; scale them up so every
    // icon fills its slot the same way.
    if (!image.isNull() && image.size() != target) {
        image = image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (size) {
        *size = image.size();
    }
    return image;
}

QUrl DockTools::iconUrl(const QVariant &icon) const
{
    const QString prefix = u"image://harbour/"_s;

    if (icon.typeId() == QMetaType::QString) {
        return QUrl(prefix + u"theme/"_s + icon.toString());
    }

    const QIcon qicon = icon.value<QIcon>();
    if (qicon.isNull()) {
        return QUrl(prefix + u"theme/application-x-executable"_s);
    }
    if (!qicon.name().isEmpty()) {
        return QUrl(prefix + u"theme/"_s + qicon.name());
    }
    iconCache().insert(qicon.cacheKey(), qicon);
    return QUrl(prefix + u"cache/"_s + QString::number(qicon.cacheKey()));
}

QVariantList DockTools::desktopActions(const QUrl &launcherUrl) const
{
    QVariantList result;
    const KService::Ptr service = serviceForLauncher(launcherUrl);
    if (!service) {
        return result;
    }

    const auto actions = service->actions();
    for (int i = 0; i < actions.size(); ++i) {
        const KServiceAction &action = actions.at(i);
        if (action.noDisplay()) {
            continue;
        }
        result.append(QVariantMap{
            {u"index"_s, i},
            {u"text"_s, action.text()},
            {u"icon"_s, action.icon()},
            {u"separator"_s, action.isSeparator()},
        });
    }
    return result;
}

void DockTools::runDesktopAction(const QUrl &launcherUrl, int index) const
{
    const KService::Ptr service = serviceForLauncher(launcherUrl);
    if (!service || index < 0 || index >= service->actions().size()) {
        return;
    }
    auto *job = new KIO::ApplicationLauncherJob(service->actions().at(index));
    job->start();
}

void DockTools::showWindows(const QVariantList &windowIds) const
{
    QStringList ids;
    ids.reserve(windowIds.size());
    for (const QVariant &id : windowIds) {
        ids.append(id.toString());
    }

    QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin.Effect.WindowView1"_s,
                                                          u"/org/kde/KWin/Effect/WindowView1"_s,
                                                          u"org.kde.KWin.Effect.WindowView1"_s,
                                                          u"activate"_s);
    message << ids;
    QDBusConnection::sessionBus().asyncCall(message);
}

int DockTools::mouseButtons() const
{
    return int(QGuiApplication::mouseButtons());
}

QPointF DockTools::cursorPosition() const
{
    return QCursor::pos();
}

bool DockTools::isApplication(const QUrl &url) const
{
    return serviceForLauncher(url) != nullptr;
}

QString DockTools::iconNameForUrl(const QUrl &url) const
{
    return KIO::iconNameForUrl(url);
}

void DockTools::openUrl(const QUrl &url) const
{
    (new KIO::OpenUrlJob(url))->start();
}

QUrl DockTools::folderUrl(const QString &path) const
{
    QString expanded = path;
    if (expanded == u"~" || expanded.startsWith(u"~/")) {
        expanded.replace(0, 1, QDir::homePath());
    }
    return QUrl::fromLocalFile(expanded);
}

QString DockTools::stackPath(const QUrl &url) const
{
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir()) {
        return {};
    }
    const QString path = QDir::cleanPath(url.toLocalFile());
    const QString home = QDir::homePath();
    if (path == home) {
        return QStringLiteral("~");
    }
    if (path.startsWith(home + u'/')) {
        return u'~' + path.mid(home.size());
    }
    return path;
}

QVariantList DockTools::folderEntries(const QUrl &folder, int limit) const
{
    QVariantList result;
    const QFileInfoList entries = QDir(folder.toLocalFile()).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo &info : entries) {
        if (result.size() >= limit) {
            break;
        }
        result.append(QVariantMap{
            {u"url"_s, QUrl::fromLocalFile(info.absoluteFilePath())},
            {u"name"_s, info.fileName()},
            {u"isDir"_s, info.isDir()},
        });
    }
    return result;
}

int DockTools::folderTotal(const QUrl &folder) const
{
    return int(QDir(folder.toLocalFile()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size());
}

void DockTools::blurBehind(QQuickItem *item, qreal radius) const
{
    if (!item || !item->window()) {
        return;
    }
    QQuickWindow *window = item->window();
    // One BlurBehind per window, created on first use and owned by the window.
    auto *blur = window->findChild<BlurBehind *>(QString(), Qt::FindDirectChildrenOnly);
    if (!blur) {
        blur = new BlurBehind(window, window);
    }
    const QRectF rect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    if (blur->setRegion(rect.toRect(), qRound(radius))) {
        window->update();
    }
}

void DockTools::moveToFolder(const QList<QUrl> &urls, const QUrl &folder) const
{
    if (!urls.isEmpty()) {
        KIO::move(urls, folder)->start();
    }
}
