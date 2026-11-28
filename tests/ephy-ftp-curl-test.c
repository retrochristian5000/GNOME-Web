/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-ftp-curl.h"
#include "ephy-curl.h"

static void
check_url (const char *uri,
           gboolean    explicit_tls,
           const char *expected,
           const char *proxy)
{
  g_autoptr (GError) error = NULL;
  CURL *handle = ephy_ftp_curl_prepare (uri, explicit_tls, proxy, &error);
  char *effective = NULL;

  g_assert_no_error (error);
  g_assert_nonnull (handle);
  g_assert_cmpint (curl_easy_getinfo (handle, CURLINFO_EFFECTIVE_URL, &effective), ==, CURLE_OK);
  g_assert_cmpstr (effective, ==, expected);
  curl_easy_cleanup (handle);
}

static void
test_prepare (void)
{
  unsigned int caps = ephy_curl_get_capabilities ();

  if (!(caps & EPHY_CURL_FTP)) {
    g_test_skip ("libcurl was built without FTP");
    return;
  }

  check_url ("ftp://anonymous@example.invalid/pub;type=d", FALSE,
             "ftp://example.invalid:21/pub/", "direct://");
  check_url ("ftp://example.invalid/a/../b%3Btype%3Di", FALSE,
             "ftp://example.invalid:21/a/../b%3Btype%3Di", "direct://");
  check_url ("ftp://example.invalid/file;type=a", FALSE,
             "ftp://example.invalid:21/file", "socks5h://proxy.example.invalid:1080");

  if (caps & EPHY_CURL_FTPS_EXPLICIT)
    check_url ("ftp://example.invalid/file", TRUE,
               "ftp://example.invalid:21/file", "direct://");
  if (caps & EPHY_CURL_FTPS_IMPLICIT)
    check_url ("ftps://example.invalid/file", FALSE,
               "ftps://example.invalid:990/file", "direct://");
}

static void
test_reject (void)
{
  static const struct {
    const char *uri;
    const char *proxy;
  } cases[] = {
    { "https://example.invalid/file", "direct://" },
    { "ftp://user:secret@example.invalid/file", "direct://" },
    { "ftp://user@example.invalid/file", "direct://" },
    { "ftp://example.invalid/file", NULL },
    { "ftp://example.invalid/file", "" },
    { "ftp://example.invalid/file", "file:///tmp/proxy" },
    { "ftp://example.invalid/file", "http://user:pass@proxy.example.invalid:8080" },
    { "ftp://example.invalid/file", "http://proxy.example.invalid/path" },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    CURL *handle = ephy_ftp_curl_prepare (cases[i].uri, FALSE, cases[i].proxy, &error);

    g_assert_null (handle);
    g_assert_nonnull (error);
  }
}

int
main (int argc, char **argv)
{
  int result;

  g_test_init (&argc, &argv, NULL);
  g_assert_cmpint (curl_global_init (CURL_GLOBAL_DEFAULT), ==, CURLE_OK);
  g_test_add_func ("/ftp/curl/prepare", test_prepare);
  g_test_add_func ("/ftp/curl/reject", test_reject);
  result = g_test_run ();
  curl_global_cleanup ();
  return result;
}
