#include "dockservice.h"
#include "dockmanager.h"
#include "harboursettings.h"
#include "logging.h"

#include <QDBusConnection>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

DockService::DockService(DockManager *docks, HarbourSettings *settings, QObject *parent)
    : QObject(parent)
    , m_docks(docks)
    , m_settings(settings)
{
}

bool DockService::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(u"dev.harbour.dock"_s)) {
        return false;
    }
    bus.registerObject(u"/Dock"_s, this, QDBusConnection::ExportAllSlots);
    return true;
}

void DockService::preview(const QString &name, const QDBusVariant &value)
{
    // The generated setters emit the change signals the docks bind to.
    if (!m_settings->setProperty(name.toUtf8().constData(), value.variant())) {
        qCWarning(lcAutoHide) << "preview: no setting called" << name;
        return;
    }
    m_docks->revealFor(1500ms);
}

void DockService::endPreview()
{
    m_settings->load();
}

void DockService::reveal(const QString &screen)
{
    m_docks->revealFor(1500ms, screen);
}

void DockService::cursorScreen(const QString &screen)
{
    m_docks->setCursorScreen(screen);
}

void DockService::pointerAtEdge(const QString &screen, bool atEdge)
{
    m_docks->setPointerAtEdge(screen, atEdge);
}
