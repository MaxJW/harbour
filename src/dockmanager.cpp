#include "dockmanager.h"
#include "autohidecontroller.h"
#include "dockwindow.h"
#include "harboursettings.h"
#include "logging.h"
#include "outputorder.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScreen>

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

DockManager::~DockManager() = default;

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
