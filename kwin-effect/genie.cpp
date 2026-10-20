/*
    SPDX-FileCopyrightText: 2026 Harbour contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "genie.h"

#include "effect/effecthandler.h"
#include "input.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <cmath>

using namespace std::chrono_literals;
using namespace KWin;

namespace Harbour
{

namespace
{
QRectF toQRectF(const RectF &r)
{
    return QRectF(r.x(), r.y(), r.width(), r.height());
}

double smoothstep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

// Maps screen space to a frame where the icon always lies "below" the window, so
// the animation maths is written once. Canonical x runs along the dock, y towards it.
struct Frame {
    enum class Kind {
        Identity, // icon below
        FlipY, // icon above
        ToLeft, // icon to the left
        ToRight, // icon to the right
    } kind;

    QPointF toCanon(const QPointF &p) const
    {
        switch (kind) {
        case Kind::Identity:
            return p;
        case Kind::FlipY:
            return QPointF(p.x(), -p.y());
        case Kind::ToLeft:
            return QPointF(p.y(), -p.x());
        case Kind::ToRight:
            return QPointF(p.y(), p.x());
        }
        return p;
    }

    QPointF fromCanon(const QPointF &p) const
    {
        switch (kind) {
        case Kind::Identity:
            return p;
        case Kind::FlipY:
            return QPointF(p.x(), -p.y());
        case Kind::ToLeft:
            return QPointF(-p.y(), p.x());
        case Kind::ToRight:
            return QPointF(p.y(), p.x());
        }
        return p;
    }

    QRectF toCanon(const QRectF &r) const
    {
        return QRectF(toCanon(r.topLeft()), toCanon(r.bottomRight())).normalized();
    }
};
}

GenieEffect::GenieEffect()
{
    reconfigure(ReconfigureAll);
    connect(effects, &EffectsHandler::windowAdded, this, &GenieEffect::addWindow);
    connect(effects, &EffectsHandler::windowDeleted, this, [this](EffectWindow *w) {
        m_animations.remove(w);
    });
    const auto windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        addWindow(window);
    }
    setVertexSnappingMode(RenderGeometry::VertexSnappingMode::None);
}

bool GenieEffect::supported()
{
    return OffscreenEffect::supported() && effects->animationsSupported();
}

void GenieEffect::reconfigure(ReconfigureFlags)
{
    const KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-harbour_genie"));
    const QString style = group.readEntry("Style", QStringLiteral("Genie"));
    m_style = style == QLatin1String("Scale") ? Style::Scale : style == QLatin1String("Suck") ? Style::Suck : Style::Genie;

    // ⚠ Default durations are first guesses at macOS's; calibrate against a Mac.
    const auto fallback = m_style == Style::Scale ? 280ms : 480ms;
    const int configured = group.readEntry("Duration", 0);
    m_duration = animationTime(configured > 0 ? std::chrono::milliseconds(configured) : fallback);
}

void GenieEffect::addWindow(EffectWindow *w)
{
    connect(w, &EffectWindow::minimizedChanged, this, [this, w]() {
        start(w, w->isMinimized());
    });
}

void GenieEffect::start(EffectWindow *w, bool minimizing)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    Animation &animation = m_animations[w];
    if (animation.timeLine.running()) {
        animation.timeLine.toggleDirection();
    } else {
        animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
        animation.timeLine.setDirection(minimizing ? TimeLine::Forward : TimeLine::Backward);
        // Shift held: slow motion, as on macOS.
        const bool slow = input()->keyboardModifiers() & Qt::ShiftModifier;
        animation.timeLine.setDuration(slow ? m_duration * 5 : m_duration);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
    }

    redirect(w);
    effects->addRepaintFull();
}

void GenieEffect::prePaintScreen(ScreenPrePaintData &data)
{
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(data);
}

void GenieEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    auto it = m_animations.find(w);
    if (it != m_animations.end()) {
        it->timeLine.advance(view);
        data.setTransformed();
    }
    effects->prePaintWindow(view, w, data);
}

void GenieEffect::postPaintScreen()
{
    for (auto it = m_animations.begin(); it != m_animations.end();) {
        if (it->timeLine.done()) {
            unredirect(it.key());
            it = m_animations.erase(it);
        } else {
            ++it;
        }
    }
    effects->addRepaintFull();
    effects->postPaintScreen();
}

bool GenieEffect::isActive() const
{
    return !m_animations.isEmpty();
}

QRectF GenieEffect::targetRect(EffectWindow *w, Edge *edge) const
{
    const QRectF geo = toQRectF(w->frameGeometry());
    QRectF icon = toQRectF(w->iconGeometry());

    if (!icon.isValid()) {
        // No dock icon: shrink towards the bottom centre of the window's screen.
        const QRectF screen = toQRectF(effects->clientArea(ScreenArea, w));
        *edge = Edge::Bottom;
        return QRectF(screen.center().x() - 24, screen.bottom() - 48, 48, 48);
    }

    // Find the dock holding the icon to learn which edge it's on.
    EffectWindow *dock = nullptr;
    const auto stackingOrder = effects->stackingOrder();
    for (EffectWindow *window : stackingOrder) {
        if (window->isDock() && toQRectF(window->frameGeometry()).intersects(icon)) {
            dock = window;
            break;
        }
    }

    const QRectF screen = toQRectF(effects->clientArea(ScreenArea, w));
    const bool horizontal = dock ? dock->width() >= dock->height() : true;
    if (horizontal) {
        *edge = icon.center().y() >= screen.center().y() ? Edge::Bottom : Edge::Top;
    } else {
        *edge = icon.center().x() <= screen.center().x() ? Edge::Left : Edge::Right;
    }

    // An auto-hidden dock: aim just past the screen edge, where the dock lives.
    if (dock && dock->isHidden()) {
        const QRectF dockScreen = toQRectF(effects->clientArea(ScreenArea, dock));
        switch (*edge) {
        case Edge::Bottom:
            icon.moveTop(dockScreen.bottom());
            break;
        case Edge::Top:
            icon.moveBottom(dockScreen.top());
            break;
        case Edge::Left:
            icon.moveRight(dockScreen.left());
            break;
        case Edge::Right:
            icon.moveLeft(dockScreen.right());
            break;
        }
    }
    Q_UNUSED(geo)
    return icon;
}

void GenieEffect::apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    Q_UNUSED(mask)
    const auto it = m_animations.constFind(w);
    if (it == m_animations.constEnd()) {
        return;
    }

    // 0 = normal, 1 = fully minimised.
    const double p = it->timeLine.value();
    const QRectF geo = toQRectF(w->frameGeometry());
    Edge edge;
    const QRectF icon = targetRect(w, &edge);

    // The last stretch fades out, so the window never visibly lands on top of the icon.
    data.multiplyOpacity(1.0 - smoothstep(0.85, 1.0, p));

    if (m_style == Style::Scale) {
        const double e = smoothstep(0, 1, p);
        for (WindowQuad &quad : quads) {
            for (int i = 0; i < 4; ++i) {
                const double u = quad[i].x() / geo.width();
                const double v = quad[i].y() / geo.height();
                const QPointF from(geo.x() + quad[i].x(), geo.y() + quad[i].y());
                const QPointF to(icon.x() + u * icon.width(), icon.y() + v * icon.height());
                const QPointF pos = from + (to - from) * e;
                quad[i].move(pos.x() - geo.x(), pos.y() - geo.y());
            }
        }
        return;
    }

    Frame frame;
    switch (edge) {
    case Edge::Bottom:
        frame.kind = Frame::Kind::Identity;
        break;
    case Edge::Top:
        frame.kind = Frame::Kind::FlipY;
        break;
    case Edge::Left:
        frame.kind = Frame::Kind::ToLeft;
        break;
    case Edge::Right:
        frame.kind = Frame::Kind::ToRight;
        break;
    }

    const QRectF win = frame.toCanon(geo);
    QRectF target = frame.toCanon(icon);
    if (m_style == Style::Suck) {
        // Suck funnels into a narrow neck at the icon's centre.
        target = QRectF(target.center().x() - target.width() * 0.08, target.top(), target.width() * 0.16, target.height());
    }

    // Stage weights. Genie bends first, then slides; Suck does both at once.
    const double bend = m_style == Style::Suck ? smoothstep(0.0, 0.7, p) : smoothstep(0.0, 0.45, p);
    const double slide = m_style == Style::Suck ? smoothstep(0.1, 1.0, p) : smoothstep(0.28, 1.0, p);

    const double funnelTop = win.top();
    const double funnelBottom = std::max(target.top(), win.top() + 1);
    const double gap = std::max(0.0, target.top() - win.bottom()); // window bottom → icon
    const double squeeze = std::max(target.height(), 1.0) * 0.9;

    // Quads are subdivided along the dock axis, finely enough for smooth S-curves.
    const bool alongY = frame.kind == Frame::Kind::Identity || frame.kind == Frame::Kind::FlipY;
    quads = alongY ? quads.makeRegularGrid(4, 96) : quads.makeRegularGrid(96, 4);

    for (WindowQuad &quad : quads) {
        for (int i = 0; i < 4; ++i) {
            const QPointF canon = frame.toCanon(QPointF(geo.x() + quad[i].x(), geo.y() + quad[i].y()));
            const double u = (canon.x() - win.left()) / win.width();
            const double ly = canon.y() - win.top();
            const double v = ly / win.height();

            // Stretch the lower part of the window down to the icon, then slide it all in.
            double y = win.top() + ly + gap * bend * v * v + (target.top() - win.top()) * slide;
            if (y > target.top()) {
                // Rows reaching the icon get squeezed into it, ever more tightly.
                y = target.top() + target.height() * (1.0 - std::exp(-(y - target.top()) / squeeze));
            }

            // The funnel: S-curves from the window's sides to the icon's, fixed in space.
            const double t = std::clamp((y - funnelTop) / (funnelBottom - funnelTop), 0.0, 1.0);
            const double curve = (1.0 - std::cos(M_PI * t)) / 2.0 * bend;
            const double left = win.left() + (target.left() - win.left()) * curve;
            const double right = win.right() + (target.right() - win.right()) * curve;
            const double x = left + (right - left) * u;

            const QPointF screen = frame.fromCanon(QPointF(x, y));
            quad[i].move(screen.x() - geo.x(), screen.y() - geo.y());
        }
    }
}

} // namespace Harbour

#include "moc_genie.cpp"
