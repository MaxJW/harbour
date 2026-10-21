#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <chrono>

#include <map>
#include <memory>

class DockWindow;
class HarbourSettings;
class OutputOrder;
class QQmlEngine;
class QScreen;

/**
 * Keeps one dock window per screen, like macOS's dock following the pointer: each
 * screen has its own auto-hide edge, so the dock comes up wherever the user pushes.
 * The dock last revealed is the active one — with auto-hide off it is the one that
 * stays up (the others wait hidden at their edges), and minimise animations aim at
 * its icons. All docks share one DockModel (tasks, launchers, desktops).
 *
 * With ScreenMode "primary", only Plasma's primary screen gets a dock. With "cursor",
 * the active dock is the one on the screen with the pointer (reported by a small KWin
 * script), and only it stays up.
 */
class DockManager : public QObject
{
    Q_OBJECT

public:
    DockManager(QQmlEngine *engine, HarbourSettings *settings, OutputOrder *outputOrder, QObject *parent = nullptr);
    ~DockManager() override;

    void start();

    /// Shows the dock on the named screen (the active dock if empty) and keeps it up
    /// until `duration` after the last call, then lets it auto-hide as usual.
    void revealFor(std::chrono::milliseconds duration, const QString &screenName = QString());

    /// The pointer is now on this screen (ScreenMode "cursor").
    void setCursorScreen(const QString &screenName);

private:
    void syncScreens();
    void addDock(QScreen *screen);
    void removeDock(QScreen *screen);
    void setActive(DockWindow *dock);
    QScreen *primaryScreen() const;
    /// Gives every dock its size scale (MatchPhysicalSize).
    void updateSizeScales();
    /// Loads or unloads the KWin script that reports the pointer's screen.
    void updateCursorTracking();

    QQmlEngine *m_engine;
    HarbourSettings *m_settings;
    OutputOrder *m_outputOrder;
    QObject *m_shared = nullptr;
    std::map<QScreen *, std::unique_ptr<DockWindow>> m_docks;
    QPointer<DockWindow> m_active;
    bool m_cursorTracking = false;

    // revealFor(): the dock being held up, and when to let go.
    QPointer<DockWindow> m_revealed;
    QTimer m_revealTimer;
};
