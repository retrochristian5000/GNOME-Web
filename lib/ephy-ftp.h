/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <gio/gio.h>

typedef enum {
  EPHY_FTP_SECURITY_PLAINTEXT,
  EPHY_FTP_SECURITY_EXPLICIT_TLS,
  EPHY_FTP_SECURITY_IMPLICIT_TLS,
} EphyFtpSecurity;

typedef enum {
  EPHY_FTP_TRANSFER_AUTO,
  EPHY_FTP_TRANSFER_ASCII,
  EPHY_FTP_TRANSFER_BINARY,
  EPHY_FTP_TRANSFER_DIRECTORY,
} EphyFtpTransfer;

typedef struct {
  EphyFtpSecurity security;
  EphyFtpTransfer transfer;
  char *host;
  guint16 port;
  char *username;
  /* RFC 1738 components, decoded separately. Never local filenames. */
  char **components;
  gboolean directory_hint;
} EphyFtpAddress;

/* No network, credential lookup or file I/O. Explicit TLS is selected
 * by trusted browser policy, not an untrusted URL parameter. */
EphyFtpAddress *ephy_ftp_address_parse (const char *uri,
                                        gboolean    require_explicit_tls,
                                        GError    **error);
void ephy_ftp_address_free (EphyFtpAddress *address);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EphyFtpAddress, ephy_ftp_address_free)
