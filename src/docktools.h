#pragma once

#include <QObject>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QUrl>
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>

/**
 * Serves theme icons as textures at exactly the size QML asks for, so the dock can
 * draw them pixel-for-pixel at rest and at a higher resolution while magnified.
 *   image://harbour/theme/<icon-name>
 *   image://harbour/cache/<QIcon::cacheKey()>   for icons without a theme name
 * Either can be prefixed with tile/ (image://harbour/tile/theme/<icon-name>) to even out
 * an icon drawn as a rounded square, see evenTile() in the .cpp, and that with pressed/
 * for the darker icon shown while it's held down (image://harbour/pressed/tile/...).
 */
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

/**
 * Previews of the files in a stack, from KDE's thumbnailers (what Dolphin shows):
 *   image://harbourpreview/<percent-encoded file URL>
 * They're made in KIO's thumbnail worker process and cached in ~/.cache/thumbnails, so
 * a large photo is never decoded in the dock (that alone cost it ~35 MB for good: the
 * freed memory stays with the process). Files without a preview get their type's icon.
 */
class PreviewProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

/// Helpers for the dock's QML that need KDE frameworks or D-Bus.
class DockTools : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    /// Whether the harbour.geometry log (item positions, for scripted tests) is on.
    Q_PROPERTY(bool logGeometry READ logGeometry CONSTANT)

public:
    using QObject::QObject;

    bool logGeometry() const;

    /// URL for an icon given as a QIcon (e.g. a TasksModel decoration) or an icon name.
    /// evenTile: draw it at the dock's common tile size and corners if it's a rounded square.
    Q_INVOKABLE QUrl iconUrl(const QVariant &icon, bool evenTile = false) const;
    /// The same icon darkened, for while it's pressed. The software renderer the dock
    /// uses has no shader effects, so it's done to the image itself.
    Q_INVOKABLE QUrl pressedIconUrl(const QUrl &iconUrl) const;

    /// The desktop file's additional actions ("New Private Window" etc.) as
    /// [{text, icon, separator}], in desktop file order.
    Q_INVOKABLE QVariantList desktopActions(const QUrl &launcherUrl) const;
    Q_INVOKABLE void runDesktopAction(const QUrl &launcherUrl, int index) const;

    /// Has the compositor blur behind an item (a rounded rect) in whatever window it is
    /// shown in, e.g. a stack popup's glass panel. Call once the window is shown.
    Q_INVOKABLE void blurBehind(QQuickItem *item, qreal radius) const;
    /// Removes the blur blurBehind() set on the item's window.
    Q_INVOKABLE void clearBlurBehind(QQuickItem *item) const;

    /// KWin's window view for the given windows ("Show All Windows").
    Q_INVOKABLE void showWindows(const QVariantList &windowIds) const;

    /// Mouse buttons currently held down, as far as this process has seen.
    Q_INVOKABLE int mouseButtons() const;
    /// The pointer's last known global position (from events this process received).
    Q_INVOKABLE QPointF cursorPosition() const;

    /// Whether a URL dropped on the dock is an application that can be kept in it.
    Q_INVOKABLE bool isApplication(const QUrl &url) const;

    /// Theme icon name for a file or folder, by MIME type (folders get their special icons).
    Q_INVOKABLE QString iconNameForUrl(const QUrl &url) const;
    /// A file's preview (see PreviewProvider).
    Q_INVOKABLE QUrl previewUrl(const QUrl &file) const;
    /// Opens a file or folder with its default application.
    Q_INVOKABLE void openUrl(const QUrl &url) const;
    /// A folder's newest entries first, as [{url, name, isDir}], at most `limit` of them.
    /// Synchronous on purpose: stacks need the count to pick their layout before opening.
    Q_INVOKABLE QVariantList folderEntries(const QUrl &folder, int limit) const;
    /// Moves files into a folder (a drop onto a stack), with KIO's usual progress and conflict handling.
    Q_INVOKABLE void moveToFolder(const QList<QUrl> &urls, const QUrl &folder) const;
    /// Expands a leading "~" to the home directory and returns a file URL.
    Q_INVOKABLE QUrl folderUrl(const QString &path) const;
    /// The reverse, for pinning a stack: a local folder's path, as "~/…" inside the home
    /// directory, or "" if the URL isn't a local folder.
    Q_INVOKABLE QString stackPath(const QUrl &url) const;
};
