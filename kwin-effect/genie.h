/*
    SPDX-FileCopyrightText: 2026 Harbour contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effectwindow.h"
#include "effect/offscreeneffect.h"
#include "effect/timeline.h"

#include <QRectF>

namespace Harbour
{

/**
 * macOS-style minimise animations aimed at the window's dock icon (the geometry
 * docks publish through the task manager): Genie, Scale and Suck.
 *
 * Genie runs in two overlapping stages, like macOS: first the window's sides bend
 * into an S-shaped funnel that narrows to the icon's width while its bottom is
 * stretched towards the dock, then the content slides down the funnel and is
 * squeezed into the icon. Holding Shift when the animation starts plays it in
 * slow motion.
 *
 * Settings live in kwinrc, group [Effect-harbour_genie]:
 *   Style = Genie | Scale | Suck,  Duration = ms (0 = style default)
 */
class GenieEffect : public KWin::OffscreenEffect
{
    Q_OBJECT

public:
    enum class Style {
        Genie,
        Scale,
        Suck,
    };

    GenieEffect();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(KWin::ScreenPrePaintData &data) override;
    void prePaintWindow(KWin::RenderView *view, KWin::EffectWindow *w, KWin::WindowPrePaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 50; // same slot as Magic Lamp
    }

    static bool supported();

protected:
    void apply(KWin::EffectWindow *window, int mask, KWin::WindowPaintData &data, KWin::WindowQuadList &quads) override;

private:
    struct Animation {
        KWin::EffectWindowVisibleRef visibleRef;
        KWin::TimeLine timeLine;
    };

    // Which screen edge the icon lies towards, seen from the window.
    enum class Edge {
        Top,
        Bottom,
        Left,
        Right,
    };

    void addWindow(KWin::EffectWindow *w);
    void start(KWin::EffectWindow *w, bool minimizing);
    QRectF targetRect(KWin::EffectWindow *w, Edge *edge) const;

    Style m_style = Style::Genie;
    std::chrono::milliseconds m_duration;
    QHash<KWin::EffectWindow *, Animation> m_animations;
};

} // namespace Harbour
