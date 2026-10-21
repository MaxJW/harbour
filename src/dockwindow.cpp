#include "dockwindow.h"
#include "autohidecontroller.h"
#include "blurbehind.h"
#include "harboursettings.h"
#include "logging.h"
#include "screenedge.h"

#include <LayerShellQt/Window>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QQmlProperty>
#include <QtMath>
#include <QQuickItem>
#include <QScreen>
#include <QTimer>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

namespace
{
// Used until the QML has computed the height it needs.
constexpr int InitialWindowHeight = 240;

// Thin strip along the screen edge that stays in the input region. After KWin
// reveals the dock the pointer sits on the bottom row (or one pixel above, after
// push-back), wherever along the edge it was, so the strip guarantees an enter event.
constexpr int EdgeStripHeight = 4;
// Without the KWin protocol the strip is permanently live, so keep it minimal.
constexpr int FallbackStripHeight = 1;
}

DockWindow::DockWindow(QQmlEngine *engine, QScreen *screen, HarbourSettings *settings, QObject *shared)
    : QQuickView(engine, nullptr)
    , m_settings(settings)
{
    setScreen(screen);
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setResizeMode(QQuickView::SizeRootObjectToView);

    m_layerWindow = LayerShellQt::Window::get(this);
    m_layerWindow->setScope(u"dock"_s);
    m_layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    m_layerWindow->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
                                                            | LayerShellQt::Window::AnchorRight));
    m_layerWindow->setExclusiveZone(-1);
    m_layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    m_layerWindow->setWantsToBeOnActiveScreen(false);
    m_layerWindow->setScreen(screen);

    resize(screen->geometry().width(), InitialWindowHeight);

    // The edge needs the platform window for its native interface.
    create();
    m_edge = new ScreenEdge(this, ScreenEdge::Border::Bottom, this);
    m_blur = new BlurBehind(this, this);
    m_autoHide = new AutoHideController(m_edge, m_settings, this);

    connect(m_autoHide, &AutoHideController::stateChanged, this, &DockWindow::onAutoHideStateChanged);
    connect(m_autoHide, &AutoHideController::hiddenFrameRequested, this, [this] {
        m_wantHiddenFrame = true;
        // The slide's final frame is normally already on its way. Only force one if
        // nothing is pending (e.g. at startup), so hiding costs no extra frame.
        QTimer::singleShot(50ms, this, [this] {
            if (m_wantHiddenFrame) {
                update();
            }
        });
    });
    // Over a fullscreen window the dock has to be in the overlay layer to be seen; it goes
    // back below once hidden, so it never sits over a fullscreen window otherwise.
    const auto updateLayer = [this] {
        const bool overlay = m_autoHide->fullscreen() && m_autoHide->state() != AutoHideController::Hidden;
        m_layerWindow->setLayer(overlay ? LayerShellQt::Window::LayerOverlay : LayerShellQt::Window::LayerTop);
    };
    connect(m_autoHide, &AutoHideController::fullscreenChanged, this, updateLayer);
    connect(m_autoHide, &AutoHideController::stateChanged, this, updateLayer);

    connect(m_autoHide, &AutoHideController::peekingChanged, this, [this] {
        if (m_autoHide->peeking()) {
            // The edge is off and the surface shows again; a hidden frame still pending
            // from before is moot (the peek ending asks for a fresh one).
            m_wantHiddenFrame = false;
            m_hiddenFrameSynced = false;
            m_armed = false;
            m_framesWhileArmed = 0;
        }
    });
    connect(m_edge, &ScreenEdge::attachedChanged, this, &DockWindow::updateInputRegion);

    connect(this, &QQuickWindow::afterSynchronizing, this, [this] {
        // GUI thread is blocked here, so this frame carries the state as of the request.
        if (m_wantHiddenFrame.exchange(false)) {
            m_hiddenFrameSynced = true;
        }
    }, Qt::DirectConnection);
    connect(this, &QQuickWindow::frameSwapped, this, &DockWindow::onFrameSwapped, Qt::DirectConnection);

    connect(m_settings, &HarbourSettings::autoHideChanged, this, &DockWindow::updateExclusiveZone);
    connect(screen, &QScreen::geometryChanged, this, &DockWindow::updateGeometry);

    setInitialProperties({
        {u"autoHide"_s, QVariant::fromValue(m_autoHide)},
        {u"dockWindow"_s, QVariant::fromValue(this)},
        {u"shared"_s, QVariant::fromValue(shared)},
    });
    loadFromModule("Harbour", "Dock");
    if (status() == QQuickView::Error) {
        for (const auto &error : errors()) {
            qCritical() << error;
        }
        return;
    }

    QQmlProperty(rootObject(), u"inputRect"_s).connectNotifySignal(this, SLOT(updateInputRegion()));
    QQmlProperty(rootObject(), u"windowHeight"_s).connectNotifySignal(this, SLOT(updateGeometry()));
    QQmlProperty(rootObject(), u"reservedHeight"_s).connectNotifySignal(this, SLOT(updateExclusiveZone()));
    updateGeometry();
    updateExclusiveZone();

    m_vsyncMs = 1000.0 / screen->refreshRate();
    qCInfo(lcFrames) << "screen" << screen->name() << screen->geometry() << screen->refreshRate() << "Hz,"
                     << "graphics API" << rendererInterface()->graphicsApi();
}

DockWindow::~DockWindow() = default;

QPoint DockWindow::globalOrigin() const
{
    // Anchored to the bottom and both sides of its screen, so this is exact even
    // though Wayland never tells clients where their surfaces are.
    const QRect screenRect = screen()->geometry();
    return QPoint(screenRect.x(), screenRect.y() + screenRect.height() - height());
}

void DockWindow::updateGeometry()
{
    const int wanted = rootObject() ? qCeil(rootObject()->property("windowHeight").toReal()) : InitialWindowHeight;
    const QSize size(screen()->geometry().width(), wanted > 0 ? wanted : InitialWindowHeight);
    if (size != this->size()) {
        resize(size);
    }
    // Wayland never reports surface positions, but Qt maps popup (menu) positions
    // through the window's global position, so give it the real one.
    setPosition(globalOrigin());
    updateInputRegion();
    Q_EMIT globalOriginChanged();
}

void DockWindow::setBlurRect(const QRectF &rect)
{
    if (rect != m_blurRect) {
        m_blurRect = rect;
        applyBlur();
        Q_EMIT blurChanged();
    }
}

void DockWindow::setBlurRadius(qreal radius)
{
    if (radius != m_blurRadius) {
        m_blurRadius = radius;
        applyBlur();
        Q_EMIT blurChanged();
    }
}

void DockWindow::applyBlur()
{
    // Set while QML animates the frame, so it lands in the same commit as the plate.
    // The compositor clips the region to the surface; fully off-surface means no blur.
    // (Dock.qml keeps the region still during slides; see blurSettled there.)
    QRect rect = m_blurRect.toRect();
    if (!rect.intersects(QRect(QPoint(0, 0), size()))) {
        rect = QRect();
    }
    if (m_blur->setRegion(rect, qRound(m_blurRadius))) {
        // Mid-animation a frame is coming anyway; at rest this commits the change.
        update();
    }
}

void DockWindow::updateExclusiveZone()
{
    // Auto-hiding docks float over windows; a permanent one keeps maximised windows clear of it.
    const int reserved = rootObject() ? qCeil(rootObject()->property("reservedHeight").toReal()) : 0;
    // Only a dock that is permanently up reserves space; one that is merely up because
    // nothing is in its way must not push windows aside (they'd never cover it).
    const bool permanent = !m_settings->autoHide() && m_active;
    m_layerWindow->setExclusiveZone(permanent ? reserved : -1);
}

void DockWindow::setSizeScale(qreal scale)
{
    if (!qFuzzyCompare(scale, m_sizeScale)) {
        m_sizeScale = scale;
        qCInfo(lcAutoHide) << "size scale on" << screen()->name() << scale;
        Q_EMIT sizeScaleChanged();
    }
}

void DockWindow::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    m_autoHide->setActive(active);
    updateExclusiveZone();
    Q_EMIT activeChanged();
}

bool DockWindow::event(QEvent *event)
{
    const bool result = QQuickView::event(event);

    switch (event->type()) {
    case QEvent::Expose:
        if (isExposed()) {
            m_autoHide->surfaceExposed();
        }
        break;
    case QEvent::Enter:
    case QEvent::DragEnter:
        m_autoHide->pointerEntered();
        break;
    case QEvent::Leave:
    case QEvent::DragLeave:
        m_autoHide->pointerLeft();
        break;
    default:
        break;
    }
    return result;
}

void DockWindow::pointerAt(const QPoint &position)
{
    const QPoint local = position - globalOrigin();
    if (!mask().contains(local)) {
        return;
    }
    m_autoHide->pointerFound();
    // Qt Quick's hover (magnification, the name label) also only learns where the pointer
    // is from events: give it the ones Wayland didn't send. The next real movement brings
    // a real enter, and leaving brings a leave as usual.
    QEnterEvent enter(local, local, position);
    QCoreApplication::sendEvent(this, &enter);
    QMouseEvent move(QEvent::MouseMove, local, local, position, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(this, &move);
}

void DockWindow::updateInputRegion()
{
    const int stripHeight = m_edge->isAttached() ? EdgeStripHeight : FallbackStripHeight;
    QRegion region(0, height() - stripHeight, width(), stripHeight);

    if (m_autoHide->state() != AutoHideController::Hidden && rootObject()) {
        region += rootObject()->property("inputRect").toRectF().toAlignedRect();
    }
    setMask(region);
}

void DockWindow::onFrameSwapped()
{
    const bool lateFrames = lcFrames().isDebugEnabled();
    if (m_recordFrames || lateFrames) {
        const auto now = std::chrono::steady_clock::now();
        QMutexLocker locker(&m_frameMutex);
        if (m_recordFrames) {
            m_frameTimes.push_back(now);
        }
        // Any frame that follows another closely is part of an animation; report the
        // ones that missed a vsync.
        if (lateFrames && m_lastSwap) {
            const double interval = std::chrono::duration<double, std::milli>(now - *m_lastSwap).count();
            if (interval < 50 && interval > m_vsyncMs * 1.5) {
                qCDebug(lcFrames) << "late frame" << interval << "ms";
            }
        }
        m_lastSwap = now;
    }

    if (!m_firstFramePresented.exchange(true)) {
        QMetaObject::invokeMethod(m_autoHide, &AutoHideController::start, Qt::QueuedConnection);
        // The blur region may have been set before the surface existed.
        QMetaObject::invokeMethod(this, &DockWindow::applyBlur, Qt::QueuedConnection);
    }
    if (m_armed) {
        ++m_framesWhileArmed;
        qCDebug(lcFrames) << "frame presented while the edge is armed";
    }
    if (m_hiddenFrameSynced.exchange(false)) {
        // From here on KWin hides the surface; any further frame is waste.
        m_armed = true;
        QMetaObject::invokeMethod(m_autoHide, &AutoHideController::hiddenFramePresented, Qt::QueuedConnection);
    }
}

void DockWindow::onAutoHideStateChanged()
{
    updateInputRegion();

    switch (m_autoHide->state()) {
    case AutoHideController::Revealing:
    case AutoHideController::Hiding: {
        if (m_armed.exchange(false)) {
            // One frame is expected: Qt re-renders when KWin shows the surface again.
            if (const int frames = m_framesWhileArmed.exchange(0); frames > 1) {
                qCWarning(lcFrames) << "rendered" << frames - 1 << "frames while hidden";
            }
        }
        QMutexLocker locker(&m_frameMutex);
        m_frameTimes.clear();
        // Timed only while sliding (and only when someone reads the result): while the
        // dock stays up, frames would otherwise pile up for as long as it does.
        m_recordFrames = lcFrames().isInfoEnabled();
        break;
    }
    case AutoHideController::Shown:
        m_recordFrames = false;
        reportFrameStats("reveal");
        break;
    case AutoHideController::Hidden:
        m_recordFrames = false;
        reportFrameStats("hide");
        break;
    }
}

void DockWindow::reportFrameStats(const char *what)
{
    if (!lcFrames().isInfoEnabled()) {
        return;
    }
    std::vector<std::chrono::steady_clock::time_point> times;
    {
        QMutexLocker locker(&m_frameMutex);
        times.swap(m_frameTimes);
    }
    if (times.size() < 2) {
        qCInfo(lcFrames) << what << ": fewer than two frames";
        return;
    }

    const double period = 1000.0 / screen()->refreshRate();
    double longest = 0;
    int late = 0;
    for (size_t i = 1; i < times.size(); ++i) {
        const double interval = std::chrono::duration<double, std::milli>(times[i] - times[i - 1]).count();
        longest = std::max(longest, interval);
        if (interval > period * 1.5) {
            ++late;
        }
    }
    const double total = std::chrono::duration<double, std::milli>(times.back() - times.front()).count();
    qCInfo(lcFrames).nospace() << what << ": " << times.size() << " frames in " << qRound(total) << " ms, mean "
                               << total / (times.size() - 1) << " ms, longest " << longest << " ms, late " << late
                               << " (vsync " << period << " ms)";
}
