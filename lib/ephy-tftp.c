/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-tftp.h"

#include <string.h>

#define TFTP_DEFAULT_PORT 69
#define TFTP_MAX_URI_LENGTH 8192

static gboolean
valid_hostname (const char *host)
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

/* RFC 3617 section 2 uses RFC 2396 unreserved and escaped characters.
 * Literal path delimiters are not part of the filename grammar. */
static gboolean
valid_encoded_filename (const char *filename)
{
  for (const char *p = filename; *p; p++) {
    if (*p == '%') {
      if (!g_ascii_isxdigit (p[1]) || !g_ascii_isxdigit (p[2]))
        return FALSE;
      p += 2;
    } else if (!g_ascii_isalnum (*p) && !strchr ("-_.!~*'()", *p)) {
      return FALSE;
    }
  }

  return *filename != '\0';
}

void
ephy_tftp_address_free (EphyTftpAddress *address)
{
  if (!address)
    return;

  g_free (address->host);
  g_free (address->filename);
  g_free (address);
}

/* RFC 3617: tftp://host/file[;mode=netascii|octet].
 *
 * Keep the filename opaque: never normalize dot segments, map it to a
 * local path or decode it twice. This parser deliberately does not
 * register a browser URI handler or initiate a transfer. A future
 * download handler must separately enforce navigation and proxy policy.
 *
 * Browser restrictions: reject empty filenames, control bytes, userinfo,
 * queries, fragments, explicit ports and nonstandard transfer modes. */
EphyTftpAddress *
ephy_tftp_address_parse (const char *uri,
                         GError    **error)
{
  g_autofree char *scheme = NULL;
  g_autofree char *userinfo = NULL;
  g_autofree char *host = NULL;
  g_autofree char *path = NULL;
  g_autofree char *query = NULL;
  g_autofree char *fragment = NULL;
  g_autofree char *encoded_filename = NULL;
  g_autofree char *filename = NULL;
  const char *authority;
  const char *file;
  const char *mode_suffix;
  const char *authority_end;
  EphyTftpMode mode = EPHY_TFTP_MODE_OCTET;
  EphyTftpAddress *address;
  int port;

  if (!uri || strlen (uri) > TFTP_MAX_URI_LENGTH ||
      g_ascii_strncasecmp (uri, "tftp://", 7) != 0)
    goto invalid;

  for (const char *p = uri; *p; p++) {
    if (g_ascii_iscntrl (*p) || g_ascii_isspace (*p))
      goto invalid;
  }

  authority = uri + 7;
  authority_end = strchr (authority, '/');
  if (!authority_end || authority_end == authority)
    goto invalid;

  /* Reject even empty explicit port fields and escaped authorities.
   * IPv6 literals must be bracketed. */
  if (*authority == '[') {
    const char *close = memchr (authority, ']', authority_end - authority);

    if (!close || close + 1 != authority_end)
      goto invalid;
  } else if (memchr (authority, ':', authority_end - authority)) {
    goto invalid;
  }

  if (memchr (authority, '%', authority_end - authority))
    goto invalid;

  if (!g_uri_split (uri, G_URI_FLAGS_ENCODED_PATH, &scheme, &userinfo,
                    &host, &port, &path, &query, &fragment, error))
    return NULL;

  if (g_ascii_strcasecmp (scheme, "tftp") != 0 ||
      userinfo || query || fragment || port != -1 ||
      !valid_hostname (host))
    goto invalid;

  /* Parse the original escaped filename, not a normalized URI path. */
  file = authority_end + 1;
  mode_suffix = strchr (file, ';');
  if (mode_suffix) {
    if (g_ascii_strcasecmp (mode_suffix, ";mode=netascii") == 0)
      mode = EPHY_TFTP_MODE_NETASCII;
    else if (g_ascii_strcasecmp (mode_suffix, ";mode=octet") != 0)
      goto invalid;

    encoded_filename = g_strndup (file, mode_suffix - file);
  } else {
    encoded_filename = g_strdup (file);
  }

  if (!valid_encoded_filename (encoded_filename))
    goto invalid;

  filename = g_uri_unescape_string (encoded_filename, NULL);
  if (!filename || !*filename)
    goto invalid;

  for (const char *p = filename; *p; p++) {
    if (g_ascii_iscntrl (*p))
      goto invalid;
  }

  address = g_new0 (EphyTftpAddress, 1);
  address->host = g_steal_pointer (&host);
  address->port = TFTP_DEFAULT_PORT;
  address->filename = g_steal_pointer (&filename);
  address->mode = mode;
  return address;

invalid:
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "Invalid or unsupported TFTP URI");
  return NULL;
}
