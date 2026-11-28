/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <gio/gio.h>
#include <libsoup/soup.h>

static void
check_route (GProxyResolver *resolver,
             const char     *url,
             const char     *expected)
{
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) routes = g_proxy_resolver_lookup (resolver, url, NULL, &error);

  g_assert_no_error (error);
  g_assert_nonnull (routes);
  g_assert_cmpstr (routes[0], ==, expected);
  g_assert_null (routes[1]);
}

static void
test_manual (void)
{
  char *ignore[] = { "direct.example.invalid", NULL };
  g_autoptr (GProxyResolver) resolver =
    g_simple_proxy_resolver_new ("http://proxy.example.invalid:8080", ignore);

  g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver),
                                         "https", "http://secure-proxy.example.invalid:8443");
  check_route (resolver, "http://proxied.example.invalid/", "http://proxy.example.invalid:8080");
  check_route (resolver, "https://proxied.example.invalid/", "http://secure-proxy.example.invalid:8443");
  check_route (resolver, "https://direct.example.invalid/", "direct://");
}

static void
test_soup_resolver (void)
{
  g_autoptr (SoupSession) session = soup_session_new ();

  g_assert_true (soup_session_get_proxy_resolver (session) == g_proxy_resolver_get_default ());
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/proxy/manual-and-bypass", test_manual);
  g_test_add_func ("/proxy/soup-system-default", test_soup_resolver);
  return g_test_run ();
}
