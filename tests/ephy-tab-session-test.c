/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "config.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"
#include "ephy-search-engine-manager.h"
#include "ephy-settings.h"
#include "ephy-tab-session.h"
#include "ephy-shell.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>

#define HTML_STRING "testing-ephy-web-view"
#define SERVER_PORT 12321

static void
test_ephy_tab_session_create (void)
{
	EphyTabSession *one = ephy_tab_session_new_from_container ("hello"),
	               *two = ephy_tab_session_new_from_container ("uuid-uuid-uuid-uuid-uuid"),
				   *three = ephy_tab_session_new_from_container ("0"),
				   *four;
	
	g_assert (ephy_tab_session_get_running_sessions_count () == 3);
	g_object_unref (three);
	g_assert (ephy_tab_session_get_running_sessions_count () == 2);
	g_object_unref (two);
	g_assert (ephy_tab_session_get_running_sessions_count () == 1);
	g_object_unref (one);
	g_assert (ephy_tab_session_get_running_sessions_count () == 0);
	
	four = ephy_tab_session_new_from_container (":O");
	g_assert (ephy_tab_session_get_running_sessions_count () == 1);
	g_object_unref (four);
	g_assert (ephy_tab_session_get_running_sessions_count () == 0);
}

int
main (int   argc,
      char *argv[])
{
  int ret;
  /* FIXME: disable AC mode for now for WebView tests because CI doesn't support it. */
  g_setenv ("WEBKIT_DISABLE_COMPOSITING_MODE", "1", FALSE);

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);

  g_test_add_func ("/lib/ephy-tab-session/create",
                   test_ephy_tab_session_create);


  ret = g_test_run ();

  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}
