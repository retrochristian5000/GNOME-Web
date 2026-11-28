/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <gio/gio.h>

typedef struct {
  char *host;
  guint16 port;
  /* An opaque remote repository path, not a local filesystem path. */
  char *path;
} EphyGitAddress;

/* Native git:// transport only. No network, Git process or file I/O. */
EphyGitAddress *ephy_git_address_parse (const char *uri,
                                       GError    **error);
void ephy_git_address_free (EphyGitAddress *address);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EphyGitAddress, ephy_git_address_free)
