/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <gio/gio.h>

int
main (int argc, char **argv)
{
  GProxyResolver *resolver;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) routes = NULL;
  const char *scheme;

  if (argc != 2) {
    g_printerr ("Usage: epiphany-proxy-check HTTP_OR_HTTPS_URL\n");
    return 2;
  }

  uri = g_uri_parse (argv[1], G_URI_FLAGS_ENCODED, NULL);
  scheme = uri ? g_uri_get_scheme (uri) : NULL;
  if (!uri || !scheme ||
      (g_ascii_strcasecmp (scheme, "http") != 0 &&
       g_ascii_strcasecmp (scheme, "https") != 0) ||
      !g_uri_get_host (uri) || !*g_uri_get_host (uri) ||
      g_uri_get_userinfo (uri)) {
    g_printerr ("Supply an absolute HTTP(S) URL without user information.\n");
    return 2;
  }

  resolver = g_proxy_resolver_get_default ();
  g_print ("Resolver: %s\n", G_OBJECT_TYPE_NAME (resolver));
  if (!g_proxy_resolver_is_supported (resolver)) {
    g_printerr ("The selected proxy resolver is not supported.\n");
    return 1;
  }

  /* Only perform proxy lookup. The resolver may retrieve a configured PAC.
   * Do not log resolver error text or the input URL. */
  routes = g_proxy_resolver_lookup (resolver, argv[1], NULL, &error);
  if (!routes || !routes[0]) {
    g_printerr ("Proxy lookup failed or returned no routes.\n");
    return 1;
  }

  for (guint i = 0; routes[i]; i++) {
    g_autoptr (GUri) route = NULL;
    g_autofree char *safe_route = NULL;

    if (g_str_equal (routes[i], "direct://")) {
      g_print ("%u. DIRECT\n", i + 1);
      continue;
    }

    route = g_uri_parse (routes[i], G_URI_FLAGS_ENCODED, NULL);
    if (!route || !g_uri_get_scheme (route) || !g_uri_get_host (route) ||
        !*g_uri_get_host (route)) {
      g_printerr ("%u. Unrecognized route (details suppressed).\n", i + 1);
      return 1;
    }

    safe_route = g_uri_join (G_URI_FLAGS_ENCODED,
                             g_uri_get_scheme (route), NULL,
                             g_uri_get_host (route), g_uri_get_port (route),
                             "", NULL, NULL);
    g_print ("%u. %s\n", i + 1, safe_route);
  }

  g_print ("Routes are resolver results, not connection or PAC-success tests.\n");
  return 0;
}
