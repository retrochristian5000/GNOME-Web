/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Purism SPC
 *  Copyright © 2018 Adrien Plazas <kekun.plazas@laposte.net>
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

#include "ephy-action-bar-common.h"

#include "ephy-window.h"
#include "ephy-containers.h"
#include "ephy-settings.h"
#include "ephy-embed.h"
#include "ephy-favicon-helpers.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-prefs.h"
#include "ephy-shell.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-action-bar-start.h"
#include "ephy-action-bar.h"

/* TODO: Most of this logic can fit into a parent class. For now it
 *   relies on the type because I am lazy. If you are reading this,
 *   ideally, you should move this logic into a class like
 *   EphyActionBarCommon, then make EphyActionBar{,Start} inherit this
 *   class. */

typedef enum {
  EPHY_NAVIGATION_HISTORY_DIRECTION_BACK,
  EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD
} EphyNavigationHistoryDirection;

typedef enum {
  WEBKIT_HISTORY_BACKWARD,
  WEBKIT_HISTORY_FORWARD
} WebKitHistoryType;

#define MAX_LABEL_LENGTH 48
#define HISTORY_ITEM_DATA_KEY "history-item-data-key"

static GtkWidget *
build_new_tab_menu (GtkWidget *action_bar,
                    GtkWidget *parent)
{
  GtkWidget *popover;
  EphyShell *embed_shell;
  EphyWindow *window;
  char *str, *uuid;
  GMenu *menu, *split;
  g_autoptr (GPtrArray) names;
  g_autoptr (GPtrArray) uuids;

  names = get_container_names ();
  uuids = get_container_uuids ();
  
  embed_shell = ephy_shell_get_default();
  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (embed_shell)));

  menu = g_menu_new ();
  split = g_menu_new ();
  
  for (size_t i = 0; i < names->len; i++) {
    uuid = g_ptr_array_index (uuids, i);
    str = g_strdup_printf ("win.new-tab-container::%s", uuid);
    g_menu_append (menu, g_ptr_array_index (names, i), str);
    g_free (str);
  }
  g_menu_append (split, "New Container", "win.new-container");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (split));

  popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
  /* TODO: should we set these again? */
  /*gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE); */
  /*gtk_widget_set_halign (popover, GTK_ALIGN_START); */
  /*gtk_widget_add_css_class (popover, "menu"); */
  gtk_widget_set_parent (popover, parent);

  if (EPHY_IS_ACTION_BAR_START (action_bar))
    ephy_action_bar_start_set_new_tab_menu (EPHY_ACTION_BAR_START (action_bar), popover);
  else
    ephy_action_bar_set_tab_button_menu (EPHY_ACTION_BAR (action_bar), popover);
  
  return popover;
}

void
handle_new_tab_menu (GtkWidget  *parent,
                     GtkWidget  *button,
                     double      x,
                     double      y,
                     GtkGesture *gesture)
{
  GtkWidget *widget;
  GtkWidget *popover;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  popover = build_new_tab_menu (parent, widget);

  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_CHECKED, FALSE);
  
  if (EPHY_IS_ACTION_BAR_START (widget))
    g_signal_connect_swapped (popover, "closed",
                              G_CALLBACK (ephy_action_bar_tab_menu_closed_cb),
                              widget);
  /* if (EPHY_IS_ACTION_BAR_START (widget)) */
  /*   g_signal_connect_swapped (ephy_action_bar_start_get_history_menu (EPHY_ACTION_BAR_START (widget)), "closed", */
  /*                             G_CALLBACK (ephy_action_bar_start_history_menu_closed_cb), widget); */
  /* else */
  /*   g_signal_connect_swapped (ephy_action_bar_get_history_menu (EPHY_ACTION_BAR (widget)), "closed", */
  /*                             G_CALLBACK (ephy_action_bar_start_history_menu_closed_cb), widget); */

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}


static void
history_row_enter_cb (GtkEventController *controller,
                      double              x,
                      double              y,
                      EphyActionBarStart *action_bar_start)
{
  GtkWidget *widget;
  const char *text;
  GtkRoot *root;
  EphyEmbed *embed;
  WebKitWebView *web_view;

  widget = gtk_event_controller_get_widget (controller);
  text = g_object_get_data (G_OBJECT (widget), "link-message");

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_start));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  ephy_web_view_set_link_message (EPHY_WEB_VIEW (web_view), text);
}

static void
history_row_leave_cb (GtkEventController *controller,
                      EphyActionBarStart *action_bar_start)
{
  GtkRoot *root;
  EphyEmbed *embed;
  WebKitWebView *web_view;

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_start));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  ephy_web_view_set_link_message (EPHY_WEB_VIEW (web_view), NULL);
}

static void
middle_click_handle_on_history_menu_item (EphyEmbed                 *embed,
                                          WebKitBackForwardListItem *item)
{
  EphyEmbed *new_embed = NULL;
  EphyNewTabFlags flags = 0;
  const gchar *url;

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))    flags |= EPHY_NEW_TAB_JUMP;

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
                                  embed,
                                  flags);
  g_assert (new_embed);

  /* Load the new URL */
  url = webkit_back_forward_list_item_get_original_uri (item);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), url);
}

static void
history_row_released_cb (GtkGesture         *gesture,
                         int                 n_click,
                         double              x,
                         double              y,
                         GtkWidget          *action_bar_common)
{
  WebKitBackForwardListItem *item;
  GtkWidget *widget;
  guint button;
  GtkRoot *root;
  EphyEmbed *embed;
  GtkPopover *history_menu_popover;
  
  if (EPHY_IS_ACTION_BAR_START (action_bar_common))
    history_menu_popover = GTK_POPOVER (ephy_action_bar_start_get_history_menu (EPHY_ACTION_BAR_START (action_bar_common)));
  else
    history_menu_popover = GTK_POPOVER (ephy_action_bar_get_history_menu (EPHY_ACTION_BAR (action_bar_common)));

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button != GDK_BUTTON_PRIMARY && button != GDK_BUTTON_MIDDLE) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_common));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));

  item = WEBKIT_BACK_FORWARD_LIST_ITEM (g_object_get_data (G_OBJECT (widget), HISTORY_ITEM_DATA_KEY));

  if (button == GDK_BUTTON_MIDDLE) {
    middle_click_handle_on_history_menu_item (embed, item);

    if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
      gtk_popover_popdown (history_menu_popover);
  } else {
    WebKitWebView *web_view;

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_go_to_back_forward_list_item (web_view, item);

    gtk_popover_popdown (history_menu_popover);
  }
}


static void
icon_loaded_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  g_autoptr (GtkWidget) image = user_data;
  g_autoptr (GIcon) favicon = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GdkTexture) icon_texture = webkit_favicon_database_get_favicon_finish (database, result, &error);
  int scale;

  if (error) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;

    g_debug ("Could not get icon: %s", error->message);
    return;
  }

  if (!icon_texture)
    return;

  scale = gtk_widget_get_scale_factor (image);
  favicon = ephy_favicon_get_from_texture_scaled (icon_texture, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
  if (favicon)
    gtk_image_set_from_gicon (GTK_IMAGE (image), favicon);
}

static GList *
construct_webkit_history_list (WebKitWebView     *web_view,
                               WebKitHistoryType  hist_type,
                               int                limit)
{
  WebKitBackForwardList *back_forward_list;

  back_forward_list = webkit_web_view_get_back_forward_list (web_view);
  return hist_type == WEBKIT_HISTORY_FORWARD ?
         g_list_reverse (webkit_back_forward_list_get_forward_list_with_limit (back_forward_list, limit)) :
         webkit_back_forward_list_get_back_list_with_limit (back_forward_list, limit);
}

static GtkWidget *
build_history_row (GtkWidget                 *action_bar_common,
                   WebKitBackForwardListItem *item)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  const char *uri;
  g_autofree char *title = NULL;
  WebKitFaviconDatabase *database;
  GtkWidget *row, *box, *icon, *label;
  GtkEventController *controller;
  GCancellable *cancellable;
  GtkGesture *gesture;
  
  if (EPHY_IS_ACTION_BAR_START (action_bar_common))
    cancellable = ephy_action_bar_start_get_cancellable (EPHY_ACTION_BAR_START (action_bar_common));
  else
    cancellable = ephy_action_bar_get_cancellable (EPHY_ACTION_BAR (action_bar_common));

  uri = webkit_back_forward_list_item_get_uri (item);
  title = g_strdup (webkit_back_forward_list_item_get_title (item));

  row = gtk_list_box_row_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  icon = gtk_image_new ();
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
  gtk_box_append (GTK_BOX (box), icon);

  label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_LABEL_LENGTH);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  g_object_set_data_full (G_OBJECT (row), HISTORY_ITEM_DATA_KEY,
                          g_object_ref (item), g_object_unref);

  if (title && *title)
    gtk_label_set_label (GTK_LABEL (label), title);
  else
    gtk_label_set_label (GTK_LABEL (label), uri);

  database = ephy_embed_shell_get_favicon_database (shell);
  webkit_favicon_database_get_favicon (database, uri,
                                       cancellable,
                                       (GAsyncReadyCallback)icon_loaded_cb,
                                       g_object_ref (icon));

  g_object_set_data_full (G_OBJECT (row), "link-message",
                          g_strdup (uri), (GDestroyNotify)g_free);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (history_row_enter_cb), action_bar_common);
  g_signal_connect (controller, "leave", G_CALLBACK (history_row_leave_cb), action_bar_common);
  gtk_widget_add_controller (row, controller);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (history_row_released_cb), action_bar_common);
  gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (gesture));

  return row;
}

static void
build_history_menu (GtkWidget                      *action_bar_common,
                    GtkWidget                      *parent,
                    EphyNavigationHistoryDirection  direction)
{
  GtkWidget *popover, *listbox;
  GtkRoot *root;
  EphyEmbed *embed;
  WebKitWebView *web_view;
  g_autoptr (GList) list = NULL;
  GList *l;

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_common));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK)
    list = construct_webkit_history_list (web_view,
                                          WEBKIT_HISTORY_BACKWARD, 10);
  else
    list = construct_webkit_history_list (web_view,
                                          WEBKIT_HISTORY_FORWARD, 10);

  popover = gtk_popover_new ();
  gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE);
  gtk_widget_set_halign (popover, GTK_ALIGN_START);
  gtk_widget_add_css_class (popover, "menu");
  gtk_widget_set_parent (popover, parent);

  listbox = gtk_list_box_new ();
  gtk_popover_set_child (GTK_POPOVER (popover), listbox);

  for (l = list; l; l = l->next) {
    GtkWidget *row = build_history_row (action_bar_common, l->data);

    gtk_list_box_append (GTK_LIST_BOX (listbox), row);
  }

  if (EPHY_IS_ACTION_BAR_START (action_bar_common))
    ephy_action_bar_start_set_history_menu (EPHY_ACTION_BAR_START (action_bar_common), popover);
  else
    ephy_action_bar_set_history_menu (EPHY_ACTION_BAR (action_bar_common), popover);
}

void
handle_history_menu (GtkWidget *action_bar,
                     double              x,
                     double              y,
                     GtkGesture         *gesture)
{
  GtkWidget *widget;
  EphyActionBarStart *action_bar_start = NULL;
  EphyActionBar      *action_bar_base  = NULL;
  GtkPopover *history_menu;
  GtkWidget *navigation_back, *navigation_forward;
  EphyNavigationHistoryDirection direction;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  if (EPHY_IS_ACTION_BAR_START (action_bar)) {
    action_bar_start = EPHY_ACTION_BAR_START (action_bar);
    navigation_back  = ephy_action_bar_start_get_navigation_button (action_bar_start, FALSE);
    navigation_forward = ephy_action_bar_start_get_navigation_button (action_bar_start, TRUE);
  } else if (EPHY_IS_ACTION_BAR (action_bar)) {
    action_bar_base  = EPHY_ACTION_BAR (action_bar);
    navigation_back  = ephy_action_bar_get_navigation_button (action_bar_base, FALSE);
    navigation_forward = ephy_action_bar_get_navigation_button (action_bar_base, TRUE);
  }
  
  g_assert (action_bar_start || action_bar_base);
  
  if (widget == navigation_back)
    direction = EPHY_NAVIGATION_HISTORY_DIRECTION_BACK;
  else if (widget == navigation_forward)
    direction = EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;
  else
    return;

  build_history_menu (action_bar, widget, direction);
  
  if (action_bar_start) {
    history_menu = GTK_POPOVER (ephy_action_bar_start_get_history_menu (action_bar_start));
  } else {
    history_menu = GTK_POPOVER (ephy_action_bar_get_history_menu (action_bar_base));
  }

  gtk_popover_popup (GTK_POPOVER (history_menu));
  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_CHECKED, FALSE);
  
  if (action_bar_start)
    g_signal_connect_swapped (history_menu, "closed",
                              G_CALLBACK (ephy_action_bar_start_history_menu_closed_cb), action_bar_start);
  else
    g_signal_connect_swapped (history_menu, "closed",
                              G_CALLBACK (ephy_action_bar_history_menu_closed_cb), action_bar_start);

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}
