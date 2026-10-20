#pragma once

#include <QObject>
#include <QPointer>
#include <QWindow>

#include <memory>

class ScreenEdgeManager;
class AutoHideEdge;

/**
 * Binds a layer-shell window to a KWin auto-hide screen edge (kde_screen_edge_v1).
 *
 * While the edge is active KWin hides the surface outright (it is not painted and
 * receives no input) and shows it again when the pointer pushes against the edge.
 * KWin sends no event when that happens; the dock notices through pointer enter.
 *
 * KWin only honours the request once the layer surface is mapped, so attach()
 * must be called after the first frame has been presented.
 */
class ScreenEdge : public QObject
{
    Q_OBJECT

public:
    enum class Border : uint32_t {
        Top = 1,
        Bottom = 2,
        Left = 3,
        Right = 4,
    };

    ScreenEdge(QWindow *window, Border border, QObject *parent = nullptr);
    ~ScreenEdge() override;

    /// Whether the compositor offers kde_screen_edge_manager_v1 at all.
    bool isSupported() const;
    /// Whether an edge object is bound to the window's current surface.
    bool isAttached() const;

    /// Create the edge object. Call once the surface is mapped.
    void attach();

    /// Hide the surface until the edge is triggered.
    void activate();
    /// Show the surface and stop listening to the edge.
    void deactivate();

Q_SIGNALS:
    void attachedChanged();

private:
    void detach();

    QPointer<QWindow> m_window;
    Border m_border;
    std::unique_ptr<ScreenEdgeManager> m_manager;
    std::unique_ptr<AutoHideEdge> m_edge;
};
