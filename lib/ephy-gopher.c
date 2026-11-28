/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-gopher.h"

#include <string.h>

#define GOPHER_DEFAULT_PORT 70
#define GOPHER_MAX_INPUT_LENGTH 8192

static gboolean
valid_host (const char *host)
{
  if (!host || !*host || strlen (host) > 253)
    return FALSE;

  if (g_hostname_is_ip_address (host))
    return TRUE;

  const char *label = host;

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
valid_field (const char *field)
{
  for (const char *p = field; *p; p++) {
    if (g_ascii_iscntrl (*p))
      return FALSE;
  }

  return TRUE;
}

static gboolean
valid_type (char type)
{
  return g_ascii_isalnum (type) || type == '+';
}

void
ephy_gopher_address_free (EphyGopherAddress *address)
{
  if (!address)
    return;

  g_free (address->host);
  g_free (address->selector);
  g_free (address->search);
  g_free (address);
}

/* RFC 4266 section 2.1/2.2. The type is metadata, not part of the
 * selector. Decode exactly once and keep search terms separate.
 * Gopher+ commands and raw query components are not supported here. */
EphyGopherAddress *
ephy_gopher_address_parse (const char *uri,
                           GError    **error)
{
  g_autofree char *scheme = NULL;
  g_autofree char *userinfo = NULL;
  g_autofree char *host = NULL;
  g_autofree char *query = NULL;
  g_autofree char *encoded_path = NULL;
  g_autofree char *path = NULL;
  char *selector;
  char *search = NULL;
  char type = '1';
  int port;
  EphyGopherAddress *address;

  if (!uri || strlen (uri) > GOPHER_MAX_INPUT_LENGTH || !valid_field (uri))
    goto invalid;

  /* g_uri_parse() resolves dot segments, which would change opaque
   * Gopher selectors. Split without hierarchical path resolution. */
  if (!g_uri_split (uri, G_URI_FLAGS_ENCODED_PATH,
                    &scheme, &userinfo, &host, &port,
                    &encoded_path, &query, NULL, error))
    return NULL;

  if (g_strcmp0 (scheme, "gopher") != 0 ||
      !valid_host (host) || userinfo || query ||
      port == 0 || port > 65535)
    goto invalid;

  if (!encoded_path || (*encoded_path && *encoded_path != '/'))
    goto invalid;

  path = g_uri_unescape_string (*encoded_path ? encoded_path + 1 : encoded_path, NULL);
  if (!path)
    goto invalid;

  selector = path;
  if (*path) {
    type = *path;
    if (!valid_type (type))
      goto invalid;
    selector++;
  }

  search = strchr (selector, '\t');
  if (search) {
    if (type != '7')
      goto invalid;
    *search++ = '\0';
    if (!valid_field (search))
      goto invalid;
  }

  if (!valid_field (selector))
    goto invalid;

  address = g_new0 (EphyGopherAddress, 1);
  address->host = g_steal_pointer (&host);
  address->port = port < 0 ? GOPHER_DEFAULT_PORT : port;
  address->type = type;
  address->selector = g_strdup (selector);
  address->search = g_strdup (search);
  return address;

invalid:
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "Invalid or unsupported Gopher URI");
  return NULL;
}

void
ephy_gopher_menu_item_free (EphyGopherMenuItem *item)
{
  if (!item)
    return;

  g_free (item->label);
  g_free (item->uri);
  g_free (item);
}

/* Parse one RFC 1436 directory record without its CRLF terminator.
 * The caller must handle the terminating dot separately. Labels are plain
 * text, never markup; any future renderer must escape them.
 * Unknown types and HTML/external-application links remain inert. */
EphyGopherMenuItem *
ephy_gopher_menu_item_parse (const char *line,
                             GError    **error)
{
  g_auto (GStrv) fields = NULL;
  g_autofree char *encoded_selector = NULL;
  g_autofree char *path = NULL;
  guint64 port;
  char type;
  EphyGopherMenuItem *item;

  if (!line || strlen (line) > GOPHER_MAX_INPUT_LENGTH ||
      strchr (line, '\r') || strchr (line, '\n'))
    goto invalid;

  fields = g_strsplit (line, "\t", -1);
  if (g_strv_length (fields) != 4 || !*fields[0] ||
      !valid_type (fields[0][0]) || !valid_field (fields[0]))
    goto invalid;

  type = fields[0][0];
  if (!strchr ("0179gIs", type)) {
    item = g_new0 (EphyGopherMenuItem, 1);
    item->type = type;
    item->label = g_utf8_make_valid (fields[0] + 1, -1);
    return item;
  }

  if (!valid_host (fields[2]) || !valid_field (fields[1]) ||
      !*fields[3])
    goto invalid;

  for (const char *p = fields[3]; *p; p++) {
    if (!g_ascii_isdigit (*p))
      goto invalid;
  }

  port = g_ascii_strtoull (fields[3], NULL, 10);
  if (port == 0 || port > 65535)
    goto invalid;

  encoded_selector = g_uri_escape_string (fields[1], G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
  path = g_strdup_printf ("/%c%s", type, encoded_selector);
  item = g_new0 (EphyGopherMenuItem, 1);
  item->type = type;
  item->label = g_utf8_make_valid (fields[0] + 1, -1);
  item->uri = g_uri_join (G_URI_FLAGS_ENCODED_PATH, "gopher", NULL,
                          fields[2], port == GOPHER_DEFAULT_PORT ? -1 : (int)port,
                          path, NULL, NULL);
  return item;

invalid:
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                       "Invalid or unsupported Gopher menu record");
  return NULL;
}
