/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-git.h"

#include <string.h>

#define GIT_DEFAULT_PORT 9418
#define GIT_MAX_URI_LENGTH 8192

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

void
ephy_git_address_free (EphyGitAddress *address)
{
  if (!address)
    return;

  g_free (address->host);
  g_free (address->path);
  g_free (address);
}

/* Conservative subset of the native Git URL syntax documented by git-fetch.
 * The native transport does not authenticate or encrypt the connection.
 * Preserve the remote path without local path or dot-segment resolution.
 * Do not reinterpret other transports, remote-helper syntax, credentials,
 * commands, queries or fragments as native Git URLs. */
EphyGitAddress *
ephy_git_address_parse (const char *uri,
                        GError    **error)
{
  g_autofree char *scheme = NULL;
  g_autofree char *userinfo = NULL;
  g_autofree char *host = NULL;
  g_autofree char *encoded_path = NULL;
  g_autofree char *query = NULL;
  g_autofree char *fragment = NULL;
  g_autofree char *path = NULL;
  const char *authority;
  const char *authority_end;
  EphyGitAddress *address;
  int port;

  if (!uri || strlen (uri) > GIT_MAX_URI_LENGTH ||
      g_ascii_strncasecmp (uri, "git://", 6) != 0)
    goto invalid;

  for (const char *p = uri; *p; p++) {
    if (g_ascii_iscntrl (*p) || g_ascii_isspace (*p) || *p == '\\')
      goto invalid;
  }

  authority = uri + 6;
  authority_end = authority + strcspn (authority, "/?#");
  if (authority_end == authority || *authority_end != '/' ||
      authority_end[-1] == ':' ||
      memchr (authority, '@', authority_end - authority) ||
      memchr (authority, '%', authority_end - authority))
    goto invalid;

  if (!g_uri_split (uri, G_URI_FLAGS_ENCODED_PATH,
                    &scheme, &userinfo, &host, &port,
                    &encoded_path, &query, &fragment, error))
    return NULL;

  if (!valid_host (host) || userinfo || query || fragment ||
      port == 0 || port > 65535)
    goto invalid;

  /* Use the original escaped path to avoid normalizing remote names. */
  path = g_uri_unescape_string (authority_end, NULL);
  if (!path || path[0] != '/' || !path[1] ||
      !g_utf8_validate (path, -1, NULL))
    goto invalid;

  for (const char *p = path; *p; p++) {
    if (g_ascii_iscntrl (*p) || *p == '\\')
      goto invalid;
  }

  address = g_new0 (EphyGitAddress, 1);
  address->host = g_steal_pointer (&host);
  address->port = port < 0 ? GIT_DEFAULT_PORT : port;
  address->path = g_steal_pointer (&path);
  return address;

invalid:
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "Invalid or unsupported native Git URI");
  return NULL;
}
