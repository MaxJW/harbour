#include "dockmanager.h"
#include "autohidecontroller.h"
#include "dockwindow.h"
#include "harboursettings.h"
#include "logging.h"
#include "outputorder.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

DockManager::DockManager(QQmlEngine *engine, HarbourSettings *settings, OutputOrder *outputOrder, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_settings(settings)
    , m_outputOrder(outputOrder)
{
    QQmlComponent component(engine);
    component.loadFromModule("Harbour", "DockModel");
    m_shared = component.create();
    if (!m_shared) {
        qCritical() << component.errors();
        return;
    }
    QQmlEngine::setObjectOwnership(m_shared, QQmlEngine::CppOwnership);
    m_shared->setParent(this);

    connect(qGuiApp, &QGuiApplication::screenAdded, this, &DockManager::syncScreens);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        // Qt deletes the screen right after this signal; its dock must go first.
        removeDock(screen);
        syncScreens();
    });
    connect(m_outputOrder, &OutputOrder::orderChanged, this, &DockManager::syncScreens);
    connect(m_settings, &HarbourSettings::matchPhysicalSizeChanged, this, &DockManager::updateSizeScales);
    connect(m_settings, &HarbourSettings::screenModeChanged, this, &DockManager::syncScreens);

    m_revealTimer.setSingleShot(true);
    connect(&m_revealTimer, &QTimer::timeout, this, [this] {
        if (m_revealed) {
            m_revealed->autoHide()->release();
        }
        m_revealed = nullptr;
    });
}

void DockManager::revealFor(std::chrono::milliseconds duration, const QString &screenName)
{
    DockWindow *target = m_active;
    for (const auto &[screen, dock] : m_docks) {
        if (!screenName.isEmpty() && screen->name() == screenName) {
            target = dock.get();
        }
    }
    if (!target) {
        return;
    }
    // Hold the dock up (revealing it if hidden) until the timer runs out.
    if (m_revealed != target) {
        if (m_revealed) {
            m_revealed->autoHide()->release();
        }
        m_revealed = target;
        target->autoHide()->hold();
    }
    m_revealTimer.start(duration);
}

namespace
{
// One name per variant (see loadPointerScript): KWin takes a moment to let go of an
// unloaded script, and refuses to load another under the same name meanwhile.
const auto PointerScript = u"harbour-pointer"_s;
const auto FollowingPointerScript = u"harbour-pointer-following"_s;

QDBusMessage kwinScripting(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/Scripting"_s, u"org.kde.kwin.Scripting"_s, method);
    message.setArguments(arguments);
    return message;
}

// KWin loads scripts from files: writes one out to the runtime directory, returning its
// path, or "" if it couldn't.
QString writeScript(const QString &fileName, const QByteArray &script)
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + u'/' + fileName;
    // Earlier versions copied the read-only resource file, and QSaveFile won't replace
    // a file it can't write.
    QFile::remove(path);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(script) != script.size() || !file.commit()) {
        qCWarning(lcAutoHide) << "couldn't write" << path;
        return {};
    }
    return path;
}

QByteArray resource(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Loads a script into KWin under a name and runs it; then() is called once it's running.
void runScript(QObject *context, const QString &path, const QString &name, std::function<void()> then = {})
{
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(kwinScripting(u"loadScript"_s, {path, name})), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [name, then](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<int> reply = *watcher;
        if (reply.isError() || reply.value() < 0) {
            qCWarning(lcAutoHide) << "KWin didn't load" << name << reply.error().message();
            return;
        }
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(u"org.kde.KWin"_s,
                                                                                u"/Scripting/Script"_s + QString::number(reply.value()),
                                                                                u"org.kde.kwin.Script"_s,
                                                                                u"run"_s));
        if (then) {
            then();
        }
    });
}
}

DockManager::~DockManager()
{
    if (m_pointerScriptLoaded) {
        const QString name = m_pointerScriptFollows ? FollowingPointerScript : PointerScript;
        QDBusConnection::sessionBus().call(kwinScripting(u"unloadScript"_s, {name}), QDBus::Block, 500);
    }
}

void DockManager::loadPointerScript()
{
    // Reloaded when ScreenMode changes: the script only follows every pointer movement
    // while it has to (see pointer.js).
    const bool followPointer = m_settings->screenMode() == u"cursor";
    if (m_pointerScriptLoaded && m_pointerScriptFollows == followPointer) {
        return;
    }
    m_pointerScriptLoaded = true;
    m_pointerScriptFollows = followPointer;
    // Unloading both also clears a copy left behind by a Harbour that crashed.
    QDBusConnection::sessionBus().asyncCall(kwinScripting(u"unloadScript"_s, {PointerScript}));
    QDBusConnection::sessionBus().asyncCall(kwinScripting(u"unloadScript"_s, {FollowingPointerScript}));
    const QString name = followPointer ? FollowingPointerScript : PointerScript;

    QByteArray script = resource(u":/harbour/pointer.js"_s);
    if (followPointer) {
        script.replace("const followPointer = false;", "const followPointer = true;");
    }
    if (const QString path = writeScript(u"harbour-pointer.js"_s, script); !path.isEmpty()) {
        runScript(this, path, name);
    }
}

void DockManager::setCursorScreen(const QString &screenName)
{
    if (m_settings->screenMode() != u"cursor") {
        return;
    }
    for (const auto &[screen, dock] : m_docks) {
        if (screen->name() == screenName) {
            setActive(dock.get());
            return;
        }
    }
}

void DockManager::setPointerAtEdge(const QString &screenName, bool atEdge)
{
    for (const auto &[screen, dock] : m_docks) {
        dock->autoHide()->pointerAtScreenEdge(atEdge && screen->name() == screenName);
    }
}

void DockManager::queryCursor()
{
    if (m_cursorScript.isEmpty()) {
        m_cursorScript = writeScript(u"harbour-cursor.js"_s, resource(u":/harbour/cursor.js"_s));
        if (m_cursorScript.isEmpty()) {
            return;
        }
    }
    // A fresh name each time: KWin takes a moment to let go of an unloaded script.
    const QString name = u"harbour-cursor-"_s + QString::number(++m_cursorQueries);
    runScript(this, m_cursorScript, name, [name] {
        // Not straight away: KWin reads the file on another thread once it's run.
        QTimer::singleShot(2s, [name] {
            QDBusConnection::sessionBus().asyncCall(kwinScripting(u"unloadScript"_s, {name}));
        });
    });
}

void DockManager::setCursorPosition(const QPoint &position)
{
    for (const auto &[screen, dock] : m_docks) {
        dock->pointerAt(position);
    }
}

void DockManager::start()
{
    syncScreens();
}

QScreen *DockManager::primaryScreen() const
{
    return m_outputOrder->primaryScreen();
}

void DockManager::syncScreens()
{
    QList<QScreen *> wanted;
    if (qEnvironmentVariableIsSet("HARBOUR_SCREEN") || m_settings->screenMode() == u"primary") {
        wanted = {primaryScreen()};
    } else {
        wanted = QGuiApplication::screens();
    }

    QList<QScreen *> unwanted;
    for (const auto &[screen, dock] : m_docks) {
        if (!wanted.contains(screen)) {
            unwanted.append(screen);
        }
    }
    for (QScreen *screen : std::as_const(unwanted)) {
        removeDock(screen);
    }
    for (QScreen *screen : std::as_const(wanted)) {
        if (screen && !m_docks.contains(screen)) {
            addDock(screen);
        }
    }

    if (!m_active && !m_docks.empty()) {
        const auto primary = m_docks.find(primaryScreen());
        setActive(primary != m_docks.end() ? primary->second.get() : m_docks.begin()->second.get());
    }
    updateSizeScales();
    loadPointerScript();
}

namespace
{
// Logical pixels per millimetre, along the diagonal so a rotated screen's swapped
// physical size doesn't matter; 0 when the screen doesn't report its size.
qreal density(const QScreen *screen)
{
    const QSizeF mm = screen->physicalSize();
    const QSize px = screen->geometry().size();
    if (mm.width() < 1 || mm.height() < 1) {
        return 0;
    }
    return std::hypot(px.width(), px.height()) / std::hypot(mm.width(), mm.height());
}
}

void DockManager::updateSizeScales()
{
    const QScreen *primary = primaryScreen();
    const qreal reference = primary ? density(primary) : 0;
    for (const auto &[screen, dock] : m_docks) {
        qreal scale = 1;
        const qreal own = density(screen);
        if (m_settings->matchPhysicalSize() && reference > 0 && own > 0) {
            // Rounded, so near-identical screens don't end up a pixel apart.
            scale = std::clamp(std::round(own / reference * 20) / 20, 0.5, 2.0);
        }
        dock->setSizeScale(scale);
    }
}

void DockManager::removeDock(QScreen *screen)
{
    const auto it = m_docks.find(screen);
    if (it == m_docks.end()) {
        return;
    }
    // Hide now, delete once control is back in the event loop: deleting a window
    // from inside Qt's own screen or event handling isn't safe.
    DockWindow *dock = it->second.release();
    m_docks.erase(it);
    if (m_active == dock) {
        m_active = nullptr;
    }
    dock->hide();
    dock->deleteLater();
}

void DockManager::addDock(QScreen *screen)
{
    auto dock = std::make_unique<DockWindow>(m_engine, screen, m_settings, m_shared);
    DockWindow *raw = dock.get();
    raw->setActive(false);

    // Whichever dock the user brings up becomes the active one.
    connect(raw->autoHide(), &AutoHideController::userRevealed, this, [this, raw] {
        setActive(raw);
    });
    connect(raw->autoHide(), &AutoHideController::pointerUnknown, this, &DockManager::queryCursor);

    connect(screen, &QScreen::physicalSizeChanged, this, &DockManager::updateSizeScales);
    connect(screen, &QScreen::geometryChanged, this, &DockManager::updateSizeScales);

    raw->show();
    m_docks.emplace(screen, std::move(dock));
    qCInfo(lcAutoHide) << "dock created on" << screen->name();
}

void DockManager::setActive(DockWindow *dock)
{
    if (m_active == dock) {
        return;
    }
    if (m_active) {
        m_active->setActive(false);
    }
    m_active = dock;
    if (dock) {
        dock->setActive(true);
        qCInfo(lcAutoHide) << "active dock:" << dock->screen()->name();
    }
}
