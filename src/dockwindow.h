#pragma once

#include <QMutex>
#include <QQuickView>
#include <QtQmlIntegration/qqmlintegration.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <vector>

class AutoHideController;
class BlurBehind;
class HarbourSettings;
class ScreenEdge;

namespace LayerShellQt
{
class Window;
}

/**
 * One dock surface on one screen: a full-width, transparent layer-shell window
 * anchored to the bottom edge. Only the input region (the dock itself plus a thin
 * strip along the edge) accepts the pointer; everything else passes through.
 */
class DockWindow : public QQuickView
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by Harbour")

    /// Top-left of the window in global (compositor) coordinates.
    Q_PROPERTY(QPoint globalOrigin READ globalOrigin NOTIFY globalOriginChanged)
    /// Area (window coordinates) and corner radius KWin should blur behind.
    Q_PROPERTY(QRectF blurRect READ blurRect WRITE setBlurRect NOTIFY blurChanged)
    Q_PROPERTY(qreal blurRadius READ blurRadius WRITE setBlurRadius NOTIFY blurChanged)
    /// The dock on the screen the user last used: the one that stays up with
    /// auto-hide off, and whose icons minimise animations aim at.
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    /// How much bigger or smaller than set the dock is drawn on this screen, so it is
    /// the same physical size as on the primary screen (see DockManager).
    Q_PROPERTY(qreal sizeScale READ sizeScale NOTIFY sizeScaleChanged)

public:
    /// `shared` is the DockModel instance all docks use.
    DockWindow(QQmlEngine *engine, QScreen *screen, HarbourSettings *settings, QObject *shared);
    ~DockWindow() override;

    QPoint globalOrigin() const;

    AutoHideController *autoHide() const { return m_autoHide; }
    bool isActive() const { return m_active; }
    void setActive(bool active);

    QRectF blurRect() const { return m_blurRect; }
    void setBlurRect(const QRectF &rect);
    qreal blurRadius() const { return m_blurRadius; }
    void setBlurRadius(qreal radius);

    qreal sizeScale() const { return m_sizeScale; }
    void setSizeScale(qreal scale);

Q_SIGNALS:
    void globalOriginChanged();
    void blurChanged();
    void activeChanged();
    void sizeScaleChanged();

protected:
    bool event(QEvent *event) override;

private Q_SLOTS:
    void updateInputRegion();
    void updateGeometry();
    void updateExclusiveZone();

private:
    void applyBlur();
    void onFrameSwapped(); // render thread
    void onAutoHideStateChanged();
    void reportFrameStats(const char *what);

    HarbourSettings *m_settings;
    LayerShellQt::Window *m_layerWindow = nullptr;
    ScreenEdge *m_edge = nullptr;
    BlurBehind *m_blur = nullptr;
    QRectF m_blurRect;
    qreal m_blurRadius = 0;
    qreal m_sizeScale = 1;
    AutoHideController *m_autoHide = nullptr;
    bool m_active = true;

    // Set from the GUI thread, consumed on the render thread.
    std::atomic_bool m_firstFramePresented = false;
    std::atomic_bool m_wantHiddenFrame = false;
    std::atomic_bool m_hiddenFrameSynced = false;

    QMutex m_frameMutex;
    std::vector<std::chrono::steady_clock::time_point> m_frameTimes;
    std::atomic_bool m_recordFrames = false; // between the start and end of a slide
    std::optional<std::chrono::steady_clock::time_point> m_lastSwap;
    double m_vsyncMs = 1000.0 / 60;
    std::atomic_bool m_armed = false; // KWin has been asked to hide the surface
    std::atomic_int m_framesWhileArmed = 0;
};
