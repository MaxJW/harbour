#pragma once

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QSet>
#include <QTimer>
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
 * A trailing ?<revision> (DockTools::iconThemeRevision) is ignored; it only makes the URL
 * new, so Qt Quick asks again rather than reusing its copy from the old icon theme.
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
    /// Goes up each time a program's icon has been read for wineIcon().
    Q_PROPERTY(int wineIconRevision READ wineIconRevision NOTIFY wineIconsChanged)
    /// Goes up each time the icon theme is changed (System Settings → Colours & Themes → Icons).
    Q_PROPERTY(int iconThemeRevision READ iconThemeRevision NOTIFY iconThemeChanged)

public:
    explicit DockTools(QObject *parent = nullptr);

    bool logGeometry() const;
    int wineIconRevision() const { return m_wineIconRevision; }
    int iconThemeRevision() const { return m_iconThemeRevision; }

    /// URL for an icon given as a QIcon (e.g. a TasksModel decoration) or an icon name.
    /// evenTile: draw it at the dock's common tile size and corners if it's a rounded square.
    /// Pass iconThemeRevision so the binding asks again, and the icon is drawn again, when
    /// the icon theme changes.
    Q_INVOKABLE QUrl iconUrl(const QVariant &icon, bool evenTile = false, int themeRevision = 0) const;
    /// An app's icon in the dock: the tasks model's, unless that is a window's own picture
    /// while the app's desktop file names an icon the theme has. The model works an app's
    /// icon out once per window, so asked under a theme without it (e.g. "discord" in
    /// Breeze), it keeps the window's for good, through later theme changes.
    Q_INVOKABLE QVariant appIcon(const QUrl &launcherUrl, const QVariant &decoration, int themeRevision) const;
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

    /// The name Steam shows for a game run through it, from its window's app id
    /// ("steam_app_<appid>", which has no desktop file): a non-Steam shortcut's name
    /// (e.g. a Wine launcher added to Steam) or an installed game's. "" for anything else.
    Q_INVOKABLE QString steamAppName(const QString &appId) const;
    /// When a process started (clock ticks since boot, from /proc), or -1 if unknown.
    Q_INVOKABLE qint64 processStartTime(qint64 pid) const;
    /// A window's icon, or for a program running under Wine its .exe's icon when that's
    /// sharper: games often give their window a 32 px icon and keep a 256 px one in the
    /// .exe. The .exe is read in KDE's thumbnail worker; until then (and if it has no
    /// larger icon) this is the window's own. Pass wineIconRevision so the binding asks again.
    Q_INVOKABLE QVariant wineIcon(qint64 pid, const QVariant &windowIcon, int revision);

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

Q_SIGNALS:
    void wineIconsChanged();
    void iconThemeChanged();

private Q_SLOTS:
    void onIconLoaderChanged();

private:
    QHash<QString, QIcon> m_exeIcons; // by .exe path; a null icon when it has none
    QSet<QString> m_exeIconsPending;
    int m_wineIconRevision = 0;
    int m_iconThemeRevision = 0;
    QTimer m_iconThemeDebounce;
};
