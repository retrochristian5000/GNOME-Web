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

#include "config.h"

#include "ephy-sidebar.h"
#include "ephy-bookmarks-dialog.h"
#include "ephy-history-dialog.h"
#include "ephy-passwords-view.h"

struct _EphySidebar {
  AdwBin parent_instance;

  GtkWidget *stack;
  GtkWidget *bookmarks;
  GtkWidget *history;
  GtkWidget *downloads;
  GtkWidget *passwords;
};

G_DEFINE_FINAL_TYPE (EphySidebar, ephy_sidebar, ADW_TYPE_BIN)

static void
ephy_sidebar_class_init (EphySidebarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/sidebar.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySidebar, stack);
  gtk_widget_class_bind_template_child (widget_class, EphySidebar, bookmarks);
  gtk_widget_class_bind_template_child (widget_class, EphySidebar, history);
  gtk_widget_class_bind_template_child (widget_class, EphySidebar, downloads);
  gtk_widget_class_bind_template_child (widget_class, EphySidebar, passwords);
}

static void
ephy_sidebar_init (EphySidebar *self)
{
  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();

  gtk_widget_init_template (GTK_WIDGET (self));

  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    adw_view_stack_remove (ADW_VIEW_STACK (self->stack), self->bookmarks);
    adw_view_stack_remove (ADW_VIEW_STACK (self->stack), self->downloads);
    adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (self->stack), "history");
  }
}

void
ephy_sidebar_set_visible_child_name (EphySidebar *self,
                                     const char  *name)
{
  g_assert (EPHY_IS_SIDEBAR (self));

  adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (self->stack), name);
}

void
ephy_sidebar_reset_state (EphySidebar *self)
{
  g_assert (EPHY_IS_SIDEBAR (self));

  ephy_bookmarks_dialog_reset_state (EPHY_BOOKMARKS_DIALOG (self->bookmarks));
  ephy_history_dialog_reset_state (EPHY_HISTORY_DIALOG (self->history));
  ephy_passwords_view_reset_state (EPHY_PASSWORDS_VIEW (self->passwords));
}
