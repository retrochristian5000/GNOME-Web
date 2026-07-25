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

#include "ephy-screenshot-session.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "ephy-embed-shell.h"
#include "ephy-screenshot-capture.h"
#include "ephy-screenshot-preview-dialog.h"

#define SCREENSHOT_MESSAGE_HANDLER "screenshot"
#define SCREENSHOT_SCRIPT_RESOURCE "/org/gnome/epiphany/screenshot-overlay.js"
#define SCREENSHOT_STYLE_RESOURCE "/org/gnome/epiphany/screenshot-overlay.css"

struct _EphyScreenshotSession {
  GObject parent_instance;

  /* Weak: an in-flight capture can outlive the window. */
  EphyWindow *window;
  EphyEmbed *embed;
  EphyWebView *web_view;
  WebKitUserContentManager *ucm;
  WebKitUserStyleSheet *stylesheet;
  AdwToast *hint_toast;
  /* Weak: owned by the embed's overlay while attached. */
  GtkWidget *toolbar;
  /* Epiphany's private script world, out of reach of page scripts. */
  const char *world;
  GCancellable *cancellable;
  char *suggested_filename;
  const char *heading;
  gulong message_id;
  gboolean overlay_active;
  gboolean capturing;
  gboolean finished;
};

G_DEFINE_FINAL_TYPE (EphyScreenshotSession, ephy_screenshot_session, G_TYPE_OBJECT)

enum {
  FINISHED,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
teardown_page (EphyScreenshotSession *self)
{
  if (self->toolbar) {
    GtkWidget *toolbar = self->toolbar;

    g_clear_weak_pointer (&self->toolbar);
    ephy_embed_remove_overlay_widget (self->embed, toolbar);
  }

  if (self->overlay_active) {
    self->overlay_active = FALSE;

    /* Nothing to remove if the window is already gone with the page. */
    if (self->window)
      webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (self->web_view),
                                           "if (window.EphyScreenshotSelector)"
                                           "  window.EphyScreenshotSelector.destroy();", -1,
                                           self->world, NULL, NULL, NULL, NULL);
  }

  if (self->message_id != 0) {
    g_clear_signal_handler (&self->message_id, self->ucm);
    webkit_user_content_manager_unregister_script_message_handler (self->ucm,
                                                                   SCREENSHOT_MESSAGE_HANDLER,
                                                                   self->world);
  }

  if (self->stylesheet) {
    webkit_user_content_manager_remove_style_sheet (self->ucm, self->stylesheet);
    g_clear_pointer (&self->stylesheet, webkit_user_style_sheet_unref);
  }

  if (self->hint_toast) {
    adw_toast_dismiss (self->hint_toast);
    g_clear_object (&self->hint_toast);
  }
}

/* Ends the interaction. The owner drops its reference from the "finished"
 * handler, so callers must not touch @self afterwards.
 */
static void
session_finish (EphyScreenshotSession *self)
{
  if (self->finished)
    return;

  self->finished = TRUE;
  teardown_page (self);

  g_signal_emit (self, signals[FINISHED], 0);
}

static void
session_fail (EphyScreenshotSession *self,
              const char            *message)
{
  if (self->window)
    ephy_window_add_toast (self->window, adw_toast_new (message));

  session_finish (self);
}

static void
retry_cb (EphyWindow *window)
{
  g_action_group_activate_action (ephy_window_get_action_group (window, "win"), "screenshot", NULL);
}

static void
capture_ready_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr (EphyScreenshotSession) self = user_data;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GError) error = NULL;
  AdwDialog *dialog;

  texture = ephy_screenshot_capture_finish (EPHY_WEB_VIEW (source), result, &error);
  if (!texture) {
    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      session_fail (self, error->message);
    else
      session_finish (self);
    return;
  }

  if (!self->window) {
    session_finish (self);
    return;
  }

  dialog = ephy_screenshot_preview_dialog_new (self->window, texture,
                                               self->heading, self->suggested_filename);
  g_signal_connect_object (dialog, "retry", G_CALLBACK (retry_cb), self->window, G_CONNECT_SWAPPED);
  adw_dialog_present (dialog, GTK_WIDGET (self->window));

  session_finish (self);
}

static void
start_capture (EphyScreenshotSession    *self,
               EphyScreenshotRegion      region,
               const EphyScreenshotCrop *crop,
               const char               *heading)
{
  if (self->capturing)
    return;

  self->capturing = TRUE;
  self->heading = heading;
  teardown_page (self);

  ephy_screenshot_capture_async (self->web_view, region, crop,
                                 self->cancellable, capture_ready_cb, g_object_ref (self));
}

static void
visible_area_clicked_cb (EphyScreenshotSession *self)
{
  start_capture (self, EPHY_SCREENSHOT_REGION_VISIBLE, NULL, _("Visible Area Screenshot"));
}

static void
full_page_clicked_cb (EphyScreenshotSession *self)
{
  start_capture (self, EPHY_SCREENSHOT_REGION_FULL_PAGE, NULL, _("Full Page Screenshot"));
}

static gboolean
toolbar_key_pressed_cb (EphyScreenshotSession *self,
                        guint                  keyval,
                        guint                  keycode,
                        GdkModifierType        state)
{
  if (keyval != GDK_KEY_Escape)
    return GDK_EVENT_PROPAGATE;

  session_finish (self);

  return GDK_EVENT_STOP;
}

static void
show_toolbar (EphyScreenshotSession *self)
{
  GtkWidget *toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *visible_area = gtk_button_new_with_label (_("Visible Area"));
  GtkWidget *full_page = gtk_button_new_with_label (_("Full Page"));
  GtkEventController *controller;

  gtk_widget_set_halign (toolbar, GTK_ALIGN_END);
  gtk_widget_set_valign (toolbar, GTK_ALIGN_START);
  gtk_widget_set_margin_top (toolbar, 12);
  gtk_widget_set_margin_end (toolbar, 12);

  gtk_widget_add_css_class (toolbar, "screenshot-toolbar");
  gtk_widget_add_css_class (visible_area, "pill");
  gtk_widget_add_css_class (full_page, "pill");
  gtk_widget_add_css_class (full_page, "suggested-action");

  g_signal_connect_object (visible_area, "clicked",
                           G_CALLBACK (visible_area_clicked_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (full_page, "clicked",
                           G_CALLBACK (full_page_clicked_cb), self, G_CONNECT_SWAPPED);

  gtk_box_append (GTK_BOX (toolbar), visible_area);
  gtk_box_append (GTK_BOX (toolbar), full_page);

  controller = gtk_event_controller_key_new ();
  g_signal_connect_object (controller, "key-pressed",
                           G_CALLBACK (toolbar_key_pressed_cb), self, G_CONNECT_SWAPPED);
  gtk_widget_add_controller (toolbar, controller);

  ephy_embed_add_overlay_widget (self->embed, toolbar);
  self->toolbar = toolbar;
  g_object_add_weak_pointer (G_OBJECT (toolbar), (gpointer *)&self->toolbar);
}

static double
get_number (JSCValue   *object,
            const char *name)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (object, name);

  return value && jsc_value_is_number (value) ? jsc_value_to_double (value) : 0;
}

static void
message_received_cb (WebKitUserContentManager *ucm,
                     JSCValue                 *message,
                     EphyScreenshotSession    *session)
{
  g_autoptr (EphyScreenshotSession) self = g_object_ref (session);
  g_autofree char *action = NULL;
  g_autoptr (JSCValue) action_value = NULL;
  EphyScreenshotCrop crop;

  if (self->capturing)
    return;

  /* The overlay removes itself from the page before posting. */
  self->overlay_active = FALSE;

  if (jsc_value_is_object (message)) {
    action_value = jsc_value_object_get_property (message, "action");
    if (jsc_value_is_string (action_value))
      action = jsc_value_to_string (action_value);
  }

  /* The overlay only reports element picking; the buttons are native. */
  if (g_strcmp0 (action, "element") != 0) {
    session_finish (self);
    return;
  }

  crop.x = get_number (message, "x");
  crop.y = get_number (message, "y");
  crop.width = get_number (message, "width");
  crop.height = get_number (message, "height");
  crop.region_width = get_number (message, "viewWidth");

  if (crop.width < 1 || crop.height < 1 || crop.region_width < 1) {
    session_fail (self, _("The selected element is empty"));
    return;
  }

  /* The element is under the pointer, so the visible region is enough and much
   * cheaper than a snapshot of the whole document.
   */
  start_capture (self, EPHY_SCREENSHOT_REGION_VISIBLE, &crop, _("Element Screenshot"));
}

static void
overlay_ready_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr (EphyScreenshotSession) self = user_data;
  g_autoptr (JSCValue) value = NULL;
  g_autoptr (GError) error = NULL;

  value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source), result, &error);
  if (!value) {
    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      session_fail (self, error->message);
    return;
  }

  if (!jsc_value_to_boolean (value)) {
    session_fail (self, _("This page cannot be captured"));
    return;
  }

  self->overlay_active = TRUE;

  if (!self->window)
    return;

  show_toolbar (self);

  self->hint_toast = adw_toast_new (_("Click an element to capture it, or press Esc to cancel"));
  ephy_window_add_toast (self->window, g_object_ref (self->hint_toast));
}

static void
load_changed_cb (EphyScreenshotSession *self,
                 WebKitLoadEvent        load_event)
{
  if (load_event == WEBKIT_LOAD_STARTED || load_event == WEBKIT_LOAD_COMMITTED)
    session_finish (self);
}

static void
active_child_changed_cb (EphyScreenshotSession *self)
{
  session_finish (self);
}

static char *
escape_js_string (const char *text)
{
  GString *escaped = g_string_new (NULL);

  for (const char *c = text; *c != '\0'; c++) {
    switch (*c) {
      case '"':
      case '\\':
        g_string_append_c (escaped, '\\');
        g_string_append_c (escaped, *c);
        break;
      case '\n':
        g_string_append (escaped, "\\n");
        break;
      case '\r':
        g_string_append (escaped, "\\r");
        break;
      default:
        g_string_append_c (escaped, *c);
    }
  }

  return g_string_free_and_steal (escaped);
}

/* The script cannot reach the translation catalogs, so its strings are
 * prepended to it here.
 */
static char *
build_overlay_script (void)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *title = escape_js_string (_("Take Screenshot"));
  GString *script;
  const char *data;
  gsize size;

  bytes = g_resources_lookup_data (SCREENSHOT_SCRIPT_RESOURCE, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  g_assert (bytes);

  script = g_string_new (NULL);
  g_string_append_printf (script,
                          "window.EphyScreenshotStrings = { title: \"%s\" };\n",
                          title);

  data = g_bytes_get_data (bytes, &size);
  g_string_append_len (script, data, size);

  return g_string_free_and_steal (script);
}

static WebKitUserStyleSheet *
build_overlay_style_sheet (void)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *source = NULL;
  const char *data;
  gsize size;

  bytes = g_resources_lookup_data (SCREENSHOT_STYLE_RESOURCE, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  g_assert (bytes);

  data = g_bytes_get_data (bytes, &size);
  source = g_strndup (data, size);

  /* A user style sheet applies even where the page's Content-Security-Policy
   * forbids inline styles.
   */
  return webkit_user_style_sheet_new (source,
                                      WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                      WEBKIT_USER_STYLE_LEVEL_USER,
                                      NULL, NULL);
}

void
ephy_screenshot_session_start (EphyScreenshotSession *self)
{
  g_autofree char *script = NULL;

  g_return_if_fail (EPHY_IS_SCREENSHOT_SESSION (self));
  g_return_if_fail (self->message_id == 0);

  self->message_id = g_signal_connect (self->ucm,
                                       "script-message-received::" SCREENSHOT_MESSAGE_HANDLER,
                                       G_CALLBACK (message_received_cb), self);
  webkit_user_content_manager_register_script_message_handler (self->ucm,
                                                               SCREENSHOT_MESSAGE_HANDLER,
                                                               self->world);

  self->stylesheet = build_overlay_style_sheet ();
  webkit_user_content_manager_add_style_sheet (self->ucm, self->stylesheet);

  g_signal_connect_object (self->web_view, "load-changed",
                           G_CALLBACK (load_changed_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->window, "notify::active-child",
                           G_CALLBACK (active_child_changed_cb), self, G_CONNECT_SWAPPED);

  /* The overlay is set up on a page that is already loaded, so a user script
   * would only take effect on the next navigation.
   */
  script = build_overlay_script ();
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (self->web_view),
                                       script, -1,
                                       self->world, NULL,
                                       self->cancellable,
                                       overlay_ready_cb,
                                       g_object_ref (self));
}

static void
ephy_screenshot_session_dispose (GObject *object)
{
  EphyScreenshotSession *self = EPHY_SCREENSHOT_SESSION (object);

  g_cancellable_cancel (self->cancellable);
  teardown_page (self);

  g_clear_weak_pointer (&self->window);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->ucm);
  g_clear_object (&self->web_view);
  g_clear_object (&self->embed);
  g_clear_pointer (&self->suggested_filename, g_free);

  G_OBJECT_CLASS (ephy_screenshot_session_parent_class)->dispose (object);
}

static void
ephy_screenshot_session_class_init (EphyScreenshotSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_screenshot_session_dispose;

  signals[FINISHED] = g_signal_new ("finished",
                                    G_OBJECT_CLASS_TYPE (object_class),
                                    G_SIGNAL_RUN_LAST,
                                    0, NULL, NULL, NULL,
                                    G_TYPE_NONE, 0);
}

static void
ephy_screenshot_session_init (EphyScreenshotSession *self)
{
  self->cancellable = g_cancellable_new ();
}

EphyScreenshotSession *
ephy_screenshot_session_new (EphyWindow *window,
                             EphyEmbed  *embed,
                             const char *suggested_filename)
{
  EphyScreenshotSession *self;
  EphyWebView *web_view;

  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);
  g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

  web_view = ephy_embed_get_web_view (embed);

  self = g_object_new (EPHY_TYPE_SCREENSHOT_SESSION, NULL);
  self->window = window;
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&self->window);
  self->embed = g_object_ref (embed);
  self->web_view = g_object_ref (web_view);
  self->world = ephy_embed_shell_get_guid (ephy_embed_shell_get_default ());
  self->ucm = g_object_ref (webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (web_view)));
  self->suggested_filename = g_strdup (suggested_filename);

  return self;
}
