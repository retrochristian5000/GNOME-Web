/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <gio/gio.h>

typedef struct {
  char *host;
  guint16 port;
  char type;
  char *selector;
  char *search;
} EphyGopherAddress;

typedef struct {
  char type;
  char *label;
  /* NULL for informational, error, and unsupported item types. */
  char *uri;
} EphyGopherMenuItem;

EphyGopherAddress *ephy_gopher_address_parse (const char *uri,
                                              GError    **error);
void ephy_gopher_address_free (EphyGopherAddress *address);

EphyGopherMenuItem *ephy_gopher_menu_item_parse (const char *line,
                                                GError    **error);
void ephy_gopher_menu_item_free (EphyGopherMenuItem *item);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EphyGopherAddress, ephy_gopher_address_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EphyGopherMenuItem, ephy_gopher_menu_item_free)
