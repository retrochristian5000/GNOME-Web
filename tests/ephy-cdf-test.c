/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ephy-cdf.h"

#include <string.h>

static void
test_channel (void)
{
  const char *cdf = "<?xml version=\"1.0\"?>"
                    "<!DOCTYPE Channel SYSTEM \"https://example.invalid/Channel.dtd\">"
                    "<CHANNEL HREF=\"https://example.org/\">"
                    "<SELF HREF=\"https://example.org/channels/news.cdf\"/>"
                    "<TITLE VALUE=\"Historic &amp; News\"/>"
                    "<Channel><Title>Articles</Title>"
                    "<ITEM HREF=\"article.html\"><TITLE>Article</TITLE>"
                    "<ABSTRACT>&lt;script&gt;bad&lt;/script&gt;</ABSTRACT></ITEM>"
                    "<ITEM HREF=\"javascript:alert(1)\"><TITLE>Unsafe link</TITLE></ITEM>"
                    "</Channel><SCHEDULE/></CHANNEL>";
  g_autoptr (GError) error = NULL;
  g_autofree char *html = ephy_cdf_to_html (cdf, strlen (cdf), &error);

  g_assert_no_error (error);
  g_assert_nonnull (html);
  g_assert_nonnull (strstr (html, "Historic &amp; News"));
  g_assert_nonnull (strstr (html, "https://example.org/channels/article.html"));
  g_assert_nonnull (strstr (html, "&lt;script&gt;bad&lt;/script&gt;"));
  g_assert_null (strstr (html, "<script>"));
  g_assert_null (strstr (html, "href=\"javascript:"));
}

static void
test_invalid (void)
{
  const char *invalid[] = {
    "<rss/>",
    "<CHANNEL>",
    "<!DOCTYPE CHANNEL [<!ENTITY local SYSTEM 'file:///nonexistent'>]><CHANNEL><TITLE>&local;</TITLE></CHANNEL>",
    NULL
  };

  for (guint i = 0; invalid[i]; i++) {
    g_autoptr (GError) error = NULL;
    g_autofree char *html = ephy_cdf_to_html (invalid[i], strlen (invalid[i]), &error);

    g_assert_null (html);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
  }
}

static void
test_limits (void)
{
  g_autofree char *oversized = g_strnfill (1024 * 1024 + 1, ' ');
  g_autoptr (GError) error = NULL;
  g_autofree char *html = ephy_cdf_to_html (oversized, strlen (oversized), &error);

  g_assert_null (html);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/cdf/channel", test_channel);
  g_test_add_func ("/cdf/invalid", test_invalid);
  g_test_add_func ("/cdf/limits", test_limits);
  return g_test_run ();
}
