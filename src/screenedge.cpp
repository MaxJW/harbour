#include "screenedge.h"
#include "logging.h"

#include "qwayland-kde-screen-edge-v1.h"

#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <qpa/qplatformwindow_p.h>

class ScreenEdgeManager : public QWaylandClientExtensionTemplate<ScreenEdgeManager>, public QtWayland::kde_screen_edge_manager_v1
{
public:
    ScreenEdgeManager()
        : QWaylandClientExtensionTemplate<ScreenEdgeManager>(1)
    {
        initialize();
    }

    ~ScreenEdgeManager() override
    {
        if (isActive()) {
            destroy();
        }
    }
};

class AutoHideEdge : public QtWayland::kde_auto_hide_screen_edge_v1
{
public:
    using QtWayland::kde_auto_hide_screen_edge_v1::kde_auto_hide_screen_edge_v1;

    ~AutoHideEdge() override
    {
        destroy();
    }
};

ScreenEdge::ScreenEdge(QWindow *window, Border border, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_border(border)
    , m_manager(std::make_unique<ScreenEdgeManager>())
{
    if (auto waylandWindow = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        // A new surface needs a new edge; the old object refers to a dead surface.
        connect(waylandWindow, &QNativeInterface::Private::QWaylandWindow::surfaceRoleDestroyed, this, &ScreenEdge::detach);
    }
}

ScreenEdge::~ScreenEdge() = default;

bool ScreenEdge::isSupported() const
{
    return m_manager->isActive();
}

bool ScreenEdge::isAttached() const
{
    return m_edge != nullptr;
}

void ScreenEdge::attach()
{
    if (m_edge || !m_window || !m_manager->isActive()) {
        return;
    }
    auto waylandWindow = m_window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    if (!waylandWindow || !waylandWindow->surface()) {
        return;
    }

    m_edge = std::make_unique<AutoHideEdge>(m_manager->get_auto_hide_screen_edge(uint32_t(m_border), waylandWindow->surface()));
    qCInfo(lcAutoHide) << "screen edge attached";
    Q_EMIT attachedChanged();
}

void ScreenEdge::detach()
{
    if (!m_edge) {
        return;
    }
    m_edge.reset();
    qCInfo(lcAutoHide) << "screen edge detached";
    Q_EMIT attachedChanged();
}

void ScreenEdge::activate()
{
    if (m_edge) {
        m_edge->activate();
    }
}

void ScreenEdge::deactivate()
{
    if (m_edge) {
        m_edge->deactivate();
    }
}
