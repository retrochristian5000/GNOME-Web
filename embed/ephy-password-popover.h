/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Epiphany Contributors
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

#include "ephy-web-view.h"

G_BEGIN_DECLS

void ephy_password_popover_dismiss       (EphyWebView        *view);

void ephy_password_popover_show_users    (EphyWebView        *view,
                                          guint64             frame_id,
                                          const char         *origin,
                                          const char * const *usernames,
                                          const char         *selected_username,
                                          int                 element_x,
                                          int                 element_y,
                                          int                 element_width,
                                          int                 element_height);

void ephy_password_popover_show_generate (EphyWebView        *view,
                                          guint64             frame_id,
                                          int                 element_x,
                                          int                 element_y,
                                          int                 element_width,
                                          int                 element_height);

G_END_DECLS
