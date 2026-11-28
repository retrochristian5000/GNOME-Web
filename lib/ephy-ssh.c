/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-ssh.h"

#include <string.h>

#define SSH_DEFAULT_PORT 22
#define SSH_MAX_URI_LENGTH 8192

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
valid_text (const char *value)
{
  for (const char *p = value; *p; p++) {
    if (g_ascii_iscntrl (*p))
      return FALSE;
  }
  return g_utf8_validate (value, -1, NULL);
}

static gboolean
valid_username (const char *username)
{
  if (!username || !*username || *username == '-' ||
      strlen (username) > 255)
    return FALSE;

  for (const char *p = username; *p; p++) {
    if (!g_ascii_isalnum (*p) && !strchr ("_.-", *p))
      return FALSE;
  }
  return TRUE;
}

void
ephy_ssh_address_free (EphySshAddress *address)
{
  if (!address)
    return;

  g_free (address->host);
  g_free (address->username);
  g_free (address->path);
  g_free (address);
}

/* Based on draft-ietf-secsh-scp-sftp-ssh-uri-04, not a final RFC.
 * Deliberately omit passwords, connection parameters, remote commands,
 * queries and fragments. Authentication and host-key decisions belong
 * to a trusted native client, never to a URI or a web document. */
EphySshAddress *
ephy_ssh_address_parse (const char *uri,
                        GError    **error)
{
  g_autofree char *scheme = NULL;
  g_autofree char *userinfo = NULL;
  g_autofree char *host = NULL;
  g_autofree char *encoded_path = NULL;
  g_autofree char *query = NULL;
  g_autofree char *fragment = NULL;
  g_autofree char *username = NULL;
  g_autofree char *path = NULL;
  const char *authority;
  const char *authority_end;
  const char *host_start;
  const char *at;
  EphySshAddress *address;
  EphySshService service;
  int port;

  if (!uri || strlen (uri) > SSH_MAX_URI_LENGTH)
    goto invalid;

  if (g_ascii_strncasecmp (uri, "ssh://", 6) == 0) {
    service = EPHY_SSH_SERVICE_TERMINAL;
    authority = uri + 6;
  } else if (g_ascii_strncasecmp (uri, "sftp://", 7) == 0) {
    service = EPHY_SSH_SERVICE_FILE_TRANSFER;
    authority = uri + 7;
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
      memchr (host_start, '%', authority_end - host_start) ||
      memchr (host_start, '@', authority_end - host_start))
    goto invalid;

  if (!g_uri_split (uri, G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED,
                    &scheme, &userinfo, &host, &port,
                    &encoded_path, &query, &fragment, error))
    return NULL;

  if (!valid_host (host) || port == 0 || port > 65535 ||
      query || fragment)
    goto invalid;

  if (userinfo) {
    username = g_uri_unescape_string (userinfo, NULL);
    if (!valid_username (username))
      goto invalid;
  }

  if (service == EPHY_SSH_SERVICE_TERMINAL) {
    if (encoded_path && *encoded_path && strcmp (encoded_path, "/") != 0)
      goto invalid;
  } else {
    if (encoded_path && *encoded_path) {
      path = g_uri_unescape_string (encoded_path, NULL);
      if (!path || *path != '/' || !valid_text (path))
        goto invalid;
    } else {
      path = g_strdup ("/");
    }
  }

  address = g_new0 (EphySshAddress, 1);
  address->service = service;
  address->host = g_steal_pointer (&host);
  address->port = port < 0 ? SSH_DEFAULT_PORT : port;
  address->username = g_steal_pointer (&username);
  address->path = g_steal_pointer (&path);
  return address;

invalid:
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "Invalid or unsupported SSH/SFTP URI");
  return NULL;
}
