/*
    SPDX-FileCopyrightText: 2026 Harbour contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "genie.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(Harbour::GenieEffect, "metadata.json", return Harbour::GenieEffect::supported();)

} // namespace KWin

#include "main.moc"
