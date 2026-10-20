#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QWindow>

#include <memory>

class BackgroundEffectManager;
class BackgroundEffect;
class KdeBlurManager;
class KdeBlur;

/**
 * Asks the compositor to blur what's behind one rounded rectangle of a window,
 * e.g. the dock's plate.
 *
 * Uses ext_background_effect_v1 (KWin 6.7+), falling back to KDE's older
 * org_kde_kwin_blur. Either way the region is double-buffered surface state that
 * lands with the next commit, so setting it while QML animates a frame puts it in
 * the same commit as that frame: the blur never trails the plate. One effect
 * object is kept and only changed regions are sent.
 */
class BlurBehind : public QObject
{
    Q_OBJECT

public:
    explicit BlurBehind(QWindow *window, QObject *parent = nullptr);
    ~BlurBehind() override;

    /// An empty rect turns the blur off. Returns whether anything was sent, in
    /// which case the window needs to commit a frame for it to take effect.
    bool setRegion(const QRect &rect, int radius);

private:
    void reset();

    QPointer<QWindow> m_window;
    std::unique_ptr<BackgroundEffectManager> m_manager;
    std::unique_ptr<BackgroundEffect> m_effect;
    std::unique_ptr<KdeBlurManager> m_kdeManager;
    std::unique_ptr<KdeBlur> m_kdeBlur;
    QRect m_rect;
    int m_radius = 0;
    bool m_applied = false;
};
