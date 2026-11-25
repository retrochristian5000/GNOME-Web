/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Igalia S.L.
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

#include "ephy-downloads-popover.h"

#include "ephy-download-widget.h"
#include "ephy-downloads-manager.h"
#include "ephy-embed-shell.h"
#include "ephy-window.h"

struct _EphyDownloadsPopover {
  AdwBin parent_instance;

  GtkWidget *downloads_stack;
  GtkWidget *downloads_box;
  GtkWidget *finished_box;
  GtkWidget *failed_box;
  GtkWidget *downloads_group;
  GtkWidget *finished_group;
  GtkWidget *failed_group;
  GtkWidget *search_button;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *clear_button;
};

G_DEFINE_FINAL_TYPE (EphyDownloadsPopover, ephy_downloads_popover, ADW_TYPE_BIN)

static void
download_completed_cb (EphyDownloadsPopover *popover,
                       EphyDownload         *download)
{
  GtkListBoxRow *row;
  int i = 0;

  gtk_widget_set_sensitive (popover->clear_button, TRUE);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), i++))) {
    if (ephy_download_widget_get_download (EPHY_DOWNLOAD_WIDGET (row)) == download) {
      GtkWidget *new_row = ephy_download_widget_new (download);

      gtk_list_box_remove (GTK_LIST_BOX (popover->downloads_box), GTK_WIDGET (row));
      gtk_list_box_prepend (GTK_LIST_BOX (popover->finished_box), new_row);
      break;
    }
  }

  if (!gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), 0))
    gtk_widget_set_visible (popover->downloads_group, FALSE);

  gtk_widget_set_visible (popover->finished_group, TRUE);
}

static void
download_failed_cb (EphyDownloadsPopover *popover,
                    GError               *error,
                    EphyDownload         *download)
{
  GtkListBoxRow *row;
  int i = 0;

  gtk_widget_set_sensitive (popover->clear_button, TRUE);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), i++))) {
    if (ephy_download_widget_get_download (EPHY_DOWNLOAD_WIDGET (row)) == download) {
      GtkWidget *new_row = ephy_download_widget_new (download);

      gtk_list_box_remove (GTK_LIST_BOX (popover->downloads_box), GTK_WIDGET (row));
      gtk_list_box_prepend (GTK_LIST_BOX (popover->failed_box), new_row);
      break;
    }
  }

  if (!gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), 0))
    gtk_widget_set_visible (popover->downloads_group, FALSE);

  gtk_widget_set_visible (popover->failed_group, TRUE);
}

static void
download_added_cb (EphyDownloadsPopover *popover,
                   EphyDownload         *download)
{
  GtkWidget *row = ephy_download_widget_new (download);
  const char *visible_child = gtk_stack_get_visible_child_name (GTK_STACK (popover->downloads_stack));

  gtk_list_box_prepend (GTK_LIST_BOX (popover->downloads_box), row);
  gtk_widget_set_visible (popover->downloads_group, TRUE);

  if (g_strcmp0 (visible_child, "empty-page") == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (popover->downloads_stack), "main-page");
    gtk_widget_set_sensitive (popover->search_button, TRUE);
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (popover->search_bar), FALSE);
  }

  g_signal_connect_object (download, "completed",
                           G_CALLBACK (download_completed_cb),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (download, "error",
                           G_CALLBACK (download_failed_cb),
                           popover, G_CONNECT_SWAPPED);
}

static void
download_removed_cb (EphyDownloadsPopover *popover,
                     EphyDownload         *download)
{
  gboolean active_downloads = !!gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), 0);
  gboolean finished_downloads = !!gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->finished_box), 0);
  gboolean failed_downloads = !!gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->failed_box), 0);

  if (!active_downloads && !!finished_downloads && !!failed_downloads) {
    gtk_stack_set_visible_child_name (GTK_STACK (popover->downloads_stack), "empty-page");
    gtk_widget_set_sensitive (popover->search_button, FALSE);
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (popover->search_bar), FALSE);
  }

  gtk_widget_set_visible (popover->downloads_group, active_downloads);
  gtk_widget_set_visible (popover->finished_group, finished_downloads);
  gtk_widget_set_visible (popover->failed_group, failed_downloads);

  gtk_widget_set_sensitive (popover->clear_button, finished_downloads || failed_downloads);
}

static gboolean
is_not_empty_page (const char *visible_child_name)
{
  return g_strcmp0 (visible_child_name, "empty-page") != 0;
}


static void
on_search_entry_changed (GtkSearchEntry *entry,
                         gpointer        user_data)
{
  EphyDownloadsPopover *popover = EPHY_DOWNLOADS_POPOVER (user_data);

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (popover->downloads_box));
}

static gboolean
on_search_entry_key_pressed (GtkEventControllerKey *key_controller,
                             guint                  keyval,
                             guint                  keycode,
                             GdkModifierType        state,
                             EphyDownloadsPopover  *popover)
{
  if (keyval == GDK_KEY_Escape) {
    GtkWidget *window = gtk_widget_get_ancestor (GTK_WIDGET (popover), EPHY_TYPE_WINDOW);

    ephy_window_toggle_sidebar (EPHY_WINDOW (window));
    return TRUE;
  }

  return FALSE;
}

static void
clear_button_clicked_cb (EphyDownloadsPopover *popover)
{
  EphyDownloadsManager *manager;
  GtkListBoxRow *row;
  int i = 0;

  manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());
  g_signal_handlers_block_by_func (manager, download_removed_cb, popover);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->finished_box), i++))) {
    EphyDownload *download = ephy_download_widget_get_download (EPHY_DOWNLOAD_WIDGET (row));

    ephy_downloads_manager_remove_download (manager, download);
    gtk_list_box_remove (GTK_LIST_BOX (popover->finished_box), GTK_WIDGET (row));
  }

  i = 0;
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->failed_box), i++))) {
    EphyDownload *download = ephy_download_widget_get_download (EPHY_DOWNLOAD_WIDGET (row));

    ephy_downloads_manager_remove_download (manager, download);
    gtk_list_box_remove (GTK_LIST_BOX (popover->failed_box), GTK_WIDGET (row));
  }

  gtk_widget_set_sensitive (popover->clear_button, FALSE);
  gtk_widget_set_visible (popover->finished_group, FALSE);
  gtk_widget_set_visible (popover->failed_group, FALSE);

  if (!gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), 0)) {
    gtk_stack_set_visible_child_name (GTK_STACK (popover->downloads_stack), "empty-page");
    gtk_widget_set_sensitive (popover->search_button, FALSE);
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (popover->search_bar), FALSE);
  }

  g_signal_handlers_unblock_by_func (manager, download_removed_cb, popover);
}

static gboolean
downloads_list_box_filter_func (GtkListBoxRow *row,
                                gpointer       user_data)
{
  EphyDownloadsPopover *popover = EPHY_DOWNLOADS_POPOVER (user_data);
  g_autofree gchar *search_casefold = NULL;
  g_autofree gchar *title_casefold = NULL;
  const char *title;
  const char *search_text;

  g_assert (GTK_IS_LIST_BOX_ROW (row));

  title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  title_casefold = g_utf8_casefold (title, -1);

  search_text = gtk_editable_get_text (GTK_EDITABLE (popover->search_entry));
  search_casefold = g_utf8_casefold (search_text, -1);

  return !!strstr (title_casefold, search_casefold);
}

static void
ephy_downloads_popover_class_init (EphyDownloadsPopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/downloads-popover.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, downloads_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, downloads_box);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, finished_box);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, failed_box);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, downloads_group);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, finished_group);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, failed_group);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, search_button);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, search_bar);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, search_entry);
  gtk_widget_class_bind_template_child (widget_class, EphyDownloadsPopover, clear_button);

  gtk_widget_class_bind_template_callback (widget_class, is_not_empty_page);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, clear_button_clicked_cb);
}

static void
ephy_downloads_popover_init (EphyDownloadsPopover *popover)
{
  GList *downloads, *l;
  EphyDownloadsManager *manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  gtk_widget_init_template (GTK_WIDGET (popover));

  gtk_list_box_set_filter_func (GTK_LIST_BOX (popover->downloads_box),
                                (GtkListBoxFilterFunc)downloads_list_box_filter_func,
                                popover, NULL);

  downloads = ephy_downloads_manager_get_downloads (manager);
  for (l = downloads; l; l = g_list_next (l)) {
    EphyDownload *download = (EphyDownload *)l->data;
    GtkWidget *row;

    g_signal_connect_object (download, "completed",
                             G_CALLBACK (download_completed_cb),
                             popover, G_CONNECT_SWAPPED);
    g_signal_connect_object (download, "error",
                             G_CALLBACK (download_failed_cb),
                             popover, G_CONNECT_SWAPPED);

    row = ephy_download_widget_new (download);
    gtk_list_box_prepend (GTK_LIST_BOX (popover->downloads_box), row);
  }

  if (gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->downloads_box), 0)) {
    gtk_stack_set_visible_child_name (GTK_STACK (popover->downloads_stack), "main-page");
    gtk_widget_set_visible (popover->downloads_group, TRUE);
  } else {
    gtk_stack_set_visible_child_name (GTK_STACK (popover->downloads_stack), "empty-page");
    gtk_widget_set_sensitive (popover->search_button, FALSE);
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (popover->search_bar), FALSE);
  }

  g_signal_connect_object (manager, "download-added",
                           G_CALLBACK (download_added_cb),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (manager, "download-removed",
                           G_CALLBACK (download_removed_cb),
                           popover, G_CONNECT_SWAPPED);
}
