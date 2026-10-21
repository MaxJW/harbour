#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QtQmlIntegration/qqmlintegration.h>

class HarbourSettings;
class ScreenEdge;

/**
 * The dock's show/hide state machine.
 *
 *       KWin revealed us (expose / pointer enters)       slide in done
 *   Hidden ───────────────────────────────────▶ Revealing ─────────▶ Shown
 *     ▲                                            ▲  │                │
 *     │ final frame presented,                     │  │ pointer left   │ pointer left
 *     │ edge re-armed                pointer enters │  ▼  + hide delay  │ + hide delay
 *     └──────────────────────────────────────── Hiding ◀──────────────┘
 *                     slide out done
 *
 * The slide itself is animated in QML; it reports back through slideFinished().
 * Before the edge is re-armed (which makes KWin hide the surface) the window
 * presents one frame with the dock fully off-screen, so a later reveal never
 * shows a stale frame. With auto-hide turned off the dock simply stays Shown.
 *
 * Peeking: while Hidden, an app asking for attention briefly pops up out of the
 * screen edge. The edge is disarmed so KWin shows the surface again, QML draws just
 * that icon, and once it's done the edge is re-armed as after a hide. Pushing
 * against the edge meanwhile reveals the dock as usual.
 */
class AutoHideController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by the dock window")

    Q_PROPERTY(bool revealed READ revealed NOTIFY revealedChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    /// No window overlaps the dock on its screen (set from QML). With ShowWhenClear the
    /// dock then stays up instead of auto-hiding.
    Q_PROPERTY(bool screenClear READ screenClear WRITE setScreenClear NOTIFY screenClearChanged)
    /// Something wants attention (set from QML); peek while hidden.
    Q_PROPERTY(bool peekRequested READ peekRequested WRITE setPeekRequested NOTIFY peekRequestedChanged)
    /// Peeking out of the screen edge while hidden.
    Q_PROPERTY(bool peeking READ peeking NOTIFY peekingChanged)
    /// A fullscreen window has this screen (set from QML). The dock stays hidden, and a
    /// push against the bottom edge (reported by the KWin script, since KWin turns
    /// client screen edges off under fullscreen windows) brings it up over the window.
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged)

public:
    enum State {
        Hidden,
        Revealing,
        Shown,
        Hiding,
    };
    Q_ENUM(State)

    AutoHideController(ScreenEdge *edge, HarbourSettings *settings, QObject *parent = nullptr);

    bool revealed() const { return m_revealed; }
    State state() const { return m_state; }

    /// Whether this dock hides when the pointer leaves: always with auto-hide on,
    /// and for inactive docks (other screens) with it off.
    bool autoHides() const;
    /// The active dock is the one on the screen the user last used.
    void setActive(bool active);

    bool screenClear() const { return m_screenClear; }
    void setScreenClear(bool clear);

    bool peekRequested() const { return m_peekRequested; }
    void setPeekRequested(bool requested);
    bool peeking() const { return m_peeking; }

    bool fullscreen() const { return m_fullscreen; }
    void setFullscreen(bool fullscreen);
    /// The pointer is on (or left) this screen's bottom row.
    void pointerAtScreenEdge(bool atEdge);

    /// Called once the window's first frame is on screen.
    void start();

    void pointerEntered();
    void pointerLeft();
    /// The pointer turned out to be over the dock (see pointerUnknown()).
    void pointerFound();
    /// The window was exposed again. While the edge is armed that means KWin revealed it,
    /// which arrives a little before pointer enter and also covers touch edge swipes.
    void surfaceExposed();

    /// Keep the dock shown while something needs it (menu, drag, keyboard focus).
    Q_INVOKABLE void hold();
    Q_INVOKABLE void release();

    /// The QML slide animation reached its target.
    Q_INVOKABLE void slideFinished();

    /// The window presented the frame requested by hiddenFrameRequested().
    void hiddenFramePresented();

Q_SIGNALS:
    void revealedChanged();
    void stateChanged();
    void screenClearChanged();
    void peekRequestedChanged();
    void peekingChanged();
    void fullscreenChanged();
    /// The user brought this dock up (see reveal()).
    void userRevealed();
    /// Present one more frame, then call hiddenFramePresented().
    void hiddenFrameRequested();
    /// A menu or stack closed after taking the pointer, and nothing says where it is
    /// now: ask KWin (answered with pointerFound() if it's over the dock).
    void pointerUnknown();

private:
    /// byUser: brought up by the pointer, an edge, a hold or a shortcut, rather than
    /// by policy (auto-hide off, nothing in the way).
    void reveal(bool byUser = true);
    void hide();
    void setState(State state);
    void setRevealed(bool revealed);
    void maybeScheduleHide();
    void applyAutoHideSetting();
    /// Starts or ends a peek to match the request, if Hidden.
    void updatePeek();
    void setPeeking(bool peeking);
    /// Emits pointerUnknown(), hiding after a grace period unless the pointer is found.
    void askWherePointerIs();

    ScreenEdge *m_edge;
    HarbourSettings *m_settings;
    State m_state = Hidden;
    bool m_started = false;
    bool m_active = true;
    bool m_screenClear = false; // as reported
    bool m_clear = false; // as acted on, after settling
    QTimer m_clearTimer;
    bool m_revealed = false;
    bool m_pointerInside = false;
    bool m_pointerTaken = false; // left while held: a menu or stack took the pointer
    QElapsedTimer m_sinceRelease; // since the last hold was let go
    bool m_armed = false; // KWin was asked to hide the surface
    int m_holds = 0;
    bool m_peekRequested = false;
    bool m_peeking = false;
    bool m_fullscreen = false;
    QTimer m_fullscreenEdgeTimer;

    QTimer m_revealTimer;
    QTimer m_hideTimer;
};
