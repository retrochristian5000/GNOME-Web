/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2025 Jan-Michael Brummer
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

#include "ephy-passwords-view.h"

#include "ephy-shell.h"

struct _EphyPasswordsView {
  EphyDataView parent_instance;

  GtkWidget *remember_switch;

  EphyPasswordManager *manager;
  GList *records;
  GtkWidget *edit_button;
  GtkWidget *remove_button;
  GtkWidget *toast_overlay;
  GtkWidget *listbox;
  GtkWidget *confirmation_dialog;

  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (EphyPasswordsView, ephy_passwords_view, EPHY_TYPE_DATA_VIEW)

static void populate_model (EphyPasswordsView *passwords_view);
static GtkWidget *confirmation_dialog_construct (EphyPasswordsView *self);

static void
on_search_text_changed (EphyPasswordsView *passwords_view)
{
  ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (passwords_view), FALSE);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (passwords_view->listbox));
}

static gboolean
on_clear_button_clicked (EphyPasswordsView *self)
{
  if (!ephy_data_view_get_has_data (EPHY_DATA_VIEW (self)))
    return GDK_EVENT_PROPAGATE;

  if (!self->confirmation_dialog) {
    GtkWidget **confirmation_dialog;

    self->confirmation_dialog = confirmation_dialog_construct (self);
    confirmation_dialog = &self->confirmation_dialog;
    g_object_add_weak_pointer (G_OBJECT (self->confirmation_dialog),
                               (gpointer *)confirmation_dialog);
  }

  adw_dialog_present (ADW_DIALOG (self->confirmation_dialog), GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));

  return GDK_EVENT_STOP;
}

static void
forget_operation_finished_cb (GObject      *source_object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  EphyPasswordsView *passwords_view = user_data;
  g_autoptr (GError) error = NULL;

  if (!ephy_password_manager_forget_finish (EPHY_PASSWORD_MANAGER (source_object), result, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to forget password: %s", error->message);
    return;
  }

  /* Repopulate the list */
  ephy_data_view_set_has_data (EPHY_DATA_VIEW (passwords_view), FALSE);
  populate_model (passwords_view);
}

static void
on_edit_button_toggled (GtkToggleButton *button,
                        gpointer         user_data)
{
  EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (user_data);
  gboolean active = gtk_toggle_button_get_active (button);
  GtkListBoxRow *row;
  int i = 0;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++))) {
    GtkWidget *check_button = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "check-button"));

    /* Uncheck all rows when toggling selection mode */
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check_button), FALSE);

    /* Show/Hide row selection widgets (check_button) */
    gtk_widget_set_visible (check_button, active);
  }

  gtk_widget_set_sensitive (self->remove_button, FALSE);
  ephy_data_view_set_reveal_bottom_bar (EPHY_DATA_VIEW (self), active);
}

static void
on_remove_button_clicked (GtkWidget *button,
                          gpointer   user_data)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (user_data);
  GtkListBoxRow *row;
  int i = 0;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (passwords_view->listbox), i++))) {
    GtkCheckButton *check_button =
      GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (row), "check-button"));

    if (gtk_check_button_get_active (check_button)) {
      EphyPasswordRecord *record = g_object_get_data (G_OBJECT (row), "record");

      ephy_password_manager_forget (passwords_view->manager,
                                    ephy_password_record_get_id (record),
                                    passwords_view->cancellable,
                                    forget_operation_finished_cb,
                                    passwords_view);
    }
  }

  /* Clear internal state */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (passwords_view->edit_button), FALSE);
  gtk_list_box_remove_all (GTK_LIST_BOX (passwords_view->listbox));
  g_clear_list (&passwords_view->records, g_object_unref);

  /* Present loading spinner while waiting for the async forget op to finish */
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view), TRUE);
}


static void
copy_password_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  const char *password = user_data;

  if (password) {
    EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (gtk_widget_get_ancestor (button, EPHY_TYPE_PASSWORDS_VIEW));
    AdwToastOverlay *toast_overlay = ephy_data_view_get_toast_overlay (EPHY_DATA_VIEW (self));
    AdwToast *toast = adw_toast_new (_("Password copied"));

    gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), password);
    adw_toast_overlay_add_toast (toast_overlay, toast);
  }
}

static void
copy_username_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  const char *username = user_data;

  if (username)
    gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), username);
}

static void
ephy_passwords_view_class_init (EphyPasswordsViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/passwords-view.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsView, edit_button);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsView, listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsView, remove_button);

  gtk_widget_class_bind_template_callback (widget_class, on_search_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_edit_button_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_button_clicked);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Delete, GDK_SHIFT_MASK,
                                (GtkShortcutFunc)on_clear_button_clicked,
                                NULL);
}

static void
confirmation_dialog_response_cb (EphyPasswordsView *self)
{
  ephy_password_manager_forget_all (self->manager);

  gtk_list_box_remove_all (GTK_LIST_BOX (self->listbox));
  ephy_data_view_set_has_data (EPHY_DATA_VIEW (self), FALSE);

  g_clear_list (&self->records, g_object_unref);
}

static GtkWidget *
confirmation_dialog_construct (EphyPasswordsView *self)
{
  AdwDialog *dialog;

  dialog = adw_alert_dialog_new (_("Delete All Passwords?"),
                                 _("This will clear all locally stored passwords, and can not be undone."));

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "delete", _("_Delete"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "delete",
                                            ADW_RESPONSE_DESTRUCTIVE);

  g_signal_connect_swapped (dialog, "response::delete",
                            G_CALLBACK (confirmation_dialog_response_cb),
                            self);

  return GTK_WIDGET (dialog);
}

static void
on_username_apply (AdwEntryRow *row,
                   gpointer     user_data)
{
  EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (user_data);
  GtkWidget *expander_row = gtk_widget_get_ancestor (GTK_WIDGET (row), ADW_TYPE_EXPANDER_ROW);
  EphyPasswordRecord *record = g_object_get_data (G_OBJECT (expander_row), "record");

  ephy_password_manager_save (self->manager,
                              ephy_password_record_get_origin (record),
                              ephy_password_record_get_target_origin (record),
                              ephy_password_record_get_username (record),
                              gtk_editable_get_text (GTK_EDITABLE (row)),
                              ephy_password_record_get_password (record),
                              ephy_password_record_get_username_field (record),
                              ephy_password_record_get_password_field (record),
                              FALSE);
}

static void
on_password_apply (AdwEntryRow *row,
                   gpointer     user_data)
{
  EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (user_data);
  GtkWidget *expander_row = gtk_widget_get_ancestor (GTK_WIDGET (row), ADW_TYPE_EXPANDER_ROW);
  EphyPasswordRecord *record = g_object_get_data (G_OBJECT (expander_row), "record");

  ephy_password_manager_save (self->manager,
                              ephy_password_record_get_origin (record),
                              ephy_password_record_get_target_origin (record),
                              ephy_password_record_get_username (record),
                              ephy_password_record_get_username (record),
                              gtk_editable_get_text (GTK_EDITABLE (row)),
                              ephy_password_record_get_username_field (record),
                              ephy_password_record_get_password_field (record),
                              FALSE);
}

static GList *
get_checked_rows (EphyPasswordsView *self)
{
  GList *checked_rows = NULL;
  GtkListBoxRow *row;
  int i = 0;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++))) {
    GtkCheckButton *check_button =
      GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (row), "check-button"));

    if (gtk_check_button_get_active (check_button))
      checked_rows = g_list_prepend (checked_rows, row);
  }

  return checked_rows;
}

static void
row_check_button_toggled (EphyPasswordsView *self)
{
  g_autoptr (GList) checked_rows = get_checked_rows (self);

  gtk_widget_set_sensitive (self->remove_button, g_list_length (checked_rows));
}

static void
populate_model_cb (GList    **records,
                   gpointer   user_data)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (user_data);

  /* We don't need to update the model if the amount of passwords hasn't changed. */
  if (g_list_length (*records) == g_list_length (passwords_view->records))
    return;

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view), TRUE);
  gtk_list_box_remove_all (GTK_LIST_BOX (passwords_view->listbox));
  ephy_data_view_set_has_data (EPHY_DATA_VIEW (passwords_view), FALSE);

  for (GList *l = *records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);
    GtkWidget *row;
    GtkWidget *sub_row;
    GtkWidget *button;
    const char *text;
    const char *username;

    row = adw_expander_row_new ();
    g_object_set_data (G_OBJECT (row), "record", record);
    adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), ephy_password_record_get_origin (record));
    username = ephy_password_record_get_username (record);
    if (username)
      adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (row), username);
    adw_expander_row_set_show_enable_switch (ADW_EXPANDER_ROW (row), FALSE);

    button = gtk_check_button_new ();
    g_object_set_data (G_OBJECT (row), "check-button", button);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (button, _("Remove the selected passwords"));
    gtk_widget_add_css_class (button, "selection-mode");
    gtk_widget_set_visible (button, FALSE);
    g_signal_connect_object (button, "toggled", G_CALLBACK (row_check_button_toggled),
                             passwords_view, G_CONNECT_SWAPPED);
    adw_expander_row_add_prefix (ADW_EXPANDER_ROW (row), button);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic");
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (button, _("Copy Password"));
    gtk_widget_add_css_class (button, "flat");
    adw_expander_row_add_suffix (ADW_EXPANDER_ROW (row), button);
    g_signal_connect (button, "clicked", G_CALLBACK (copy_password_clicked), (void *)(ephy_password_record_get_password (record)));

    /* Username */
    sub_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (sub_row), _("Username"));
    adw_entry_row_set_show_apply_button (ADW_ENTRY_ROW (sub_row), TRUE);
    g_signal_connect (sub_row, "apply", G_CALLBACK (on_username_apply), passwords_view);
    adw_expander_row_add_row (ADW_EXPANDER_ROW (row), sub_row);

    text = ephy_password_record_get_username (record);
    if (text)
      gtk_editable_set_text (GTK_EDITABLE (sub_row), text);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic");
    g_signal_connect (button, "clicked", G_CALLBACK (copy_username_clicked), (void *)(ephy_password_record_get_username (record)));
    gtk_widget_set_tooltip_text (button, _("Copy Username"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (button, "flat");
    adw_entry_row_add_suffix (ADW_ENTRY_ROW (sub_row), button);

    /* Password */
    sub_row = adw_password_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (sub_row), _("Password"));
    adw_entry_row_set_show_apply_button (ADW_ENTRY_ROW (sub_row), TRUE);
    g_signal_connect (sub_row, "apply", G_CALLBACK (on_password_apply), passwords_view);
    adw_expander_row_add_row (ADW_EXPANDER_ROW (row), sub_row);

    text = ephy_password_record_get_password (record);
    if (text)
      gtk_editable_set_text (GTK_EDITABLE (sub_row), text);

    g_object_set_data (G_OBJECT (row), "record", record);

    gtk_list_box_append (GTK_LIST_BOX (passwords_view->listbox), row);
  }

  if (g_list_length (*records))
    ephy_data_view_set_has_data (EPHY_DATA_VIEW (passwords_view), TRUE);

  gtk_widget_set_sensitive (passwords_view->edit_button,
                            ephy_data_view_get_has_data (EPHY_DATA_VIEW (passwords_view)));
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view), FALSE);

  g_list_free_full (passwords_view->records, g_object_unref);
  passwords_view->records = g_list_copy_deep (*records, (GCopyFunc)g_object_ref, NULL);
}

static void
populate_model (EphyPasswordsView *self)
{
  g_assert (EPHY_IS_PASSWORDS_VIEW (self));

  /* Ask for all password records. */
  ephy_password_manager_query (self->manager,
                               NULL, NULL, NULL, NULL, NULL, NULL,
                               FALSE,
                               populate_model_cb, self);
}

static gboolean
password_filter (GtkListBoxRow *row,
                 gpointer       user_data)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (user_data);
  EphyPasswordRecord *record = g_object_get_data (G_OBJECT (row), "record");
  const char *username;
  const char *origin;
  gboolean visible = FALSE;
  const char *search_text = ephy_data_view_get_search_text (EPHY_DATA_VIEW (passwords_view));

  if (!search_text)
    return TRUE;

  origin = ephy_password_record_get_origin (record);
  username = ephy_password_record_get_username (record);

  if (origin && g_strrstr (origin, search_text))
    visible = TRUE;
  else if (username && g_strrstr (username, search_text))
    visible = TRUE;

  if (visible)
    ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (passwords_view), TRUE);

  return visible;
}

static void
ephy_passwords_view_init (EphyPasswordsView *self)
{
  self->manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));

  gtk_widget_init_template (GTK_WIDGET (self));

  ephy_data_view_set_show_title (EPHY_DATA_VIEW (self), FALSE);
  ephy_data_view_set_reveal_bottom_bar (EPHY_DATA_VIEW (self), FALSE);

  self->cancellable = g_cancellable_new ();

  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->listbox), password_filter, self, NULL);

  populate_model (self);

  g_signal_connect_swapped (self->manager, "synchronizable-modified",
                            G_CALLBACK (populate_model), self);
  g_signal_connect_swapped (self->manager, "synchronizable-deleted",
                            G_CALLBACK (populate_model), self);
}

void
ephy_passwords_show (EphyWindow *window)
{
  GtkWidget *dialog = g_object_new (EPHY_TYPE_PASSWORDS_VIEW, NULL);

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
}

void
ephy_passwords_view_reset_state (EphyPasswordsView *self)
{
  GtkListBoxRow *row;
  int i = 0;

  g_assert (EPHY_IS_PASSWORDS_VIEW (self));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->edit_button), FALSE);
  ephy_data_view_reset_search (EPHY_DATA_VIEW (self));

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++)))
    adw_expander_row_set_expanded (ADW_EXPANDER_ROW (row), FALSE);
}
