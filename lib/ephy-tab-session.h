/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2017 Igalia S.L.
 * Copyright © 2018 Jan-Michael Brummer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <webkit/webkit.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TAB_SESSION (ephy_tab_session_get_type())
G_DECLARE_FINAL_TYPE (EphyTabSession, ephy_tab_session, EPHY, TAB_SESSION, GObject)

unsigned int    ephy_tab_session_get_running_sessions_count (void);
void            ephy_tab_session_set_network_session_singleton (WebKitNetworkSession *object);

WebKitNetworkSession *ephy_tab_session_get_network_session (EphyTabSession *self);
const char *ephy_tab_session_get_cache_dir (EphyTabSession *self);
const char *ephy_tab_session_get_profile_dir (EphyTabSession *self);
const char *ephy_tab_session_get_container_uuid (EphyTabSession *self);
EphyTabSession *ephy_tab_session_new_from_container (const char *uuid);
EphyTabSession *ephy_tab_session_new (void);

G_END_DECLS
