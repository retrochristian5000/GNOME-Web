/*
 *  Copyright © 2025 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SIDEBAR (ephy_sidebar_get_type())

G_DECLARE_FINAL_TYPE (EphySidebar, ephy_sidebar, EPHY, SIDEBAR, AdwBin)

void       ephy_sidebar_set_visible_child_name (EphySidebar *self,
                                                const char  *name);

void       ephy_sidebar_reset_state (EphySidebar *self);

G_END_DECLS
