/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-ftp.h"

#include <string.h>

#define FTP_DEFAULT_PORT 21
#define FTPS_DEFAULT_PORT 990
#define FTP_MAX_URI_LENGTH 8192

static gboolean
valid_host (const char *host)
{
  const char *label;

  if (!host || !*host || strlen (host) > 253)
    return FALSE;

  if (g_hostname_is_ip_address (host))
    return TRUE;

  label = host;
  for (const char *p = host; ; p++) {
    if (*p == '.' || *p == '\0') {
      gsize length = p - label;

      if (length == 0 || length > 63 || *label == '-' || p[-1] == '-')
        return FALSE;
      if (*p == '\0' || p[1] == '\0')
        return TRUE;
      label = p + 1;
    } else if (!g_ascii_isalnum (*p) && *p != '-') {
      return FALSE;
    }
  }
}

static gboolean
valid_field (const char *value)
{
  for (const char *p = value; *p; p++) {
    if (g_ascii_iscntrl (*p))
      return FALSE;
  }

  return g_utf8_validate (value, -1, NULL);
}

void
ephy_ftp_address_free (EphyFtpAddress *address)
{
  if (!address)
    return;

  g_free (address->host);
  g_free (address->username);
  g_strfreev (address->components);
  g_free (address);
}

/* RFC 1738 section 3.2, with browser-specific restrictions:
 * no embedded passwords, queries or fragments, and no automatic
 * network actions. Split before decoding so escaped slashes and
 * semicolons remain part of their original remote path component.
 * Do not resolve dot segments or infer a local filesystem path.
 * ftps:// denotes implicit TLS; explicit TLS is an independent policy. */
EphyFtpAddress *
ephy_ftp_address_parse (const char *uri,
                        gboolean    require_explicit_tls,
                        GError    **error)
{
  g_autofree char *scheme = NULL;
  g_autofree char *userinfo = NULL;
  g_autofree char *host = NULL;
  g_autofree char *encoded_path = NULL;
  g_autofree char *query = NULL;
  g_autofree char *fragment = NULL;
  g_autofree char *username = NULL;
  g_autofree char *raw_path = NULL;
  g_auto (GStrv) components = NULL;
  const char *authority;
  const char *authority_end;
  const char *host_start;
  const char *at;
  char *suffix;
  int port;
  EphyFtpSecurity security;
  EphyFtpTransfer transfer = EPHY_FTP_TRANSFER_AUTO;
  EphyFtpAddress *address;
  gboolean directory_hint;

  if (!uri || strlen (uri) > FTP_MAX_URI_LENGTH)
    goto invalid;

  if (g_ascii_strncasecmp (uri, "ftp://", 6) == 0) {
    authority = uri + 6;
    security = require_explicit_tls ? EPHY_FTP_SECURITY_EXPLICIT_TLS : EPHY_FTP_SECURITY_PLAINTEXT;
  } else if (g_ascii_strncasecmp (uri, "ftps://", 7) == 0) {
    authority = uri + 7;
    security = EPHY_FTP_SECURITY_IMPLICIT_TLS;
  } else {
    goto invalid;
  }

  for (const char *p = uri; *p; p++) {
    if (g_ascii_iscntrl (*p) || g_ascii_isspace (*p) || *p == '\\')
      goto invalid;
  }

  authority_end = authority + strcspn (authority, "/?#");
  if (authority_end == authority || authority_end[-1] == ':')
    goto invalid;

  at = memchr (authority, '@', authority_end - authority);
  host_start = at ? at + 1 : authority;
  if (host_start == authority_end ||
      memchr (host_start, '@', authority_end - host_start) ||
      memchr (host_start, '%', authority_end - host_start))
    goto invalid;

  if (!g_uri_split (uri, G_URI_FLAGS_ENCODED,
                    &scheme, &userinfo, &host, &port,
                    &encoded_path, &query, &fragment, error))
    return NULL;

  if (!valid_host (host) || port == 0 || port > 65535 || query || fragment)
    goto invalid;

  if (userinfo) {
    username = g_uri_unescape_string (userinfo, NULL);
    if (!username || !*username || !valid_field (username) ||
        strchr (username, ':') || strchr (username, '@'))
      goto invalid;
  }

  /* Use the original path to avoid URI path normalization. */
  raw_path = g_strdup (*authority_end == '/' ? authority_end + 1 : "");
  suffix = strchr (raw_path, ';');
  if (suffix) {
    if (g_ascii_strcasecmp (suffix, ";type=a") == 0)
      transfer = EPHY_FTP_TRANSFER_ASCII;
    else if (g_ascii_strcasecmp (suffix, ";type=i") == 0)
      transfer = EPHY_FTP_TRANSFER_BINARY;
    else if (g_ascii_strcasecmp (suffix, ";type=d") == 0)
      transfer = EPHY_FTP_TRANSFER_DIRECTORY;
    else
      goto invalid;

    *suffix = '\0';
  }

  directory_hint = transfer == EPHY_FTP_TRANSFER_DIRECTORY ||
                   (transfer == EPHY_FTP_TRANSFER_AUTO &&
                    (!*raw_path || g_str_has_suffix (raw_path, "/")));
  if ((!*raw_path || g_str_has_suffix (raw_path, "/")) &&
      (transfer == EPHY_FTP_TRANSFER_ASCII || transfer == EPHY_FTP_TRANSFER_BINARY))
    goto invalid;

  components = g_strsplit (raw_path, "/", -1);
  for (gsize i = 0; components[i]; i++) {
    char *decoded = g_uri_unescape_string (components[i], NULL);

    if (!decoded || !valid_field (decoded)) {
      g_free (decoded);
      goto invalid;
    }
    g_free (components[i]);
    components[i] = decoded;
  }

  address = g_new0 (EphyFtpAddress, 1);
  address->security = security;
  address->transfer = transfer;
  address->host = g_steal_pointer (&host);
  address->port = port < 0 ?
                  (security == EPHY_FTP_SECURITY_IMPLICIT_TLS ? FTPS_DEFAULT_PORT : FTP_DEFAULT_PORT) : port;
  address->username = g_steal_pointer (&username);
  address->components = g_steal_pointer (&components);
  address->directory_hint = directory_hint;
  return address;

invalid:
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "Invalid or unsupported FTP/FTPS URI");
  return NULL;
}
