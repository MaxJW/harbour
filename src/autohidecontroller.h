#pragma once

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

    /// Called once the window's first frame is on screen.
    void start();

    void pointerEntered();
    void pointerLeft();
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
    /// The user brought this dock up (see reveal()).
    void userRevealed();
    /// Present one more frame, then call hiddenFramePresented().
    void hiddenFrameRequested();

private:
    /// byUser: brought up by the pointer, an edge, a hold or a shortcut, rather than
    /// by policy (auto-hide off, nothing in the way).
    void reveal(bool byUser = true);
    void hide();
    void setState(State state);
    void setRevealed(bool revealed);
    void maybeScheduleHide();
    void applyAutoHideSetting();

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
    bool m_armed = false; // KWin was asked to hide the surface
    int m_holds = 0;

    QTimer m_revealTimer;
    QTimer m_hideTimer;
};
