#include "docktools.h"
#include "blurbehind.h"

#include <KDesktopFile>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CopyJob>
#include <KIO/Global>
#include <KIO/OpenUrlJob>
#include <KIO/PreviewJob>
#include <KService>

#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QCursor>
#include <QGuiApplication>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QCache>
#include <QFile>
#include <QRegularExpression>
#include <QtEndian>
#include <QIcon>
#include <QLoggingCategory>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>

#include <optional>

using namespace Qt::StringLiterals;

namespace
{
// Icons that have no theme name (window icons, e.g. games under Wine), keyed by
// QIcon::cacheKey(). Every window brings a new one, so only the most recent are kept.
QCache<qint64, QIcon> &iconCache()
{
    static QCache<qint64, QIcon> cache(64);
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

// App icons drawn as rounded squares ("tiles": many third-party apps, and themes for
// apps they draw that way) come in every size and corner radius; one filling 96% of its
// canvas sits beside one filling 80%. Tiles are redrawn on macOS's grid instead, where a
// tile is 824 of 1024 px with corners of about 22% (the radius Zed's icon uses too).
// 82% rather than 80% because that reads as the same size as Papirus's circles (87.5%).
// Anything else (circles, folders, irregular shapes) is left as the theme drew it.
constexpr qreal TileSide = 0.82;
constexpr qreal TileRadius = 0.225;

// How much darker an item is drawn while pressed or while its menu is open (by eye).
constexpr qreal PressedDarkening = 0.3;

// The rounded square an icon is drawn as, if it is one: its opaque part is square, fills
// at least 90% of that square (a circle fills 79%), and has straight sides.
std::optional<QRect> tileRect(const QImage &source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const auto opaque = [&image](int x, int y) {
        return qAlpha(reinterpret_cast<const QRgb *>(image.constScanLine(y))[x]) > 127;
    };

    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    qint64 count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (opaque(x, y)) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
                ++count;
            }
        }
    }
    const QRect box(QPoint(left, top), QPoint(right, bottom));
    const int side = qMax(box.width(), box.height());
    if (right < 0 || side < image.width() / 2 || qAbs(box.width() - box.height()) > side / 100) {
        return std::nullopt;
    }
    if (count < 0.9 * box.width() * box.height()) {
        return std::nullopt;
    }
    // A folder's tab or a squarish blob has the area but not four flat sides.
    const int inset = qMax(1, side / 64);
    for (int t = side * 3 / 10; t < side * 7 / 10; ++t) {
        if (!opaque(box.left() + t, box.top() + inset) || !opaque(box.left() + t, box.bottom() - inset)
            || !opaque(box.left() + inset, box.top() + t) || !opaque(box.right() - inset, box.top() + t)) {
            return std::nullopt;
        }
    }
    return box;
}

// The tile part of the icon, at TileSide of the target and clipped to TileRadius corners.
// Clipping only ever takes off corners that were sharper; rounder ones keep their shape.
QImage evenTile(const QImage &source, const QRect &tile, const QSize &target)
{
    const int side = qRound(TileSide * qMin(target.width(), target.height()));
    const QPoint origin((target.width() - side) / 2, (target.height() - side) / 2);
    const QImage scaled = source.copy(tile).scaled(side, side, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(scaled);
    painter.setBrushOrigin(origin);
    painter.drawRoundedRect(QRect(origin, QSize(side, side)), side * TileRadius, side * TileRadius);
    return image;
}
}

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (id.startsWith(u"pressed/")) {
        QImage image = requestImage(id.mid(8), size, requestedSize).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        // Black over the icon's own pixels only: colours scaled down, alpha untouched.
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        painter.fillRect(image.rect(), QColor::fromRgbF(0, 0, 0, PressedDarkening));
        return image;
    }

    const bool even = id.startsWith(u"tile/");
    const QString path = even ? id.mid(5) : id;
    const QString kind = path.section(u'/', 0, 0);
    const QString key = path.section(u'/', 1);

    QIcon icon;
    if (kind == u"theme") {
        icon = QIcon::fromTheme(key);
    } else if (kind == u"cache") {
        if (const QIcon *cached = iconCache().object(key.toLongLong())) {
            icon = *cached;
        }
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(u"application-x-executable"_s);
    }

    const QSize target = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    if (even) {
        // Looked at twice as large, so the tile is scaled down into place, never up.
        const QImage large = icon.pixmap(target * 2).toImage();
        if (const auto tile = tileRect(large)) {
            const QImage image = evenTile(large, *tile, target);
            if (size) {
                *size = image.size();
            }
            return image;
        }
    }
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

QUrl DockTools::iconUrl(const QVariant &icon, bool evenTile) const
{
    const QString prefix = evenTile ? u"image://harbour/tile/"_s : u"image://harbour/"_s;

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
    iconCache().insert(qicon.cacheKey(), new QIcon(qicon));
    return QUrl(prefix + u"cache/"_s + QString::number(qicon.cacheKey()));
}

QUrl DockTools::pressedIconUrl(const QUrl &iconUrl) const
{
    if (iconUrl.scheme() != u"image" || iconUrl.host() != u"harbour") {
        return iconUrl;
    }
    QUrl url = iconUrl;
    url.setPath(u"/pressed"_s + iconUrl.path());
    return url;
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

bool DockTools::logGeometry() const
{
    // The same category Dock.qml logs to, so the same logging rules switch it on.
    static const QLoggingCategory category("harbour.geometry", QtWarningMsg);
    return category.isInfoEnabled();
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

QUrl DockTools::previewUrl(const QUrl &file) const
{
    return QUrl(u"image://harbourpreview/"_s + QString::fromLatin1(QUrl::toPercentEncoding(file.toString())));
}

namespace
{
class PreviewResponse : public QQuickImageResponse
{
public:
    PreviewResponse(const QUrl &url, const QSize &size)
    {
        // Made on Qt Quick's image loading thread, but KIO jobs belong to the main thread:
        // the response lives there too, so the job never outlives it mid-callback.
        moveToThread(qApp->thread());
        QMetaObject::invokeMethod(this, [this, url, size] {
            start(url, size);
        }, Qt::QueuedConnection);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    void cancel() override
    {
        QMetaObject::invokeMethod(this, [this] {
            if (m_job) {
                m_job->kill(KJob::Quietly);
            }
            Q_EMIT finished();
        }, Qt::QueuedConnection);
    }

private:
    void start(const QUrl &url, const QSize &size)
    {
        // Plugins and size limits from the user's preview settings, as in Dolphin.
        m_job = new KIO::PreviewJob(KFileItemList{KFileItem(url)}, size);
        connect(m_job, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &, const QPixmap &preview) {
            m_image = preview.toImage();
        });
        connect(m_job, &KJob::result, this, [this, url, size] {
            if (m_image.isNull()) {
                m_image = QIcon::fromTheme(KIO::iconNameForUrl(url)).pixmap(size).toImage();
            }
            Q_EMIT finished();
        });
    }

    QPointer<KIO::PreviewJob> m_job;
    QImage m_image;
};
}

QQuickImageResponse *PreviewProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    const QSize size = requestedSize.isValid() ? requestedSize : QSize(128, 128);
    return new PreviewResponse(QUrl(QUrl::fromPercentEncoding(id.toLatin1())), size);
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

void DockTools::clearBlurBehind(QQuickItem *item) const
{
    if (!item || !item->window()) {
        return;
    }
    if (auto *blur = item->window()->findChild<BlurBehind *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (blur->setRegion(QRect(), 0)) {
            item->window()->update();
        }
    }
}

void DockTools::moveToFolder(const QList<QUrl> &urls, const QUrl &folder) const
{
    if (!urls.isEmpty()) {
        KIO::move(urls, folder)->start();
    }
}

namespace
{
// Steam installs: native, the ~/.steam link to it, and Flathub's.
QStringList steamRoots()
{
    QStringList roots;
    for (const QString &path : {QDir::homePath() + u"/.local/share/Steam"_s, QDir::homePath() + u"/.steam/steam"_s,
                                QDir::homePath() + u"/.var/app/com.valvesoftware.Steam/.local/share/Steam"_s}) {
        const QString root = QFileInfo(path).canonicalFilePath();
        if (!root.isEmpty() && !roots.contains(root)) {
            roots.append(root);
        }
    }
    return roots;
}

// Steam's binary KeyValues (shortcuts.vdf): a type byte, a NUL-terminated key, then the
// value: 0 a nested map up to its 8, 1 a NUL-terminated string, 2 a 32-bit integer.
class BinaryVdf
{
public:
    explicit BinaryVdf(const QByteArray &data)
        : m_data(data)
    {
    }

    // The map's entries, or nothing if the file is cut short or has types we don't know.
    std::optional<QVariantMap> map()
    {
        QVariantMap result;
        while (m_pos < m_data.size()) {
            const char type = m_data[m_pos++];
            if (type == 8) {
                return result;
            }
            const auto key = string();
            if (!key) {
                return {};
            }
            if (type == 0) {
                const auto value = map();
                if (!value) {
                    return {};
                }
                result.insert(*key, *value);
            } else if (type == 1) {
                const auto value = string();
                if (!value) {
                    return {};
                }
                result.insert(*key, *value);
            } else if (type == 2 && m_pos + 4 <= m_data.size()) {
                result.insert(*key, qFromLittleEndian<quint32>(m_data.constData() + m_pos));
                m_pos += 4;
            } else {
                return {};
            }
        }
        return {};
    }

private:
    std::optional<QString> string()
    {
        const qsizetype end = m_data.indexOf('\0', m_pos);
        if (end < 0) {
            return {};
        }
        const QString value = QString::fromUtf8(m_data.mid(m_pos, end - m_pos));
        m_pos = end + 1;
        return value;
    }

    const QByteArray m_data;
    qsizetype m_pos = 0;
};

// A key's value in Steam's text KeyValues (appmanifest_*.acf, libraryfolders.vdf):
// every `"key" "value"` pair in the file, wherever it's nested.
QStringList textVdfValues(const QString &path, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QRegularExpression pair(u"^\\s*\"%1\"\\s+\"((?:[^\"\\\\]|\\\\.)*)\""_s.arg(QRegularExpression::escape(key)),
                                  QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    QStringList values;
    for (const QRegularExpressionMatch &match : pair.globalMatch(QString::fromUtf8(file.readAll()))) {
        values.append(match.captured(1).replace(u"\\\\"_s, u"\\"_s).replace(u"\\\""_s, u"\""_s));
    }
    return values;
}

QString shortcutName(const QString &root, quint32 appId)
{
    const QFileInfoList users = QDir(root + u"/userdata"_s).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &user : users) {
        QFile file(user.absoluteFilePath() + u"/config/shortcuts.vdf"_s);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        // The file holds one map, "shortcuts", of maps "0", "1", ...
        const auto parsed = BinaryVdf(file.readAll()).map();
        if (!parsed) {
            continue;
        }
        const QVariantMap shortcuts = parsed->value(u"shortcuts"_s).toMap();
        for (const QVariant &entry : shortcuts) {
            QVariantMap fields;
            // Older Steam writes the keys in lower case.
            const QVariantMap raw = entry.toMap();
            for (auto it = raw.begin(); it != raw.end(); ++it) {
                fields.insert(it.key().toLower(), it.value());
            }
            if (fields.value(u"appid"_s).toUInt() == appId) {
                return fields.value(u"appname"_s).toString();
            }
        }
    }
    return {};
}

QString installedGameName(const QString &root, quint32 appId)
{
    QStringList libraries = textVdfValues(root + u"/steamapps/libraryfolders.vdf"_s, u"path"_s);
    libraries.prepend(root);
    for (const QString &library : std::as_const(libraries)) {
        const QStringList names = textVdfValues(library + u"/steamapps/appmanifest_%1.acf"_s.arg(appId), u"name"_s);
        if (!names.isEmpty()) {
            return names.first();
        }
    }
    return {};
}
}

QString DockTools::steamAppName(const QString &appId) const
{
    static const QRegularExpression steamAppId(u"^steam_app_(\\d+)$"_s);
    const QRegularExpressionMatch match = steamAppId.match(appId);
    bool ok = false;
    const quint32 id = match.hasMatch() ? match.captured(1).toUInt(&ok) : 0;
    if (!ok) {
        return {};
    }
    // Read afresh each time (only when an icon is made), so a rename in Steam shows next time.
    for (const QString &root : steamRoots()) {
        QString name = shortcutName(root, id);
        if (name.isEmpty()) {
            name = installedGameName(root, id);
        }
        if (!name.isEmpty()) {
            return name;
        }
    }
    return {};
}

qint64 DockTools::processStartTime(qint64 pid) const
{
    QFile file(u"/proc/%1/stat"_s.arg(pid));
    if (pid <= 0 || !file.open(QIODevice::ReadOnly)) {
        return -1;
    }
    // "pid (comm) state ppid ...": comm can hold spaces and brackets, so count the fields
    // after its last ')'. starttime is field 22, the 20th after comm.
    const QByteArray stat = file.readAll();
    const QList<QByteArray> fields = stat.mid(stat.lastIndexOf(')') + 2).split(' ');
    bool ok = false;
    const qint64 start = fields.value(19).toLongLong(&ok);
    return ok ? start : -1;
}
