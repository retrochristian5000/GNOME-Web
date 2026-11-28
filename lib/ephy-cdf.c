/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ephy-cdf.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define CDF_MAX_BYTES (1024 * 1024)
#define CDF_MAX_DEPTH 32
#define CDF_MAX_ENTRIES 4096
#define CDF_MAX_URL_BYTES 8192
#define CDF_MAX_HTML_BYTES (4 * 1024 * 1024)

static gboolean
node_is (const xmlNode *node,
         const char    *name)
{
  return node && node->type == XML_ELEMENT_NODE &&
         !node->ns && g_ascii_strcasecmp ((const char *)node->name, name) == 0;
}

static char *
attribute (const xmlNode *node,
           const char    *name)
{
  for (xmlAttr *attr = node->properties; attr; attr = attr->next) {
    if (!attr->ns && g_ascii_strcasecmp ((const char *)attr->name, name) == 0) {
      xmlChar *value = xmlNodeListGetString (node->doc, attr->children, 1);
      char *copy = g_strdup ((const char *)value);

      xmlFree (value);
      return copy;
    }
  }

  return NULL;
}

static char *
metadata (const xmlNode *node,
          const char    *name)
{
  for (xmlNode *child = node->children; child; child = child->next) {
    if (node_is (child, name)) {
      char *value = attribute (child, "VALUE");
      GString *text;

      if (value)
        return value;

      text = g_string_new (NULL);
      for (xmlNode *part = child->children; part; part = part->next) {
        if ((part->type == XML_TEXT_NODE || part->type == XML_CDATA_SECTION_NODE) && part->content)
          g_string_append (text, (const char *)part->content);
      }

      return g_string_free (text, FALSE);
    }
  }

  return NULL;
}

static char *
safe_link (const char *href,
           const char *base)
{
  g_autofree char *resolved = NULL;
  g_autoptr (GUri) uri = NULL;
  const char *scheme;

  if (!href || !*href || strlen (href) > CDF_MAX_URL_BYTES)
    return NULL;

  resolved = base ? g_uri_resolve_relative (base, href, G_URI_FLAGS_ENCODED, NULL) : g_strdup (href);
  if (!resolved || strlen (resolved) > CDF_MAX_URL_BYTES)
    return NULL;

  uri = g_uri_parse (resolved, G_URI_FLAGS_ENCODED, NULL);
  if (!uri)
    return NULL;

  scheme = g_uri_get_scheme (uri);
  if (!scheme || (g_ascii_strcasecmp (scheme, "http") != 0 &&
                  g_ascii_strcasecmp (scheme, "https") != 0) ||
      !g_uri_get_host (uri) || !*g_uri_get_host (uri) || g_uri_get_userinfo (uri))
    return NULL;

  return g_uri_to_string (uri);
}

static void
append_escaped (GString    *html,
                const char *text)
{
  g_autofree char *escaped = g_markup_escape_text (text, -1);

  g_string_append (html, escaped);
}

static gboolean
render_entry (GString       *html,
              const xmlNode *node,
              const char    *base,
              guint          depth,
              guint         *entries)
{
  gboolean channel = node_is (node, "CHANNEL");
  g_autofree char *title = metadata (node, "TITLE");
  g_autofree char *abstract = metadata (node, "ABSTRACT");
  g_autofree char *href = attribute (node, "HREF");
  g_autofree char *link = safe_link (href, base);

  if (depth > CDF_MAX_DEPTH || ++*entries > CDF_MAX_ENTRIES)
    return FALSE;

  g_string_append (html, channel ? "<li><strong>Channel: </strong>" : "<li>");
  if (link) {
    g_string_append (html, "<a href=\"");
    append_escaped (html, link);
    g_string_append (html, "\" rel=\"noreferrer noopener\">");
  }

  append_escaped (html, title && *title ? title : href && *href ? href : channel ? "Untitled channel" : "Untitled item");
  if (link)
    g_string_append (html, "</a>");

  if (abstract && *abstract) {
    g_string_append (html, "<p>");
    append_escaped (html, abstract);
    g_string_append (html, "</p>");
  }

  if (html->len > CDF_MAX_HTML_BYTES)
    return FALSE;

  if (channel) {
    g_string_append (html, "<ul>");
    for (xmlNode *child = node->children; child; child = child->next) {
      if ((node_is (child, "CHANNEL") || node_is (child, "ITEM")) &&
          !render_entry (html, child, base, depth + 1, entries))
        return FALSE;
    }
    g_string_append (html, "</ul>");
  }

  g_string_append (html, "</li>");
  return html->len <= CDF_MAX_HTML_BYTES;
}

char *
ephy_cdf_to_html (const char *data,
                  gsize       length,
                  GError    **error)
{
  xmlParserCtxt *context;
  xmlDoc *doc;
  xmlNode *root;
  g_autofree char *base = NULL;
  GString *html;
  guint entries = 0;
  gboolean rendered;
  int options = XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING;

#if LIBXML_VERSION >= 21300
  options |= XML_PARSE_NO_XXE;
#endif

  if (!data || !length || length > CDF_MAX_BYTES) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                         "Channel definition is empty or exceeds the 1 MiB limit");
    return NULL;
  }

  /* Never enable external DTD loading, entity substitution or recovery.
   * Reject declarations before reading metadata. */
  context = xmlNewParserCtxt ();
  if (!context) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Could not create XML parser");
    return NULL;
  }

  doc = xmlCtxtReadMemory (context, data, (int)length, NULL, NULL, options);
  xmlFreeParserCtxt (context);

  if (!doc) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid CDF XML");
    return NULL;
  }

  root = xmlDocGetRootElement (doc);
  if (!node_is (root, "CHANNEL") || doc->extSubset ||
      (doc->intSubset && doc->intSubset->children)) {
    xmlFreeDoc (doc);
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                         "Expected a CHANNEL document without internal DTD declarations");
    return NULL;
  }

  for (xmlNode *child = root->children; child; child = child->next) {
    if (node_is (child, "SELF")) {
      g_autofree char *href = attribute (child, "HREF");

      base = safe_link (href, NULL);
      break;
    }
  }

  html = g_string_new ("<!doctype html><html><head><meta charset=\"utf-8\">"
                       "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; base-uri 'none'; form-action 'none'\">"
                       "<meta name=\"referrer\" content=\"no-referrer\">"
                       "<title>Channel Definition</title></head><body>"
                       "<h1>Active Channel</h1><p>Read-only channel index. No automatic downloads.</p><ul>");
  rendered = render_entry (html, root, base, 0, &entries);
  xmlFreeDoc (doc);

  if (!rendered) {
    g_string_free (html, TRUE);
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Channel nesting, entry or output limit exceeded");
    return NULL;
  }

  g_string_append (html, "</ul></body></html>");
  return g_string_free (html, FALSE);
}

gboolean
ephy_cdf_is_file (GFile *file)
{
  g_autofree char *name = g_file_get_basename (file);
  gsize length = name ? strlen (name) : 0;

  return length >= 4 && g_ascii_strcasecmp (name + length - 4, ".cdf") == 0;
}

char *
ephy_cdf_load_file (GFile   *file,
                    GError **error)
{
  g_autofree char *path = g_file_get_path (file);
  g_autofree char *data = NULL;
  struct stat st;
  gsize length = 0;
  int fd;

  if (!path) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Open a locally saved CDF file");
    return NULL;
  }

  fd = g_open (path, O_RDONLY | O_NONBLOCK | O_CLOEXEC, 0);
  if (fd < 0) {
    g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (errno), "Could not open CDF file");
    return NULL;
  }

  if (fstat (fd, &st) != 0 || !S_ISREG (st.st_mode) || st.st_size > CDF_MAX_BYTES) {
    close (fd);
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "CDF must be a regular file no larger than 1 MiB");
    return NULL;
  }

  data = g_malloc (CDF_MAX_BYTES + 1);
  while (length < CDF_MAX_BYTES + 1) {
    ssize_t received = read (fd, data + length, CDF_MAX_BYTES + 1 - length);

    if (received < 0 && errno == EINTR)
      continue;
    if (received < 0) {
      int saved_errno = errno;

      close (fd);
      g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (saved_errno), "Could not read CDF file");
      return NULL;
    }
    if (received == 0)
      break;

    length += received;
  }
  close (fd);

  return ephy_cdf_to_html (data, length, error);
}
