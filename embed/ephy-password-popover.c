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

#include "config.h"

#include "ephy-password-popover.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pwquality.h>

#define POPOVER_DATA_KEY "ephy-password-popover"

typedef struct {
  EphyWebView *view;
  GtkWidget *popover;
  guint64 frame_id;
  char *username;
} UserChoiceData;

typedef struct {
  EphyWebView *view;
  GtkWidget *popover;
  GtkWidget *password_label;
  guint64 frame_id;
  char *password;
} GeneratePasswordData;

static void
user_choice_data_free (gpointer  data,
                       GClosure *closure)
{
  UserChoiceData *choice = data;

  g_free (choice->username);
  g_free (choice);
}

static void
generate_password_data_free (GeneratePasswordData *data)
{
  g_free (data->password);
  g_free (data);
}

static char *
generate_secure_password (void)
{
  static pwquality_settings_t *settings = NULL;
  char *result = NULL;
  int ret;

  if (!settings)
    settings = pwquality_default_settings ();

  /* 128 is bits of entropy. See https://security.stackexchange.com/a/257535 */
  ret = pwquality_generate (settings, 128, &result);
  if (ret < 0) {
    g_warning ("Failed to generate secure random password: %s",
               pwquality_strerror (NULL, 0, ret, NULL));
    return NULL;
  }

  return result;
}

static void
cleanup_popover (EphyWebView *view,
                 GtkWidget   *popover)
{
  if (g_object_get_data (G_OBJECT (view), POPOVER_DATA_KEY) == popover)
    g_object_set_data (G_OBJECT (view), POPOVER_DATA_KEY, NULL);

  if (gtk_widget_get_parent (popover))
    gtk_widget_unparent (popover);
}

static void
on_popover_closed (GtkPopover  *popover,
                   EphyWebView *view)
{
  cleanup_popover (view, GTK_WIDGET (popover));
}

static void
on_view_destroy (GtkWidget *view,
                 GtkWidget *popover)
{
  if (gtk_widget_get_parent (popover))
    gtk_widget_unparent (popover);
}

void
ephy_password_popover_dismiss (EphyWebView *view)
{
  GtkWidget *existing;

  g_assert (EPHY_IS_WEB_VIEW (view));

  existing = g_object_get_data (G_OBJECT (view), POPOVER_DATA_KEY);
  if (!existing)
    return;

  g_object_set_data (G_OBJECT (view), POPOVER_DATA_KEY, NULL);
  gtk_popover_popdown (GTK_POPOVER (existing));
  if (gtk_widget_get_parent (existing))
    gtk_widget_unparent (existing);
}

static GtkWidget *
create_popover_for_view (EphyWebView *view,
                         int          element_x,
                         int          element_y,
                         int          element_width,
                         int          element_height)
{
  GtkWidget *popover;
  GdkRectangle rect;

  ephy_password_popover_dismiss (view);

  popover = gtk_popover_new ();
  gtk_popover_set_has_arrow (GTK_POPOVER (popover), TRUE);
  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (popover), TRUE);

  rect.x = MAX (0, element_x);
  rect.y = MAX (0, element_y);
  rect.width = MAX (1, element_width);
  rect.height = MAX (1, element_height);
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

  gtk_widget_set_parent (popover, GTK_WIDGET (view));
  g_object_set_data (G_OBJECT (view), POPOVER_DATA_KEY, popover);

  g_signal_connect_object (popover, "closed", G_CALLBACK (on_popover_closed), view, G_CONNECT_DEFAULT);
  g_signal_connect_object (view, "destroy", G_CALLBACK (on_view_destroy), popover, G_CONNECT_DEFAULT);

  return popover;
}

static void
on_user_button_clicked (GtkButton      *button,
                        UserChoiceData *choice)
{
  EphyWebView *view = choice->view;
  guint64 frame_id = choice->frame_id;
  g_autofree char *username = g_strdup (choice->username);

  gtk_popover_popdown (GTK_POPOVER (choice->popover));
  ephy_web_view_select_password_user (view, frame_id, username);
}

static void
on_manage_passwords_clicked (GtkButton   *button,
                             EphyWebView *view)
{
  ephy_password_popover_dismiss (view);
  gtk_widget_activate_action (GTK_WIDGET (view), "win.passwords", NULL);
}

void
ephy_password_popover_show_users (EphyWebView        *view,
                                  guint64             frame_id,
                                  const char         *origin,
                                  const char * const *usernames,
                                  const char         *selected_username,
                                  int                 element_x,
                                  int                 element_y,
                                  int                 element_width,
                                  int                 element_height)
{
  GtkWidget *popover;
  GtkWidget *box;
  GtkWidget *header;
  GtkWidget *manage_btn;
  GtkWidget *manage_label;
  g_autoptr (GUri) uri = NULL;
  const char *hostname = NULL;

  g_assert (EPHY_IS_WEB_VIEW (view));

  if (!gtk_widget_get_mapped (GTK_WIDGET (view)))
    return;

  if (!usernames || !usernames[0])
    return;

  if (origin) {
    uri = g_uri_parse (origin, G_URI_FLAGS_NONE, NULL);
    if (uri)
      hostname = g_uri_get_host (uri);
  }
  if (!hostname || !*hostname)
    hostname = origin;

  popover = create_popover_for_view (view, element_x, element_y, element_width, element_height);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_size_request (box, 240, -1);

  header = gtk_label_new (_("Saved Passwords"));
  gtk_label_set_xalign (GTK_LABEL (header), 0.0f);
  gtk_widget_set_margin_start (header, 8);
  gtk_widget_set_margin_end (header, 8);
  gtk_widget_set_margin_top (header, 4);
  gtk_widget_set_margin_bottom (header, 2);
  gtk_widget_add_css_class (header, "dim-label");
  gtk_widget_add_css_class (header, "caption-heading");
  gtk_box_append (GTK_BOX (box), header);

  gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  for (gsize i = 0; usernames[i] != NULL; i++) {
    GtkWidget *button;
    GtkWidget *row_box;
    GtkWidget *icon;
    GtkWidget *text_box;
    GtkWidget *title_label;
    UserChoiceData *choice;

    button = gtk_button_new ();
    gtk_widget_add_css_class (button, "flat");

    row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top (row_box, 2);
    gtk_widget_set_margin_bottom (row_box, 2);

    icon = gtk_image_new_from_icon_name ("dialog-password-symbolic");
    gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
    gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (row_box), icon);

    text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand (text_box, TRUE);

    title_label = gtk_label_new (usernames[i]);
    gtk_label_set_xalign (GTK_LABEL (title_label), 0.0f);
    gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (title_label), 32);
    gtk_widget_add_css_class (title_label, "heading");
    gtk_box_append (GTK_BOX (text_box), title_label);

    if (hostname && *hostname) {
      GtkWidget *origin_label = gtk_label_new (hostname);
      gtk_label_set_xalign (GTK_LABEL (origin_label), 0.0f);
      gtk_label_set_ellipsize (GTK_LABEL (origin_label), PANGO_ELLIPSIZE_END);
      gtk_label_set_max_width_chars (GTK_LABEL (origin_label), 32);
      gtk_widget_add_css_class (origin_label, "dim-label");
      gtk_widget_add_css_class (origin_label, "caption");
      gtk_box_append (GTK_BOX (text_box), origin_label);
    }

    gtk_box_append (GTK_BOX (row_box), text_box);
    gtk_button_set_child (GTK_BUTTON (button), row_box);

    choice = g_new0 (UserChoiceData, 1);
    choice->view = view;
    choice->popover = popover;
    choice->frame_id = frame_id;
    choice->username = g_strdup (usernames[i]);

    g_signal_connect_data (button, "clicked",
                           G_CALLBACK (on_user_button_clicked),
                           choice, user_choice_data_free, G_CONNECT_DEFAULT);

    gtk_box_append (GTK_BOX (box), button);

    if (selected_username && g_strcmp0 (usernames[i], selected_username) == 0)
      gtk_popover_set_default_widget (GTK_POPOVER (popover), button);
  }

  gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  manage_btn = gtk_button_new ();
  gtk_widget_add_css_class (manage_btn, "flat");
  manage_label = gtk_label_new (_("Manage Passwords…"));
  gtk_label_set_xalign (GTK_LABEL (manage_label), 0.0f);
  gtk_button_set_child (GTK_BUTTON (manage_btn), manage_label);
  g_signal_connect_object (manage_btn, "clicked",
                           G_CALLBACK (on_manage_passwords_clicked),
                           view, G_CONNECT_DEFAULT);
  gtk_box_append (GTK_BOX (box), manage_btn);

  gtk_popover_set_child (GTK_POPOVER (popover), box);
  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
on_generate_accept_clicked (GtkButton            *button,
                            GeneratePasswordData *data)
{
  EphyWebView *view = data->view;
  guint64 frame_id = data->frame_id;
  g_autofree char *password = g_strdup (data->password);

  gtk_popover_popdown (GTK_POPOVER (data->popover));
  if (password)
    ephy_web_view_fill_generated_password (view, frame_id, password);
}

static void
on_generate_refresh_clicked (GtkButton            *button,
                             GeneratePasswordData *data)
{
  char *new_password = generate_secure_password ();

  if (!new_password)
    return;

  g_free (data->password);
  data->password = new_password;
  gtk_label_set_text (GTK_LABEL (data->password_label), data->password);
}

void
ephy_password_popover_show_generate (EphyWebView *view,
                                     guint64      frame_id,
                                     int          element_x,
                                     int          element_y,
                                     int          element_width,
                                     int          element_height)
{
  GtkWidget *popover;
  GtkWidget *row_box;
  GtkWidget *accept_btn;
  GtkWidget *btn_box;
  GtkWidget *icon;
  GtkWidget *text_box;
  GtkWidget *title_label;
  GtkWidget *password_label;
  GtkWidget *refresh_btn;
  GeneratePasswordData *data;
  char *password;

  g_assert (EPHY_IS_WEB_VIEW (view));

  if (!gtk_widget_get_mapped (GTK_WIDGET (view)))
    return;

  password = generate_secure_password ();
  if (!password)
    return;

  popover = create_popover_for_view (view, element_x, element_y, element_width, element_height);

  row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

  accept_btn = gtk_button_new ();
  gtk_widget_add_css_class (accept_btn, "flat");
  gtk_widget_set_hexpand (accept_btn, TRUE);

  btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_margin_top (btn_box, 2);
  gtk_widget_set_margin_bottom (btn_box, 2);

  icon = gtk_image_new_from_icon_name ("dialog-password-symbolic");
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
  gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (btn_box), icon);

  text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  title_label = gtk_label_new (_("Use and Save Suggested Password"));
  gtk_label_set_xalign (GTK_LABEL (title_label), 0.0f);
  gtk_widget_add_css_class (title_label, "heading");
  gtk_box_append (GTK_BOX (text_box), title_label);

  password_label = gtk_label_new (password);
  gtk_label_set_xalign (GTK_LABEL (password_label), 0.0f);
  gtk_widget_add_css_class (password_label, "dim-label");
  gtk_widget_add_css_class (password_label, "monospace");
  gtk_widget_add_css_class (password_label, "caption");
  gtk_box_append (GTK_BOX (text_box), password_label);

  gtk_box_append (GTK_BOX (btn_box), text_box);
  gtk_button_set_child (GTK_BUTTON (accept_btn), btn_box);
  gtk_box_append (GTK_BOX (row_box), accept_btn);

  refresh_btn = gtk_button_new_from_icon_name ("view-refresh-symbolic");
  gtk_widget_add_css_class (refresh_btn, "flat");
  gtk_widget_set_valign (refresh_btn, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (refresh_btn, _("Generate new password"));
  gtk_box_append (GTK_BOX (row_box), refresh_btn);

  data = g_new0 (GeneratePasswordData, 1);
  data->view = view;
  data->popover = popover;
  data->password_label = password_label;
  data->frame_id = frame_id;
  data->password = password;

  g_object_set_data_full (G_OBJECT (popover), "generate-data", data,
                          (GDestroyNotify)generate_password_data_free);

  g_signal_connect (accept_btn, "clicked", G_CALLBACK (on_generate_accept_clicked), data);
  g_signal_connect (refresh_btn, "clicked", G_CALLBACK (on_generate_refresh_clicked), data);

  gtk_popover_set_child (GTK_POPOVER (popover), row_box);
  gtk_popover_popup (GTK_POPOVER (popover));
}
