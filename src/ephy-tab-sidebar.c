/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Christophe Jean
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

/* A vertical list of the pages of an AdwTabView.
 *
 * Only the public AdwTabView/AdwTabPage API is used, so that this widget can
 * later move to libadwaita. As in AdwTabBar, pinned pages are shown apart,
 * as icons, and the tab context menu is the view's menu model, set up through
 * AdwTabView::setup-menu. Accessibility mirrors AdwTabBar: each group of
 * pages is a tab list and each page is a tab.
 */

#include "config.h"
#include "ephy-tab-sidebar.h"

#include <glib/gi18n.h>

/* A scrolled window whose content fades out on the edges past which there is
 * more to scroll, as AdwTabBox does horizontally (see adw_tab_box_snapshot). */

#define FADE_OFFSET 6.0f
#define FADE_WIDTH 36.0f

#define EPHY_TYPE_TAB_SIDEBAR_SCROLLER (ephy_tab_sidebar_scroller_get_type ())

G_DECLARE_FINAL_TYPE (EphyTabSidebarScroller, ephy_tab_sidebar_scroller, EPHY, TAB_SIDEBAR_SCROLLER, GtkWidget)

struct _EphyTabSidebarScroller {
  GtkWidget parent_instance;

  GtkScrolledWindow *scrolled_window;
};

G_DEFINE_FINAL_TYPE (EphyTabSidebarScroller, ephy_tab_sidebar_scroller, GTK_TYPE_WIDGET)

static void
append_fade (GtkSnapshot *snapshot,
             float        width,
             float        from,
             float        to,
             float        opacity)
{
  GskColorStop stops[] = {
    { 0, { 0, 0, 0, opacity } },
    { 1, { 0, 0, 0, 0 } },
  };

  gtk_snapshot_append_linear_gradient (snapshot,
                                       &GRAPHENE_RECT_INIT (0, MIN (from, to) - FADE_OFFSET,
                                                            width, FADE_OFFSET + FADE_WIDTH + FADE_OFFSET),
                                       &GRAPHENE_POINT_INIT (0, from),
                                       &GRAPHENE_POINT_INIT (0, to),
                                       stops,
                                       G_N_ELEMENTS (stops));
}

static void
ephy_tab_sidebar_scroller_snapshot (GtkWidget   *widget,
                                    GtkSnapshot *snapshot)
{
  EphyTabSidebarScroller *self = EPHY_TAB_SIDEBAR_SCROLLER (widget);
  GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment (self->scrolled_window);
  double value = gtk_adjustment_get_value (adjustment);
  double page_size = gtk_adjustment_get_page_size (adjustment);
  double upper = gtk_adjustment_get_upper (adjustment);
  gboolean fade_top = value > 0;
  gboolean fade_bottom = value + page_size < upper;
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);

  if (fade_top || fade_bottom) {
    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_ALPHA);

    if (fade_top)
      append_fade (snapshot, width, FADE_OFFSET, FADE_OFFSET + FADE_WIDTH,
                   CLAMP (value / FADE_OFFSET, 0, 1));

    if (fade_bottom)
      append_fade (snapshot, width, height - FADE_OFFSET, height - FADE_OFFSET - FADE_WIDTH,
                   CLAMP ((upper - value - page_size) / FADE_OFFSET, 0, 1));

    gtk_snapshot_pop (snapshot);
  }

  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->scrolled_window), snapshot);

  if (fade_top || fade_bottom)
    gtk_snapshot_pop (snapshot);
}

static void
ephy_tab_sidebar_scroller_dispose (GObject *object)
{
  EphyTabSidebarScroller *self = EPHY_TAB_SIDEBAR_SCROLLER (object);

  g_clear_pointer ((GtkWidget **)&self->scrolled_window, gtk_widget_unparent);

  G_OBJECT_CLASS (ephy_tab_sidebar_scroller_parent_class)->dispose (object);
}

static void
ephy_tab_sidebar_scroller_class_init (EphyTabSidebarScrollerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_tab_sidebar_scroller_dispose;
  widget_class->snapshot = ephy_tab_sidebar_scroller_snapshot;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
ephy_tab_sidebar_scroller_init (EphyTabSidebarScroller *self)
{
  GtkAdjustment *adjustment;

  self->scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  gtk_scrolled_window_set_policy (self->scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_height (self->scrolled_window, TRUE);
  gtk_widget_set_parent (GTK_WIDGET (self->scrolled_window), GTK_WIDGET (self));

  adjustment = gtk_scrolled_window_get_vadjustment (self->scrolled_window);
  g_signal_connect_object (adjustment, "value-changed", G_CALLBACK (gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (adjustment, "changed", G_CALLBACK (gtk_widget_queue_draw), self, G_CONNECT_SWAPPED);
}

static GtkWidget *
ephy_tab_sidebar_scroller_new (GtkWidget *child)
{
  EphyTabSidebarScroller *self = g_object_new (EPHY_TYPE_TAB_SIDEBAR_SCROLLER, NULL);

  gtk_scrolled_window_set_child (self->scrolled_window, child);

  return GTK_WIDGET (self);
}

struct _EphyTabSidebar {
  AdwBin parent_instance;

  GtkFlowBox *pinned_box;
  GtkListBox *list;
  GtkWidget *context_menu;

  AdwTabView *view;
  GListModel *pinned_pages;
  GListModel *unpinned_pages;

  gboolean syncing_selection;
  guint reset_setup_menu_id;
  GtkWidget *pinned_scrolled_window;
  GtkWidget *scrolled_window;
  GtkWidget *pinned_separator;

  AdwTabPage *drag_page;
  gboolean drag_start_pinned;
  int drag_start_position;
  double drag_x;
  double drag_y;
  guint scroll_tick_id;
  guint scroll_ticks;
};

G_DEFINE_FINAL_TYPE (EphyTabSidebar, ephy_tab_sidebar, ADW_TYPE_BIN)

typedef enum {
  PROP_VIEW = 1,
} EphyTabSidebarProps;

static GParamSpec *obj_properties[PROP_VIEW + 1];

enum {
  CREATE_TAB,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

#define PAGE_KEY "ephy-tab-sidebar-page"

static AdwTabPage *
get_page (GtkWidget *tab)
{
  return g_object_get_data (G_OBJECT (tab), PAGE_KEY);
}

/* Like AdwTab: the close button only shows on the selected or hovered tab. */
static void
update_close_button_cb (GtkWidget     *row,
                        GtkStateFlags  previous_flags,
                        GtkWidget     *close_button)
{
  gboolean show_close = (gtk_widget_get_state_flags (row) & (GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_PRELIGHT)) != 0;

  gtk_widget_set_opacity (close_button, show_close ? 1 : 0);
  gtk_widget_set_can_target (close_button, show_close);
  gtk_widget_set_can_focus (close_button, show_close);
  gtk_accessible_update_state (GTK_ACCESSIBLE (close_button),
                               GTK_ACCESSIBLE_STATE_HIDDEN, !show_close,
                               -1);
}

static gboolean
icon_transform_cb (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   AdwTabView   *view)
{
  GIcon *icon = g_value_get_object (from_value);

  g_value_set_object (to_value, icon ? icon : adw_tab_view_get_default_icon (view));

  return TRUE;
}

static GtkWidget *
create_icon (AdwTabPage *page,
             AdwTabView *view)
{
  GtkWidget *icon = gtk_image_new ();

  g_object_bind_property_full (page, "icon", icon, "gicon", G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc)icon_transform_cb, NULL,
                               g_object_ref (view), g_object_unref);

  return icon;
}

/* Context menu */

static void
reset_setup_menu_cb (EphyTabSidebar *self)
{
  self->reset_setup_menu_id = 0;

  if (self->view)
    g_signal_emit_by_name (self->view, "setup-menu", NULL);
}

static void
context_menu_notify_visible_cb (EphyTabSidebar *self)
{
  if (gtk_widget_get_visible (self->context_menu))
    return;

  /* Deferred, as AdwTabBar does, so that the activated menu action still sees
   * the page it was set up for. */
  g_clear_handle_id (&self->reset_setup_menu_id, g_source_remove);
  self->reset_setup_menu_id = g_idle_add_once ((GSourceOnceFunc)reset_setup_menu_cb, self);
}

/* x and y are in tab coordinates; negative means below the tab, for keyboard
 * invocation. */
static void
popup_context_menu (EphyTabSidebar *self,
                    GtkWidget      *tab,
                    double          x,
                    double          y)
{
  GMenuModel *model;
  graphene_point_t point;
  GdkRectangle rect;

  if (!self->view)
    return;

  model = adw_tab_view_get_menu_model (self->view);
  if (!G_IS_MENU_MODEL (model))
    return;

  if (x < 0 || y < 0) {
    x = 0;
    y = gtk_widget_get_height (tab);
  }

  if (!gtk_widget_compute_point (tab, GTK_WIDGET (self), &GRAPHENE_POINT_INIT (x, y), &point))
    return;

  g_clear_handle_id (&self->reset_setup_menu_id, g_source_remove);
  g_signal_emit_by_name (self->view, "setup-menu", get_page (tab));

  if (!self->context_menu) {
    self->context_menu = gtk_popover_menu_new_from_model (model);
    gtk_widget_set_parent (self->context_menu, GTK_WIDGET (self));
    gtk_popover_set_position (GTK_POPOVER (self->context_menu), GTK_POS_BOTTOM);
    gtk_popover_set_has_arrow (GTK_POPOVER (self->context_menu), FALSE);
    gtk_widget_set_halign (self->context_menu, GTK_ALIGN_START);
    g_signal_connect_object (self->context_menu, "notify::visible",
                             G_CALLBACK (context_menu_notify_visible_cb), self,
                             G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  } else {
    gtk_popover_menu_set_menu_model (GTK_POPOVER_MENU (self->context_menu), model);
  }

  rect.x = (int)point.x;
  rect.y = (int)point.y;
  rect.width = rect.height = 0;
  gtk_popover_set_pointing_to (GTK_POPOVER (self->context_menu), &rect);
  gtk_popover_popup (GTK_POPOVER (self->context_menu));
}

static GtkWidget *
get_selected_tab (EphyTabSidebar *self)
{
  g_autoptr (GList) children = gtk_flow_box_get_selected_children (self->pinned_box);
  GtkListBoxRow *row = gtk_list_box_get_selected_row (self->list);

  if (row)
    return GTK_WIDGET (row);

  return children ? children->data : NULL;
}

static void
popup_menu_action_cb (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
  EphyTabSidebar *self = EPHY_TAB_SIDEBAR (widget);
  GtkWidget *tab = get_selected_tab (self);

  if (tab)
    popup_context_menu (self, tab, -1, -1);
}

static void
tab_pressed_cb (GtkGestureClick *gesture,
                int              n_press,
                double           x,
                double           y,
                EphyTabSidebar  *self)
{
  GtkWidget *tab = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  GdkEvent *event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (gdk_event_triggers_context_menu (event)) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    popup_context_menu (self, tab, x, y);
  }
}

/* Like AdwTabBar, close on release: GtkGestureClick only emits it when the
 * press and release form a click on the same tab. */
static void
tab_middle_released_cb (GtkGestureClick *gesture,
                        int              n_press,
                        double           x,
                        double           y,
                        EphyTabSidebar  *self)
{
  GtkWidget *tab = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (self->view)
    adw_tab_view_close_page (self->view, get_page (tab));
}

/* Reordering by drag and drop, live like AdwTabBar: the dragged page moves
 * as soon as the pointer goes over another tab, leaving an empty slot where
 * it is, while a copy of it follows the pointer. Going over a pinned tab pins
 * the dragged page, and the other way around. Moving a page recreates its
 * tab, with its drag source, so the drag is followed on the GdkDrag. */

static void
update_dragged_tab (GtkWidget      *tab,
                    EphyTabSidebar *self)
{
  if (self->drag_page && get_page (tab) == self->drag_page)
    gtk_widget_add_css_class (tab, "dragged");
  else
    gtk_widget_remove_css_class (tab, "dragged");
}

static void
update_dragged_tabs (EphyTabSidebar *self)
{
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->pinned_box)); child; child = gtk_widget_get_next_sibling (child))
    update_dragged_tab (child, self);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->list)); child; child = gtk_widget_get_next_sibling (child))
    update_dragged_tab (child, self);
}

static void
move_dragged_page (EphyTabSidebar *self,
                   AdwTabPage     *page)
{
  gboolean pinned = adw_tab_page_get_pinned (page);

  /* AdwTabView keeps pinned pages first: change sections first, which moves
   * the page to the boundary, then reorder within the section. */
  if (adw_tab_page_get_pinned (self->drag_page) != pinned)
    adw_tab_view_set_page_pinned (self->view, self->drag_page, pinned);

  adw_tab_view_reorder_page (self->view, self->drag_page,
                             adw_tab_view_get_page_position (self->view, page));
}

static void
restore_dragged_page (EphyTabSidebar *self)
{
  int n_pinned;
  int position;

  if (adw_tab_page_get_pinned (self->drag_page) != self->drag_start_pinned)
    adw_tab_view_set_page_pinned (self->view, self->drag_page, self->drag_start_pinned);

  /* Other pages may have been closed meanwhile: stay within the section. */
  n_pinned = adw_tab_view_get_n_pinned_pages (self->view);
  if (self->drag_start_pinned)
    position = CLAMP (self->drag_start_position, 0, n_pinned - 1);
  else
    position = CLAMP (self->drag_start_position, n_pinned, adw_tab_view_get_n_pages (self->view) - 1);

  adw_tab_view_reorder_page (self->view, self->drag_page, position);
}

static void
end_drag (EphyTabSidebar *self)
{
  g_clear_object (&self->drag_page);
  update_dragged_tabs (self);
}

static void
drag_cancel_cb (EphyTabSidebar      *self,
                GdkDragCancelReason  reason)
{
  if (self->drag_page && self->view)
    restore_dragged_page (self);

  end_drag (self);
}

static GtkWidget *
create_drag_icon (EphyTabSidebar *self,
                  GtkWidget      *tab)
{
  AdwTabPage *page = get_page (tab);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  gtk_box_append (GTK_BOX (box), create_icon (page, self->view));

  if (GTK_IS_LIST_BOX_ROW (tab)) {
    GtkWidget *title = gtk_label_new (adw_tab_page_get_title (page));

    gtk_label_set_ellipsize (GTK_LABEL (title), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign (GTK_LABEL (title), 0);
    gtk_widget_set_hexpand (title, TRUE);
    gtk_box_append (GTK_BOX (box), title);
  } else {
    gtk_widget_set_halign (gtk_widget_get_first_child (box), GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (gtk_widget_get_first_child (box), TRUE);
  }

  gtk_widget_set_size_request (box, gtk_widget_get_width (tab), gtk_widget_get_height (tab));
  gtk_widget_add_css_class (box, "tab-sidebar-drag-icon");

  return box;
}

static GdkContentProvider *
tab_drag_prepare_cb (GtkDragSource  *source,
                     double          x,
                     double          y,
                     EphyTabSidebar *self)
{
  GtkWidget *tab = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));

  if (!self->view)
    return NULL;

  self->drag_x = x;
  self->drag_y = y;

  return gdk_content_provider_new_typed (ADW_TYPE_TAB_PAGE, get_page (tab));
}

static void
tab_drag_begin_cb (GtkDragSource  *source,
                   GdkDrag        *drag,
                   EphyTabSidebar *self)
{
  GtkWidget *tab = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  AdwTabPage *page = get_page (tab);

  gtk_drag_icon_set_child (GTK_DRAG_ICON (gtk_drag_icon_get_for_drag (drag)), create_drag_icon (self, tab));
  gdk_drag_set_hotspot (drag, (int)self->drag_x, (int)self->drag_y);

  g_set_object (&self->drag_page, page);
  self->drag_start_pinned = adw_tab_page_get_pinned (page);
  self->drag_start_position = adw_tab_view_get_page_position (self->view, page);
  update_dragged_tab (tab, self);

  g_signal_connect_object (drag, "cancel", G_CALLBACK (drag_cancel_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (drag, "dnd-finished", G_CALLBACK (end_drag), self, G_CONNECT_SWAPPED);
}

static GdkDragAction
tab_drop_motion_cb (GtkDropTarget  *target,
                    double          x,
                    double          y,
                    EphyTabSidebar *self)
{
  GtkWidget *tab = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (target));
  const GValue *value = gtk_drop_target_get_value (target);
  AdwTabPage *page = get_page (tab);

  /* Only pages dragged from this sidebar. */
  if (!value || !self->view || !self->drag_page || g_value_get_object (value) != self->drag_page)
    return 0;

  if (page != self->drag_page)
    move_dragged_page (self, page);

  return GDK_ACTION_MOVE;
}

static gboolean
tab_drop_cb (GtkDropTarget  *target,
             const GValue   *value,
             double          x,
             double          y,
             EphyTabSidebar *self)
{
  /* The page is already in place. */
  return self->drag_page && g_value_get_object (value) == self->drag_page;
}

/* Tabs */

/* What the tabs and the pinned tabs have in common. */
static void
setup_tab (EphyTabSidebar *self,
           GtkWidget      *tab,
           AdwTabPage     *page)
{
  GtkDragSource *drag_source = gtk_drag_source_new ();
  GtkDropTarget *drop_target = gtk_drop_target_new (ADW_TYPE_TAB_PAGE, GDK_ACTION_MOVE);
  GtkGesture *click = gtk_gesture_click_new ();
  GtkGesture *middle_click = gtk_gesture_click_new ();

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 0);
  g_signal_connect_object (click, "pressed", G_CALLBACK (tab_pressed_cb), self, G_CONNECT_DEFAULT);
  gtk_widget_add_controller (tab, GTK_EVENT_CONTROLLER (click));

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (middle_click), GDK_BUTTON_MIDDLE);
  g_signal_connect_object (middle_click, "released", G_CALLBACK (tab_middle_released_cb), self, G_CONNECT_DEFAULT);
  gtk_widget_add_controller (tab, GTK_EVENT_CONTROLLER (middle_click));

  gtk_drag_source_set_actions (drag_source, GDK_ACTION_MOVE);
  g_signal_connect_object (drag_source, "prepare", G_CALLBACK (tab_drag_prepare_cb), self, G_CONNECT_DEFAULT);
  g_signal_connect_object (drag_source, "drag-begin", G_CALLBACK (tab_drag_begin_cb), self, G_CONNECT_DEFAULT);
  gtk_widget_add_controller (tab, GTK_EVENT_CONTROLLER (drag_source));

  /* Preloaded, so that motion can check the dragged page. */
  gtk_drop_target_set_preload (drop_target, TRUE);
  g_signal_connect_object (drop_target, "motion", G_CALLBACK (tab_drop_motion_cb), self, G_CONNECT_DEFAULT);
  g_signal_connect_object (drop_target, "drop", G_CALLBACK (tab_drop_cb), self, G_CONNECT_DEFAULT);
  gtk_widget_add_controller (tab, GTK_EVENT_CONTROLLER (drop_target));

  g_object_set_data_full (G_OBJECT (tab), PAGE_KEY, g_object_ref (page), g_object_unref);
  update_dragged_tab (tab, self);
}

static void
close_clicked_cb (GtkButton      *button,
                  EphyTabSidebar *self)
{
  GtkWidget *row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);

  if (self->view)
    adw_tab_view_close_page (self->view, get_page (row));
}

static GtkWidget *
create_row (AdwTabPage     *page,
            EphyTabSidebar *self)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *title;
  GtkWidget *close_button;

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "accessible-role", GTK_ACCESSIBLE_ROLE_TAB,
                      NULL);
  setup_tab (self, row, page);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  gtk_box_append (GTK_BOX (box), create_icon (page, self->view));

  title = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (title), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (title), 0);
  gtk_widget_set_hexpand (title, TRUE);
  g_object_bind_property (page, "title", title, "label", G_BINDING_SYNC_CREATE);
  g_object_bind_property (page, "title", row, "tooltip-text", G_BINDING_SYNC_CREATE);
  gtk_box_append (GTK_BOX (box), title);

  close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
  gtk_widget_add_css_class (close_button, "flat");
  gtk_widget_add_css_class (close_button, "circular");
  gtk_widget_set_valign (close_button, GTK_ALIGN_CENTER);
  /* Taking the focus would scroll a partly visible row into view, under the
   * pointer, as the tab is being closed. */
  gtk_widget_set_focus_on_click (close_button, FALSE);
  /* Same string as the AdwTab close button: reuse libadwaita's translation. */
  gtk_widget_set_tooltip_text (close_button, g_dgettext ("libadwaita", "Close Tab"));
  g_signal_connect_object (close_button, "clicked", G_CALLBACK (close_clicked_cb), self, G_CONNECT_DEFAULT);
  gtk_box_append (GTK_BOX (box), close_button);

  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  g_signal_connect (row, "state-flags-changed", G_CALLBACK (update_close_button_cb), close_button);
  update_close_button_cb (row, 0, close_button);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, title, NULL,
                                  -1);

  return row;
}

static void
update_pinned_tab_label_cb (GtkWidget *child)
{
  const char *title = adw_tab_page_get_title (get_page (child));

  gtk_widget_set_tooltip_text (child, title);
  gtk_accessible_update_property (GTK_ACCESSIBLE (child),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, title,
                                  -1);
}

static GtkWidget *
create_pinned_tab (AdwTabPage     *page,
                   EphyTabSidebar *self)
{
  GtkWidget *child;

  child = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                        "accessible-role", GTK_ACCESSIBLE_ROLE_TAB,
                        NULL);
  setup_tab (self, child, page);

  /* Pinned tabs share the width of a line, down to this minimum width under
   * which they wrap to a new line. */
  gtk_widget_set_size_request (child, 40, -1);
  gtk_widget_set_hexpand (child, TRUE);

  gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (child), create_icon (page, self->view));

  g_signal_connect_object (page, "notify::title", G_CALLBACK (update_pinned_tab_label_cb), child, G_CONNECT_SWAPPED);
  update_pinned_tab_label_cb (child);

  return child;
}

static GtkWidget *
create_row_cb (gpointer item,
               gpointer user_data)
{
  return create_row (ADW_TAB_PAGE (item), EPHY_TAB_SIDEBAR (user_data));
}

static GtkWidget *
create_pinned_tab_cb (gpointer item,
                      gpointer user_data)
{
  return create_pinned_tab (ADW_TAB_PAGE (item), EPHY_TAB_SIDEBAR (user_data));
}

/* Selection */

#define MAX_SCROLL_TICKS 60

/* Runs on frame ticks: gtk_viewport_scroll_to() is a no-op until the tab is
 * allocated (a tab can have bounds but no size yet), and the scroll is
 * clamped until the adjustment covers the tab, which a newly added tab or a
 * freshly mapped window does not do yet. Gives up without scrolling after
 * MAX_SCROLL_TICKS frames rather than scroll to a wrong place. */
static gboolean
scroll_to_selected_tab_cb (GtkWidget      *list,
                           GdkFrameClock  *frame_clock,
                           EphyTabSidebar *self)
{
  GtkWidget *tab = get_selected_tab (self);
  /* GtkScrolledWindow wraps its non-scrollable child in a viewport. */
  GtkWidget *viewport = tab ? gtk_widget_get_ancestor (tab, GTK_TYPE_VIEWPORT) : NULL;
  GtkAdjustment *adjustment;
  graphene_rect_t bounds;
  gboolean ready;

  if (!viewport)
    goto done;

  adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (viewport));
  ready = gtk_widget_compute_bounds (tab, gtk_viewport_get_child (GTK_VIEWPORT (viewport)), &bounds) &&
          bounds.size.height > 0 &&
          gtk_adjustment_get_upper (adjustment) >= bounds.origin.y + bounds.size.height;

  if (!ready && ++self->scroll_ticks < MAX_SCROLL_TICKS)
    return G_SOURCE_CONTINUE;

  if (ready)
    gtk_viewport_scroll_to (GTK_VIEWPORT (viewport), tab, NULL);

done:
  self->scroll_tick_id = 0;
  self->scroll_ticks = 0;

  return G_SOURCE_REMOVE;
}

/* Looks the tab up among the tabs rather than in the pinned or unpinned
 * pages: both filter the same pages, and while one of them reports a change,
 * such as a pinned page closing, the other one is not up to date yet, and
 * reading it reaches past the pages. */
static GtkWidget *
find_tab (GtkWidget  *box,
          AdwTabPage *page)
{
  for (GtkWidget *child = gtk_widget_get_first_child (box); child; child = gtk_widget_get_next_sibling (child)) {
    if (get_page (child) == page)
      return child;
  }

  return NULL;
}

static void
sync_selection_cb (EphyTabSidebar *self)
{
  AdwTabPage *page = adw_tab_view_get_selected_page (self->view);
  GtkFlowBoxChild *child = NULL;
  GtkListBoxRow *row = NULL;

  if (page && adw_tab_page_get_pinned (page))
    child = GTK_FLOW_BOX_CHILD (find_tab (GTK_WIDGET (self->pinned_box), page));
  else if (page)
    row = GTK_LIST_BOX_ROW (find_tab (GTK_WIDGET (self->list), page));

  self->syncing_selection = TRUE;

  if (child)
    gtk_flow_box_select_child (self->pinned_box, child);
  else
    gtk_flow_box_unselect_all (self->pinned_box);

  gtk_list_box_select_row (self->list, row);

  self->syncing_selection = FALSE;

  if ((row || child) && !self->scroll_tick_id)
    self->scroll_tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self->list),
                                                         (GtkTickCallback)scroll_to_selected_tab_cb,
                                                         self, NULL);
}

static void
row_selected_cb (GtkListBox     *list,
                 GtkListBoxRow  *row,
                 EphyTabSidebar *self)
{
  if (self->syncing_selection || !row || !self->view)
    return;

  adw_tab_view_set_selected_page (self->view, get_page (GTK_WIDGET (row)));
}

static void
pinned_selected_cb (EphyTabSidebar *self)
{
  g_autoptr (GList) children = NULL;

  if (self->syncing_selection || !self->view)
    return;

  children = gtk_flow_box_get_selected_children (self->pinned_box);
  if (children)
    adw_tab_view_set_selected_page (self->view, get_page (children->data));
}

/* Pinning */

/* The pages pinned or not, kept up to date as they are pinned and unpinned. */
static GListModel *
filter_pages (GListModel *pages,
              gboolean    pinned)
{
  GtkFilter *filter = GTK_FILTER (gtk_bool_filter_new (gtk_property_expression_new (ADW_TYPE_TAB_PAGE, NULL, "pinned")));
  GtkFilterListModel *model;

  gtk_bool_filter_set_invert (GTK_BOOL_FILTER (filter), !pinned);
  model = gtk_filter_list_model_new (pages, filter);
  gtk_filter_list_model_set_watch_items (model, TRUE);

  return G_LIST_MODEL (model);
}

/* Closed or moved to another view: there is nothing left to reorder. */
static void
page_detached_cb (EphyTabSidebar *self,
                  AdwTabPage     *page)
{
  if (page == self->drag_page)
    end_drag (self);
}

/* An empty scrolled window still takes the height of its scrollbar. */
static void
update_lists_visibility_cb (EphyTabSidebar *self)
{
  gboolean pinned = self->pinned_pages && g_list_model_get_n_items (self->pinned_pages) > 0;
  gboolean unpinned = self->unpinned_pages && g_list_model_get_n_items (self->unpinned_pages) > 0;

  /* Pinned tabs share the line, down to their minimum width. */
  gtk_flow_box_set_max_children_per_line (self->pinned_box,
                                          pinned ? g_list_model_get_n_items (self->pinned_pages) : 1);
  gtk_widget_set_visible (self->pinned_scrolled_window, pinned);
  gtk_widget_set_visible (self->pinned_separator, pinned && unpinned);
  gtk_widget_set_visible (self->scrolled_window, unpinned);
}

AdwTabView *
ephy_tab_sidebar_get_view (EphyTabSidebar *self)
{
  g_return_val_if_fail (EPHY_IS_TAB_SIDEBAR (self), NULL);

  return self->view;
}

void
ephy_tab_sidebar_set_view (EphyTabSidebar *self,
                           AdwTabView     *view)
{
  g_return_if_fail (EPHY_IS_TAB_SIDEBAR (self));
  g_return_if_fail (view == NULL || ADW_IS_TAB_VIEW (view));

  if (self->view == view)
    return;

  if (self->view) {
    end_drag (self);
    g_signal_handlers_disconnect_by_data (self->view, self);
    g_signal_handlers_disconnect_by_data (self->pinned_pages, self);
    g_signal_handlers_disconnect_by_data (self->unpinned_pages, self);
    gtk_flow_box_bind_model (self->pinned_box, NULL, NULL, NULL, NULL);
    gtk_list_box_bind_model (self->list, NULL, NULL, NULL, NULL);
    g_clear_object (&self->pinned_pages);
    g_clear_object (&self->unpinned_pages);
  }

  g_set_object (&self->view, view);

  if (self->view) {
    GListModel *pages = G_LIST_MODEL (adw_tab_view_get_pages (self->view));

    self->pinned_pages = filter_pages (g_object_ref (pages), TRUE);
    self->unpinned_pages = filter_pages (pages, FALSE);

    gtk_flow_box_bind_model (self->pinned_box, self->pinned_pages, create_pinned_tab_cb, self, NULL);
    gtk_list_box_bind_model (self->list, self->unpinned_pages, create_row_cb, self, NULL);

    g_signal_connect_object (self->view, "page-detached", G_CALLBACK (page_detached_cb), self, G_CONNECT_SWAPPED);

    /* Connected after binding the models, so tabs already match the models
     * when the selection is synced. */
    g_signal_connect_object (self->view, "notify::selected-page",
                             G_CALLBACK (sync_selection_cb), self,
                             G_CONNECT_SWAPPED | G_CONNECT_AFTER);
    g_signal_connect_object (self->pinned_pages, "items-changed",
                             G_CALLBACK (sync_selection_cb), self,
                             G_CONNECT_SWAPPED | G_CONNECT_AFTER);
    g_signal_connect_object (self->unpinned_pages, "items-changed",
                             G_CALLBACK (sync_selection_cb), self,
                             G_CONNECT_SWAPPED | G_CONNECT_AFTER);
    g_signal_connect_object (self->pinned_pages, "items-changed",
                             G_CALLBACK (update_lists_visibility_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->unpinned_pages, "items-changed",
                             G_CALLBACK (update_lists_visibility_cb), self,
                             G_CONNECT_SWAPPED);
    sync_selection_cb (self);
  }

  update_lists_visibility_cb (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_VIEW]);
}

static void
new_tab_clicked_cb (EphyTabSidebar *self)
{
  AdwTabPage *page = NULL;

  g_signal_emit (self, signals[CREATE_TAB], 0, &page);

  if (!page) {
    g_critical ("EphyTabSidebar::create-tab handler must not return NULL");
    return;
  }

  if (self->view)
    adw_tab_view_set_selected_page (self->view, page);
}

static void
ephy_tab_sidebar_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyTabSidebar *self = EPHY_TAB_SIDEBAR (object);

  switch ((EphyTabSidebarProps)prop_id) {
    case PROP_VIEW:
      g_value_set_object (value, ephy_tab_sidebar_get_view (self));
      break;
  }
}

static void
ephy_tab_sidebar_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyTabSidebar *self = EPHY_TAB_SIDEBAR (object);

  switch ((EphyTabSidebarProps)prop_id) {
    case PROP_VIEW:
      ephy_tab_sidebar_set_view (self, g_value_get_object (value));
      break;
  }
}

static void
ephy_tab_sidebar_dispose (GObject *object)
{
  EphyTabSidebar *self = EPHY_TAB_SIDEBAR (object);

  ephy_tab_sidebar_set_view (self, NULL);
  g_clear_handle_id (&self->reset_setup_menu_id, g_source_remove);
  g_clear_object (&self->drag_page);
  if (self->scroll_tick_id) {
    gtk_widget_remove_tick_callback (GTK_WIDGET (self->list), self->scroll_tick_id);
    self->scroll_tick_id = 0;
  }
  g_clear_pointer (&self->context_menu, gtk_widget_unparent);

  G_OBJECT_CLASS (ephy_tab_sidebar_parent_class)->dispose (object);
}

static void
ephy_tab_sidebar_class_init (EphyTabSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_tab_sidebar_get_property;
  object_class->set_property = ephy_tab_sidebar_set_property;
  object_class->dispose = ephy_tab_sidebar_dispose;

  obj_properties[PROP_VIEW] =
    g_param_spec_object ("view", NULL, NULL,
                         ADW_TYPE_TAB_VIEW,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (obj_properties), obj_properties);

  /* Same contract as AdwTabOverview::create-tab: return the new page. */
  signals[CREATE_TAB] =
    g_signal_new ("create-tab",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_first_wins, NULL, NULL,
                  ADW_TYPE_TAB_PAGE,
                  0);

  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, popup_menu_action_cb);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_F10, GDK_SHIFT_MASK, "menu.popup", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Menu, GDK_NO_MODIFIER_MASK, "menu.popup", NULL);

  gtk_widget_class_set_css_name (widget_class, "tabsidebar");
}

static void
ephy_tab_sidebar_init (EphyTabSidebar *self)
{
  GtkWidget *new_tab_button;
  GtkWidget *box;

  self->pinned_box = g_object_new (GTK_TYPE_FLOW_BOX,
                                   "accessible-role", GTK_ACCESSIBLE_ROLE_TAB_LIST,
                                   "selection-mode", GTK_SELECTION_SINGLE,
                                   "homogeneous", TRUE,
                                   "min-children-per-line", 1,
                                   NULL);
  /* Neutral selection and hover, like the other tabs and AdwTabBar. The style
   * also sets the spacing, aligned with the rows of the other tabs. */
  gtk_widget_add_css_class (GTK_WIDGET (self->pinned_box), "navigation-sidebar");
  g_signal_connect_object (self->pinned_box, "selected-children-changed",
                           G_CALLBACK (pinned_selected_cb), self, G_CONNECT_SWAPPED);

  self->list = g_object_new (GTK_TYPE_LIST_BOX,
                             "accessible-role", GTK_ACCESSIBLE_ROLE_TAB_LIST,
                             "selection-mode", GTK_SELECTION_SINGLE,
                             NULL);
  gtk_widget_add_css_class (GTK_WIDGET (self->list), "navigation-sidebar");
  g_signal_connect_object (self->list, "row-selected", G_CALLBACK (row_selected_cb), self, G_CONNECT_DEFAULT);

  /* As in AdwTabBar, pinned tabs stay in place while the other tabs scroll,
   * and scroll on their own when too many. When space is short, the box
   * shares it between both lists. */
  self->pinned_scrolled_window = ephy_tab_sidebar_scroller_new (GTK_WIDGET (self->pinned_box));
  gtk_widget_set_visible (self->pinned_scrolled_window, FALSE);

  self->pinned_separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_margin_top (self->pinned_separator, 6);
  gtk_widget_set_visible (self->pinned_separator, FALSE);

  self->scrolled_window = ephy_tab_sidebar_scroller_new (GTK_WIDGET (self->list));
  gtk_widget_set_visible (self->scrolled_window, FALSE);

  /* Icon only: outside of header bars, buttons have an icon or a label. */
  new_tab_button = gtk_button_new_from_icon_name ("tab-new-symbolic");
  gtk_widget_set_tooltip_text (new_tab_button, _("New Tab"));
  gtk_widget_add_css_class (new_tab_button, "flat");
  gtk_widget_set_margin_start (new_tab_button, 6);
  gtk_widget_set_margin_end (new_tab_button, 6);
  gtk_widget_set_margin_bottom (new_tab_button, 6);
  g_signal_connect_object (new_tab_button, "clicked", G_CALLBACK (new_tab_clicked_cb), self, G_CONNECT_SWAPPED);

  /* The button follows the last tab, then sticks to the bottom once the list
   * overflows: the box shrinks the scrolled window first, and a box gives
   * no extra space to children that do not expand. */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (box), self->pinned_scrolled_window);
  gtk_box_append (GTK_BOX (box), self->pinned_separator);
  gtk_box_append (GTK_BOX (box), self->scrolled_window);
  gtk_box_append (GTK_BOX (box), new_tab_button);

  adw_bin_set_child (ADW_BIN (self), box);
}
