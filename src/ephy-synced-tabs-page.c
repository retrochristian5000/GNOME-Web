/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#include "ephy-synced-tabs-page.h"
#include "ephy-firefox-sync-dialog.h"

#include <json-glib/json-glib.h>

#include "ephy-desktop-utils.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-helpers.h"
#include "ephy-shell.h"

struct _EphySyncedTabsPage {
  AdwNavigationPage parent_instance;

  AdwPreferencesGroup *local_tabs_preferences_group;
  AdwPreferencesGroup *tabs_preferences_group;

  EphyOpenTabsManager *manager;
  WebKitFaviconDatabase *database;
  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (EphySyncedTabsPage, ephy_synced_tabs_page, ADW_TYPE_NAVIGATION_PAGE)

typedef enum {
  PROP_OPEN_TABS_MANAGER = 1,
} EphySyncedTabsPageProps;

static GParamSpec *obj_properties[PROP_OPEN_TABS_MANAGER + 1];

typedef struct {
  GtkWidget *row;
  char *url;
} FaviconAsyncData;

static FaviconAsyncData *
favicon_async_data_new (GtkWidget  *row,
                        const char *url)
{
  FaviconAsyncData *data = g_new (FaviconAsyncData, 1);
  data->row = g_object_ref (row);
  data->url = g_strdup (url);
  return data;
}

static void
favicon_async_data_free (FaviconAsyncData *data)
{
  g_object_unref (data->row);
  g_free (data->url);
  g_free (data);
}

static void
tab_row_activated_cb (AdwActionRow *row,
                      gpointer      user_data)
{
  EphySyncedTabsPage *page = EPHY_SYNCED_TABS_PAGE (user_data);
  const char *url;
  EphyShell *shell;
  EphyEmbed *embed;
  GtkWindow *window;

  url = g_object_get_data (G_OBJECT (row), "tab-url");
  if (!url)
    return;

  shell = ephy_shell_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (shell));
  embed = ephy_shell_new_tab (shell, EPHY_WINDOW (window),
                              NULL, EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), url);
  gtk_window_close (GTK_WINDOW (page));
}

static void
favicon_loaded_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  FaviconAsyncData *data = user_data;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GIcon) favicon = NULL;
  g_autoptr (GError) error = NULL;

  texture = webkit_favicon_database_get_favicon_finish (database, result, &error);
  if (!texture && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    favicon_async_data_free (data);
    return;
  }

  favicon = ephy_favicon_get_from_texture_scaled (texture, FAVICON_SIZE, FAVICON_SIZE);
  if (!favicon) {
    const char *icon_name = ephy_get_fallback_favicon_name (data->url, EPHY_FAVICON_TYPE_SHOW_MISSING_PLACEHOLDER);
    if (!icon_name)
      icon_name = "adw-tab-icon-missing-symbolic";
    favicon = g_themed_icon_new (icon_name);
  }

  adw_action_row_add_prefix (ADW_ACTION_ROW (data->row),
                             gtk_image_new_from_gicon (favicon));

  favicon_async_data_free (data);
}

static GtkWidget *
ephy_tab_row_from_data (EphySyncedTabsPage *page,
                        JsonObject         *data,
                        gboolean            is_local)
{
  const char *title = json_object_get_string_member (data, "title");
  JsonArray *url_history = json_object_get_array_member (data, "urlHistory");
  const char *url = json_array_get_string_element (url_history, 0);
  GtkWidget *tab_row;

  tab_row = adw_action_row_new ();
  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (tab_row), FALSE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (tab_row), title);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (tab_row), url);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (tab_row), !is_local);
  adw_action_row_set_subtitle_lines (ADW_ACTION_ROW (tab_row), 1);
  adw_action_row_set_title_lines (ADW_ACTION_ROW (tab_row), 1);
  g_object_set_data_full (G_OBJECT (tab_row), "tab-url", g_strdup (url), g_free);

  if (!is_local)
    g_signal_connect (tab_row, "activated",
                      G_CALLBACK (tab_row_activated_cb), page);

  /* Load favicon asynchronously. */
  webkit_favicon_database_get_favicon (page->database, url,
                                       page->cancellable,
                                       favicon_loaded_cb,
                                       favicon_async_data_new (tab_row, url));
  return g_steal_pointer (&tab_row);
}

static void
ephy_synced_tabs_page_populate_from_record (EphySyncedTabsPage *page,
                                            EphyOpenTabsRecord *record,
                                            gboolean            is_local)
{
  GList *tabs;

  tabs = ephy_open_tabs_record_get_tabs (record);
  if (!is_local && !tabs)
    return;

  if (is_local) {
    for (GList *l = tabs; l && l->data; l = l->next) {
      GtkWidget *tab_row = ephy_tab_row_from_data (page, l->data, is_local);
      adw_preferences_group_add (page->local_tabs_preferences_group, tab_row);
    }
  } else {
    GtkWidget *expander;
    const char *device_name;

    device_name = ephy_open_tabs_record_get_client_name (record);
    expander = adw_expander_row_new ();
    adw_expander_row_set_expanded (ADW_EXPANDER_ROW (expander), FALSE);
    adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (expander), FALSE);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (expander), device_name);
    adw_expander_row_add_prefix (ADW_EXPANDER_ROW (expander),
                                 gtk_image_new_from_icon_name ("computer-symbolic"));

    for (GList *l = tabs; l && l->data; l = l->next) {
      GtkWidget *tab_row = ephy_tab_row_from_data (page, l->data, is_local);
      adw_expander_row_add_row (ADW_EXPANDER_ROW (expander), tab_row);
    }

    adw_preferences_group_add (page->tabs_preferences_group, GTK_WIDGET (expander));
  }
}

static void
ephy_synced_tabs_page_populate_model (EphySyncedTabsPage *page)
{
  EphyOpenTabsRecord *record;
  GList *remotes;

  record = ephy_open_tabs_manager_get_local_tabs (page->manager);
  ephy_synced_tabs_page_populate_from_record (page, record, TRUE);

  remotes = ephy_open_tabs_manager_get_remote_tabs (page->manager);
  for (GList *l = remotes; l && l->data; l = l->next) {
    if (ephy_open_tabs_record_is_stale (l->data, EPHY_SYNC_TABS_MAX_AGE))
      continue;

    ephy_synced_tabs_page_populate_from_record (page, l->data, FALSE);
  }

  g_object_unref (record);
}

static void
ephy_synced_tabs_page_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphySyncedTabsPage *page = EPHY_SYNCED_TABS_PAGE (object);

  switch ((EphySyncedTabsPageProps)prop_id) {
    case PROP_OPEN_TABS_MANAGER:
      if (page->manager)
        g_object_unref (page->manager);
      page->manager = g_object_ref (g_value_get_object (value));
      break;
  }
}

static void
ephy_synced_tabs_page_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EphySyncedTabsPage *page = EPHY_SYNCED_TABS_PAGE (object);

  switch ((EphySyncedTabsPageProps)prop_id) {
    case PROP_OPEN_TABS_MANAGER:
      g_value_set_object (value, page->manager);
      break;
  }
}

static void
ephy_synced_tabs_page_constructed (GObject *object)
{
  EphySyncedTabsPage *page = EPHY_SYNCED_TABS_PAGE (object);

  G_OBJECT_CLASS (ephy_synced_tabs_page_parent_class)->constructed (object);

  ephy_synced_tabs_page_populate_model (page);
}

static void
ephy_synced_tabs_page_dispose (GObject *object)
{
  EphySyncedTabsPage *page = EPHY_SYNCED_TABS_PAGE (object);

  g_clear_object (&page->manager);

  g_cancellable_cancel (page->cancellable);
  g_clear_object (&page->cancellable);

  G_OBJECT_CLASS (ephy_synced_tabs_page_parent_class)->dispose (object);
}

static void
ephy_synced_tabs_page_class_init (EphySyncedTabsPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_synced_tabs_page_set_property;
  object_class->get_property = ephy_synced_tabs_page_get_property;
  object_class->constructed = ephy_synced_tabs_page_constructed;
  object_class->dispose = ephy_synced_tabs_page_dispose;

  obj_properties[PROP_OPEN_TABS_MANAGER] =
    g_param_spec_object ("open-tabs-manager",
                         NULL, NULL,
                         EPHY_TYPE_OPEN_TABS_MANAGER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (obj_properties), obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/synced-tabs-page.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySyncedTabsPage, tabs_preferences_group);
  gtk_widget_class_bind_template_child (widget_class, EphySyncedTabsPage, local_tabs_preferences_group);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, GDK_NO_MODIFIER_MASK, "window.close", NULL);
}

static void
ephy_synced_tabs_page_init (EphySyncedTabsPage *page)
{
  gtk_widget_init_template (GTK_WIDGET (page));

  page->database = ephy_embed_shell_get_favicon_database (ephy_embed_shell_get_default ());
  page->cancellable = g_cancellable_new ();
}

EphySyncedTabsPage *
ephy_synced_tabs_page_new (EphyOpenTabsManager *manager)
{
  return EPHY_SYNCED_TABS_PAGE (g_object_new (EPHY_TYPE_SYNCED_TABS_PAGE,
                                              "open-tabs-manager", manager,
                                              NULL));
}
