/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <gio/gio.h>

typedef enum {
  EPHY_SSH_SERVICE_TERMINAL,
  EPHY_SSH_SERVICE_FILE_TRANSFER,
} EphySshService;

typedef struct {
  EphySshService service;
  char *host;
  guint16 port;
  char *username;
  /* Only set for SFTP. Opaque remote path, never a local path. */
  char *path;
} EphySshAddress;

/* Parse a conservative subset of SSH/SFTP URIs. No network or process I/O. */
EphySshAddress *ephy_ssh_address_parse (const char *uri,
                                        GError    **error);
void ephy_ssh_address_free (EphySshAddress *address);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EphySshAddress, ephy_ssh_address_free)
