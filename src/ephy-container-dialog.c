/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2018-2022 Jan-Michael Brummer
 *  Copyright © 2019 Purism SPC
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
#include "ephy-container-dialog.h"

#include "ephy-containers.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-favicon-helpers.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-uri-helpers.h"
#include "ephy-time-helpers.h"
#include "ephy-window.h"

#include <adwaita.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <time.h>

struct _EphyContainerDialog {
  AdwDialog parent_instance;

  GCancellable *cancellable;

  /* UI Elements */
  GtkWidget *header_bars_stack;
  GtkWidget *color_box;
  GtkWidget *container_entry;

  EphyWindow *parent_window;
};

G_DEFINE_FINAL_TYPE (EphyContainerDialog, ephy_container_dialog, ADW_TYPE_DIALOG)

/*static GParamSpec *obj_properties[LAST_PROP]; */

static gboolean add_urls_source (EphyContainerDialog *self);
static void set_is_selection_empty (EphyContainerDialog *self,
                                    gboolean             is_selection_empty);
static GList *get_checked_rows (EphyContainerDialog *self);

EphyWindow *
ephy_container_dialog_get_parent_window (EphyContainerDialog *self)
{
  return self->parent_window;
}

void
ephy_container_dialog_set_parent_window (EphyContainerDialog *self,
                                         EphyWindow          *window)
{
  self->parent_window = window;
}

static void
create_button_pressed_cb (GtkGesture *gesture)
{
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
create_button_released_cb (GtkGesture         *gesture,
                           int                 n_click,
                           double              x,
                           double              y,
                           EphyContainerDialog *container_dialog)
{
  GtkEditable *container_entry = GTK_EDITABLE (container_dialog->container_entry);
  const char *container_name = gtk_editable_get_text (container_entry);
  
  if (!try_create_container (container_name, "blue")) {
    // TODO: Put a toast
  }
  
  adw_dialog_close (ADW_DIALOG (container_dialog));
}

static void
ephy_container_dialog_finalize (GObject *object)
{
  EphyContainerDialog *self = EPHY_CONTAINER_DIALOG (object);
}

static void
ephy_container_dialog_constructed (GObject *object)
{
  EphyContainerDialog *self = EPHY_CONTAINER_DIALOG (object);

  static const char *colors[] = { "blue", "teal", "green", "yellow", "orange", "red", "pink", "purple", "slate", NULL };

  GtkWidget *root = gtk_toggle_button_new ();
  for (const char **color = colors; *color; color++) {
    GtkWidget *swatch = (color == colors) ? root : gtk_toggle_button_new ();
    if (color != colors){
      gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (swatch), GTK_TOGGLE_BUTTON (root));
    }
    gtk_widget_add_css_class (GTK_WIDGET (swatch), "toggle");
    gtk_widget_add_css_class (GTK_WIDGET (swatch), "accent-button");
    gtk_widget_add_css_class (GTK_WIDGET (swatch), *color);
    gtk_box_append (GTK_BOX (self->color_box), GTK_WIDGET (swatch));
  }
}

static void
ephy_container_dialog_class_init (EphyContainerDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ephy_container_dialog_constructed;
  object_class->finalize = ephy_container_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/container-dialog.ui");
                                               
  gtk_widget_class_bind_template_callback (widget_class,
                                           create_button_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class,
                                           create_button_released_cb);

  /* UI Elements */
  gtk_widget_class_bind_template_child (widget_class, EphyContainerDialog, color_box);
  gtk_widget_class_bind_template_child (widget_class, EphyContainerDialog, container_entry);
}

GtkWidget *
ephy_container_dialog_new ()
{
  EphyContainerDialog *self;

  self = g_object_new (EPHY_TYPE_CONTAINER_DIALOG, NULL);

  return GTK_WIDGET (self);
}

static void
ephy_container_dialog_init (EphyContainerDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
