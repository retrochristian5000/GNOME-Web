/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <gio/gio.h>

typedef enum {
  EPHY_TFTP_MODE_OCTET,
  EPHY_TFTP_MODE_NETASCII,
} EphyTftpMode;

typedef struct {
  char *host;
  guint16 port;
  char *filename;
  EphyTftpMode mode;
} EphyTftpAddress;

/* URI validation only. Does not perform DNS, networking or file I/O. */
EphyTftpAddress *ephy_tftp_address_parse (const char *uri,
                                          GError    **error);
void ephy_tftp_address_free (EphyTftpAddress *address);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EphyTftpAddress, ephy_tftp_address_free)
