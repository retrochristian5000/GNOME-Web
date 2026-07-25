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

#include "config.h"

#include "ephy-screenshot-preview-dialog.h"

#include <glib/gi18n.h>

#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-window.h"

#define PREVIEW_WIDTH 360
#define PREVIEW_MIN_HEIGHT 120
#define PREVIEW_MAX_HEIGHT 280

struct _EphyScreenshotPreviewDialog {
  AdwAlertDialog parent_instance;

  /* Weak: the dialog can outlive the window while the save is in flight. */
  EphyWindow *window;
  GdkTexture *texture;
  char *suggested_filename;
};

G_DEFINE_FINAL_TYPE (EphyScreenshotPreviewDialog, ephy_screenshot_preview_dialog, ADW_TYPE_ALERT_DIALOG)

enum {
  RETRY,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
add_toast (EphyScreenshotPreviewDialog *self,
           const char                  *title)
{
  if (self->window)
    ephy_window_add_toast (self->window, adw_toast_new (title));
}

static void
copy_to_clipboard (EphyScreenshotPreviewDialog *self)
{
  gdk_clipboard_set_texture (gtk_widget_get_clipboard (GTK_WIDGET (self)), self->texture);
  add_toast (self, _("Screenshot copied"));
}

static void
replace_contents_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr (EphyScreenshotPreviewDialog) self = user_data;
  g_autoptr (GError) error = NULL;

  if (!g_file_replace_contents_finish (G_FILE (source), result, NULL, &error))
    add_toast (self, error->message);
  else
    add_toast (self, _("Screenshot finished"));
}

static void
save_finish_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr (EphyScreenshotPreviewDialog) self = user_data;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) parent = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *parent_path = NULL;

  file = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), result, &error);
  if (!file) {
    if (error && !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      add_toast (self, error->message);
    return;
  }

  parent = g_file_get_parent (file);
  parent_path = parent ? g_file_get_path (parent) : NULL;
  if (parent_path)
    g_settings_set_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY, parent_path);

  bytes = gdk_texture_save_to_png_bytes (self->texture);

  g_file_replace_contents_bytes_async (file, bytes, NULL, FALSE, G_FILE_CREATE_NONE,
                                       NULL, replace_contents_cb, g_object_ref (self));
}

static void
save_to_file (EphyScreenshotPreviewDialog *self)
{
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;
  const char *last_directory_path;

  gtk_file_dialog_set_title (dialog, _("Save Screenshot"));
  gtk_file_dialog_set_initial_name (dialog, self->suggested_filename);

  last_directory_path = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY);
  if (last_directory_path && last_directory_path[0]) {
    g_autoptr (GFile) last_directory = g_file_new_for_path (last_directory_path);

    gtk_file_dialog_set_initial_folder (dialog, last_directory);
  }

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("PNG"));
  gtk_file_filter_add_pattern (filter, "*.png");
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (self->window),
                        NULL,
                        save_finish_cb,
                        g_object_ref (self));
  g_object_unref (dialog);
}

static void
response_cb (EphyScreenshotPreviewDialog *self,
             const char                  *response)
{
  if (g_strcmp0 (response, "retry") == 0)
    g_signal_emit (self, signals[RETRY], 0);
  else if (g_strcmp0 (response, "copy") == 0)
    copy_to_clipboard (self);
  else if (g_strcmp0 (response, "save") == 0)
    save_to_file (self);
}

static void
ephy_screenshot_preview_dialog_dispose (GObject *object)
{
  EphyScreenshotPreviewDialog *self = EPHY_SCREENSHOT_PREVIEW_DIALOG (object);

  g_clear_weak_pointer (&self->window);

  g_clear_object (&self->texture);
  g_clear_pointer (&self->suggested_filename, g_free);

  G_OBJECT_CLASS (ephy_screenshot_preview_dialog_parent_class)->dispose (object);
}

static void
ephy_screenshot_preview_dialog_class_init (EphyScreenshotPreviewDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_screenshot_preview_dialog_dispose;

  signals[RETRY] = g_signal_new ("retry",
                                 G_OBJECT_CLASS_TYPE (object_class),
                                 G_SIGNAL_RUN_LAST,
                                 0, NULL, NULL, NULL,
                                 G_TYPE_NONE, 0);
}

static void
ephy_screenshot_preview_dialog_init (EphyScreenshotPreviewDialog *self)
{
  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (self),
                                  "cancel", _("_Cancel"),
                                  "retry", _("_Retry"),
                                  "copy", _("_Copy"),
                                  "save", _("_Save…"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (self), "save", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (self), "save");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (self), "cancel");

  g_signal_connect (self, "response", G_CALLBACK (response_cb), NULL);
}

AdwDialog *
ephy_screenshot_preview_dialog_new (EphyWindow *window,
                                    GdkTexture *texture,
                                    const char *heading,
                                    const char *suggested_filename)
{
  EphyScreenshotPreviewDialog *self;
  GtkWidget *picture;
  int width;
  int height;

  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  self = g_object_new (EPHY_TYPE_SCREENSHOT_PREVIEW_DIALOG,
                       "heading", heading,
                       NULL);
  self->window = window;
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&self->window);
  self->texture = g_object_ref (texture);
  self->suggested_filename = g_strdup (suggested_filename);

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  adw_alert_dialog_format_body (ADW_ALERT_DIALOG (self), _("%d × %d pixels"), width, height);

  picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
  gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
  gtk_widget_set_size_request (picture, PREVIEW_WIDTH,
                               CLAMP (PREVIEW_WIDTH * height / width,
                                      PREVIEW_MIN_HEIGHT, PREVIEW_MAX_HEIGHT));
  gtk_widget_set_overflow (picture, GTK_OVERFLOW_HIDDEN);
  gtk_widget_add_css_class (picture, "card");
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (self), picture);

  return ADW_DIALOG (self);
}
