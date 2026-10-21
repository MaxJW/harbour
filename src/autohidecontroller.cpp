#include "autohidecontroller.h"
#include "harboursettings.h"
#include "logging.h"
#include "screenedge.h"

AutoHideController::AutoHideController(ScreenEdge *edge, HarbourSettings *settings, QObject *parent)
    : QObject(parent)
    , m_edge(edge)
    , m_settings(settings)
{
    m_revealTimer.setSingleShot(true);
    connect(&m_revealTimer, &QTimer::timeout, this, [this] {
        reveal();
    });

    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [this] {
        if (!m_pointerInside && m_holds == 0 && autoHides()) {
            hide();
        }
    });

    connect(m_settings, &HarbourSettings::autoHideChanged, this, &AutoHideController::applyAutoHideSetting);
    connect(m_settings, &HarbourSettings::showWhenClearChanged, this, &AutoHideController::applyAutoHideSetting);
    connect(m_settings, &HarbourSettings::screenModeChanged, this, &AutoHideController::applyAutoHideSetting);

    // Over a fullscreen window the pointer has to rest on the bottom row a moment, like
    // KWin's own edge delay, so sweeping past it in a game doesn't bring the dock up.
    m_fullscreenEdgeTimer.setSingleShot(true);
    connect(&m_fullscreenEdgeTimer, &QTimer::timeout, this, [this] {
        if (m_fullscreen && m_state == Hidden) {
            qCInfo(lcAutoHide) << "pushed against the edge over a fullscreen window";
            reveal();
        }
    });

    // Becoming clear settles briefly first, so dragging a window across the dock
    // doesn't flash it up and down. Becoming covered acts at once (hide delay applies).
    m_clearTimer.setSingleShot(true);
    m_clearTimer.setInterval(300);
    connect(&m_clearTimer, &QTimer::timeout, this, [this] {
        m_clear = true;
        applyAutoHideSetting();
    });
}

void AutoHideController::start()
{
    m_started = true;
    m_edge->attach();
    if (!m_edge->isAttached()) {
        qCWarning(lcAutoHide) << "kde_screen_edge_v1 unavailable; falling back to an input strip at the screen edge";
    }
    applyAutoHideSetting();
}

bool AutoHideController::autoHides() const
{
    // A fullscreen window has the screen: keep out of its way until pushed for, even
    // with auto-hide off.
    if (m_fullscreen) {
        return true;
    }
    // Nothing in the way: stay up (on every screen, like an empty desktop should look,
    // or with ScreenMode "cursor" only on the pointer's screen).
    if (m_settings->showWhenClear() && m_clear && (m_active || m_settings->screenMode() != u"cursor")) {
        return false;
    }
    // With auto-hide off, only the active dock stays up; the others wait at their edges.
    return m_settings->autoHide() || !m_active;
}

void AutoHideController::setScreenClear(bool clear)
{
    if (m_screenClear == clear) {
        return;
    }
    m_screenClear = clear;
    qCDebug(lcAutoHide) << "screen clear:" << clear;
    Q_EMIT screenClearChanged();
    if (clear) {
        m_clearTimer.start();
    } else {
        m_clearTimer.stop();
        m_clear = false;
        applyAutoHideSetting();
    }
}

void AutoHideController::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        applyAutoHideSetting();
        updatePeek();
    }
}

void AutoHideController::setFullscreen(bool fullscreen)
{
    if (m_fullscreen == fullscreen) {
        return;
    }
    m_fullscreen = fullscreen;
    qCInfo(lcAutoHide) << (fullscreen ? "fullscreen window on this screen" : "no fullscreen window");
    if (!fullscreen) {
        m_fullscreenEdgeTimer.stop();
    }
    Q_EMIT fullscreenChanged();
    applyAutoHideSetting();
}

void AutoHideController::pointerAtScreenEdge(bool atEdge)
{
    if (!atEdge || !m_fullscreen || m_state != Hidden) {
        m_fullscreenEdgeTimer.stop();
        return;
    }
    if (!m_fullscreenEdgeTimer.isActive()) {
        m_fullscreenEdgeTimer.start(150 + m_settings->revealDelay());
    }
}

void AutoHideController::setPeekRequested(bool requested)
{
    if (m_peekRequested == requested) {
        return;
    }
    m_peekRequested = requested;
    Q_EMIT peekRequestedChanged();
    updatePeek();
}

void AutoHideController::updatePeek()
{
    if (!m_started || m_state != Hidden) {
        return;
    }
    // Only the dock the user last used peeks.
    const bool want = m_peekRequested && m_active;
    if (want && !m_peeking) {
        m_revealTimer.stop();
        m_edge->deactivate();
        m_armed = false;
        setPeeking(true);
    } else if (!want && m_peeking) {
        setPeeking(false);
        // As after a hide: one frame with nothing showing, then the edge goes back on.
        Q_EMIT hiddenFrameRequested();
    }
}

void AutoHideController::setPeeking(bool peeking)
{
    if (m_peeking == peeking) {
        return;
    }
    m_peeking = peeking;
    qCInfo(lcAutoHide) << (peeking ? "peeking" : "peek over");
    Q_EMIT peekingChanged();
}

void AutoHideController::applyAutoHideSetting()
{
    if (!m_started) {
        return;
    }
    if (!autoHides()) {
        if (m_state == Hidden || m_state == Hiding) {
            reveal(false);
        }
        return;
    }
    if (m_state == Hidden) {
        Q_EMIT hiddenFrameRequested();
    } else {
        maybeScheduleHide();
    }
}

void AutoHideController::pointerEntered()
{
    m_pointerInside = true;
    m_hideTimer.stop();
    qCDebug(lcAutoHide) << "pointer entered in" << m_state;

    switch (m_state) {
    case Hidden:
        if (m_peeking) {
            // Pushed against the edge mid-peek (the edge itself is off meanwhile).
            reveal();
        } else {
            // Only reachable once KWin has shown the surface because the edge fired.
            surfaceExposed();
        }
        break;
    case Hiding:
        reveal();
        break;
    case Revealing:
    case Shown:
        // The dock the user is using becomes the active one (it bounces, peeks and
        // gets minimise animations), even if it came up on its own.
        Q_EMIT userRevealed();
        break;
    }
}

void AutoHideController::surfaceExposed()
{
    if (m_state != Hidden || !m_armed) {
        return;
    }
    m_armed = false;
    if (m_settings->revealDelay() > 0) {
        m_revealTimer.start(m_settings->revealDelay());
    } else {
        reveal();
    }
}

void AutoHideController::pointerLeft()
{
    m_pointerInside = false;
    qCDebug(lcAutoHide) << "pointer left in" << m_state;

    if (m_state == Hidden) {
        // Left during the reveal delay: KWin already disarmed the edge, so re-arm it.
        if (m_revealTimer.isActive()) {
            m_revealTimer.stop();
            m_edge->activate();
            m_armed = true;
        }
        return;
    }
    maybeScheduleHide();
}

void AutoHideController::hold()
{
    ++m_holds;
    m_hideTimer.stop();
    if (m_state == Hidden || m_state == Hiding) {
        reveal();
    }
}

void AutoHideController::release()
{
    if (m_holds == 0) {
        qCWarning(lcAutoHide) << "release() without matching hold()";
        return;
    }
    --m_holds;
    maybeScheduleHide();
}

void AutoHideController::slideFinished()
{
    if (m_state == Revealing && m_revealed) {
        setState(Shown);
        maybeScheduleHide();
    } else if (m_state == Hiding && !m_revealed) {
        setState(Hidden);
        if (m_peekRequested && m_active) {
            updatePeek();
        } else {
            Q_EMIT hiddenFrameRequested();
        }
    }
}

void AutoHideController::hiddenFramePresented()
{
    if (m_state != Hidden || m_peeking || !autoHides()) {
        return;
    }
    m_edge->activate();
    m_armed = true;
    qCDebug(lcAutoHide) << "edge armed";
}

void AutoHideController::reveal(bool byUser)
{
    if (byUser) {
        Q_EMIT userRevealed();
    }
    m_revealTimer.stop();
    m_hideTimer.stop();
    // Harmless if KWin already disarmed the edge; needed for reveals that don't come from it.
    m_edge->deactivate();
    m_armed = false;
    setPeeking(false);
    setState(Revealing);
    setRevealed(true);
}

void AutoHideController::hide()
{
    setState(Hiding);
    setRevealed(false);
}

void AutoHideController::maybeScheduleHide()
{
    if ((m_state == Shown || m_state == Revealing) && !m_pointerInside && m_holds == 0 && autoHides()) {
        m_hideTimer.start(m_settings->hideDelay());
    }
}

void AutoHideController::setState(State state)
{
    if (m_state == state) {
        return;
    }
    qCInfo(lcAutoHide) << m_state << "->" << state;
    m_state = state;
    Q_EMIT stateChanged();
}

void AutoHideController::setRevealed(bool revealed)
{
    if (m_revealed == revealed) {
        return;
    }
    m_revealed = revealed;
    Q_EMIT revealedChanged();
}
