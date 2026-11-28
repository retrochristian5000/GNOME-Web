/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "ephy-ruffle-web.h"

#if HAVE_RUFFLE_WEB
#include <libsoup/soup.h>

static void
asset_request_cb (WebKitURISchemeRequest *request,
                  gpointer                user_data)
{
  GResource *resource = user_data;
  const char *path = webkit_uri_scheme_request_get_path (request);
  const char *mime;
  g_autoptr (GUri) uri = g_uri_parse (webkit_uri_scheme_request_get_uri (request), G_URI_FLAGS_NONE, NULL);
  g_autofree char *resource_path = NULL;
  g_autoptr (GBytes) data = NULL;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (WebKitURISchemeResponse) response = NULL;
  g_autoptr (GError) error = NULL;
  SoupMessageHeaders *headers;

  /* No filesystem path resolution, no cookies, no private resources.
   * Only immutable public assets from the separately loaded bundle. */
  if (!uri || g_strcmp0 (g_uri_get_host (uri), "assets") != 0 ||
      g_uri_get_userinfo (uri) || g_uri_get_port (uri) != -1 ||
      g_strcmp0 (webkit_uri_scheme_request_get_http_method (request), "GET") != 0 ||
      !path || path[0] != '/' ||
      !g_regex_match_simple ("^/[A-Za-z0-9_.-]+\\.(js|wasm)$", path, 0, 0)) {
    error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "Invalid Ruffle asset request");
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  mime = g_str_has_suffix (path, ".wasm") ? "application/wasm" : "text/javascript";
  resource_path = g_strconcat ("/ruffle-web", path, NULL);
  data = g_resource_lookup_data (resource, resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  if (!data) {
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  stream = g_memory_input_stream_new_from_bytes (data);
  response = webkit_uri_scheme_response_new (stream, g_bytes_get_size (data));
  webkit_uri_scheme_response_set_content_type (response, mime);
  webkit_uri_scheme_response_set_status (response, 200, NULL);

  headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
  soup_message_headers_append (headers, "Access-Control-Allow-Origin", "*");
  soup_message_headers_append (headers, "X-Content-Type-Options", "nosniff");
  webkit_uri_scheme_response_set_http_headers (response, headers);
  webkit_uri_scheme_request_finish_with_response (request, response);
}
#endif

void
ephy_ruffle_web_register (WebKitWebContext *context)
{
#if HAVE_RUFFLE_WEB
  g_autoptr (GError) error = NULL;
  GResource *resource;
  WebKitSecurityManager *security_manager;

#if DEVELOPER_MODE
  resource = g_resource_load (BUILD_ROOT "/third-party/ruffle-web.gresource", &error);
#else
  resource = g_resource_load (RUFFLE_WEB_RESOURCE, &error);
#endif
  if (!resource) {
    g_debug ("Ruffle Web resource unavailable: %s", error->message);
    return;
  }

  security_manager = webkit_web_context_get_security_manager (context);
  webkit_web_context_register_uri_scheme (context, "ephy-ruffle",
                                          asset_request_cb, resource,
                                          (GDestroyNotify)g_resource_unref);
  webkit_security_manager_register_uri_scheme_as_secure (security_manager, "ephy-ruffle");
  webkit_security_manager_register_uri_scheme_as_cors_enabled (security_manager, "ephy-ruffle");
  g_object_set_data (G_OBJECT (context), "ephy-ruffle-web-available", GINT_TO_POINTER (1));
#endif
}

gboolean
ephy_ruffle_web_available (WebKitWebContext *context)
{
  return g_object_get_data (G_OBJECT (context), "ephy-ruffle-web-available") != NULL;
}
