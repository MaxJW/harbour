#include "blurbehind.h"

#include "qwayland-blur.h"
#include "qwayland-ext-background-effect-v1.h"

#include <QGuiApplication>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <qpa/qplatformwindow_p.h>

#include <wayland-client-protocol.h>

#include <cmath>

class BackgroundEffectManager : public QWaylandClientExtensionTemplate<BackgroundEffectManager>,
                                public QtWayland::ext_background_effect_manager_v1
{
public:
    BackgroundEffectManager()
        : QWaylandClientExtensionTemplate<BackgroundEffectManager>(1)
    {
        initialize();
    }

    ~BackgroundEffectManager() override
    {
        if (isActive()) {
            destroy();
        }
    }
};

class BackgroundEffect : public QtWayland::ext_background_effect_surface_v1
{
public:
    using QtWayland::ext_background_effect_surface_v1::ext_background_effect_surface_v1;

    ~BackgroundEffect() override
    {
        destroy();
    }
};

class KdeBlurManager : public QWaylandClientExtensionTemplate<KdeBlurManager>, public QtWayland::org_kde_kwin_blur_manager
{
public:
    KdeBlurManager()
        : QWaylandClientExtensionTemplate<KdeBlurManager>(1)
    {
        initialize();
    }
};

class KdeBlur : public QtWayland::org_kde_kwin_blur
{
public:
    using QtWayland::org_kde_kwin_blur::org_kde_kwin_blur;

    ~KdeBlur() override
    {
        release();
    }
};

namespace
{
// wl_region only knows rectangles, so build the rounded corners from one strip per
// row of the corner arc (merged where neighbouring rows have the same inset).
void addRoundedRect(wl_region *region, const QRect &rect, int radius)
{
    radius = std::min({radius, rect.width() / 2, rect.height() / 2});
    if (radius <= 0) {
        wl_region_add(region, rect.x(), rect.y(), rect.width(), rect.height());
        return;
    }

    auto insetForRow = [radius](int row) {
        const double dy = radius - row - 0.5;
        return int(std::round(radius - std::sqrt(std::max(0.0, double(radius) * radius - dy * dy))));
    };

    int row = 0;
    while (row < radius) {
        const int inset = insetForRow(row);
        int span = 1;
        while (row + span < radius && insetForRow(row + span) == inset) {
            ++span;
        }
        const int width = rect.width() - 2 * inset;
        wl_region_add(region, rect.x() + inset, rect.y() + row, width, span);
        wl_region_add(region, rect.x() + inset, rect.y() + rect.height() - row - span, width, span);
        row += span;
    }
    wl_region_add(region, rect.x(), rect.y() + radius, rect.width(), rect.height() - 2 * radius);
}
}

BlurBehind::BlurBehind(QWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_manager(std::make_unique<BackgroundEffectManager>())
    , m_kdeManager(std::make_unique<KdeBlurManager>())
{
    if (auto waylandWindow = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        connect(waylandWindow, &QNativeInterface::Private::QWaylandWindow::surfaceDestroyed, this, &BlurBehind::reset);
    }
}

BlurBehind::~BlurBehind() = default;

void BlurBehind::reset()
{
    m_effect.reset();
    m_kdeBlur.reset();
    m_applied = false;
}

bool BlurBehind::setRegion(const QRect &rect, int radius)
{
    if (m_applied && rect == m_rect && radius == m_radius) {
        return false;
    }
    if (!m_window) {
        return false;
    }
    auto waylandWindow = m_window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandWindow || !waylandWindow->surface() || !waylandApp) {
        return false;
    }

    wl_region *region = nullptr;
    if (m_manager->isActive()) {
        if (!m_effect) {
            m_effect = std::make_unique<BackgroundEffect>(m_manager->get_background_effect(waylandWindow->surface()));
        }
        // NULL removes the effect here.
        if (!rect.isEmpty()) {
            region = wl_compositor_create_region(waylandApp->compositor());
            addRoundedRect(region, rect, radius);
        }
        m_effect->set_blur_region(region);
    } else if (m_kdeManager->isActive()) {
        if (!m_kdeBlur) {
            m_kdeBlur = std::make_unique<KdeBlur>(m_kdeManager->create(waylandWindow->surface()));
        }
        // NULL means "blur everything" here, so "off" is an empty region.
        region = wl_compositor_create_region(waylandApp->compositor());
        if (!rect.isEmpty()) {
            addRoundedRect(region, rect, radius);
        }
        m_kdeBlur->set_region(region);
        m_kdeBlur->commit();
    } else {
        return false;
    }

    if (region) {
        wl_region_destroy(region);
    }
    m_rect = rect;
    m_radius = radius;
    m_applied = true;
    return true;
}
