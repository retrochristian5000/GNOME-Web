/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Cleo Menezes Jr. <menezesjr@pm.me>
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ephy-embed.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SCREENSHOT_SESSION (ephy_screenshot_session_get_type ())

G_DECLARE_FINAL_TYPE (EphyScreenshotSession, ephy_screenshot_session, EPHY, SCREENSHOT_SESSION, GObject)

/* Drives a single screenshot interaction: it shows the selection overlay on
 * the page, captures what the user picked and presents the preview dialog.
 * The "finished" signal is emitted exactly once, whatever the outcome.
 */
EphyScreenshotSession *ephy_screenshot_session_new   (EphyWindow *window,
                                                      EphyEmbed  *embed,
                                                      const char *suggested_filename);

void                   ephy_screenshot_session_start (EphyScreenshotSession *self);

G_END_DECLS
