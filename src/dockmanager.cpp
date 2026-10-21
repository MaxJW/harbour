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
#include <QScreen>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

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
const auto PointerScript = u"harbour-pointer"_s;

QDBusMessage kwinScripting(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/Scripting"_s, u"org.kde.kwin.Scripting"_s, method);
    message.setArguments(arguments);
    return message;
}
}

DockManager::~DockManager()
{
    if (m_pointerScriptLoaded) {
        QDBusConnection::sessionBus().call(kwinScripting(u"unloadScript"_s, {PointerScript}), QDBus::Block, 500);
    }
}

void DockManager::loadPointerScript()
{
    if (m_pointerScriptLoaded) {
        return;
    }
    m_pointerScriptLoaded = true;
    // Unloading first also clears a copy left behind by a Harbour that crashed.
    QDBusConnection::sessionBus().asyncCall(kwinScripting(u"unloadScript"_s, {PointerScript}));

    // KWin loads scripts from files: copy ours out of the resources.
    const QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + u"/harbour-pointer.js"_s;
    QFile::remove(path);
    if (!QFile::copy(u":/harbour/pointer.js"_s, path)) {
        qCWarning(lcAutoHide) << "couldn't write" << path;
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(kwinScripting(u"loadScript"_s, {path, PointerScript})), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<int> reply = *watcher;
        if (reply.isError() || reply.value() < 0) {
            qCWarning(lcAutoHide) << "KWin didn't load the pointer script:" << reply.error().message();
            return;
        }
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(u"org.kde.KWin"_s,
                                                                                u"/Scripting/Script"_s + QString::number(reply.value()),
                                                                                u"org.kde.kwin.Script"_s,
                                                                                u"run"_s));
    });
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
