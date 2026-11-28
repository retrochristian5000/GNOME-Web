/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-gopher.h"

static void
test_addresses (void)
{
  static const struct {
    const char *uri;
    const char *host;
    guint16 port;
    char type;
    const char *selector;
    const char *search;
  } cases[] = {
    { "gopher://example.invalid", "example.invalid", 70, '1', "", NULL },
    { "gopher://example.invalid/", "example.invalid", 70, '1', "", NULL },
    { "GOPHER://example.invalid/1", "example.invalid", 70, '1', "", NULL },
    { "gopher://example.invalid:7070/00read%20me", "example.invalid", 7070, '0', "0read me", NULL },
    { "gopher://[::1]/1/docs", "::1", 70, '1', "/docs", NULL },
    { "gopher://example.invalid/7search%09old%20web", "example.invalid", 70, '7', "search", "old web" },
    { "gopher://example.invalid/7search", "example.invalid", 70, '7', "search", NULL },
    { "gopher://example.invalid/0%2520%3F%23", "example.invalid", 70, '0', "%20?#", NULL },
    { "gopher://example.invalid/1/a/../b", "example.invalid", 70, '1', "/a/../b", NULL },
    { "gopher://example.invalid/1/a/%2E%2E/b", "example.invalid", 70, '1', "/a/../b", NULL },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyGopherAddress) address = ephy_gopher_address_parse (cases[i].uri, &error);

    g_assert_no_error (error);
    g_assert_nonnull (address);
    g_assert_cmpstr (address->host, ==, cases[i].host);
    g_assert_cmpuint (address->port, ==, cases[i].port);
    g_assert_cmpint (address->type, ==, cases[i].type);
    g_assert_cmpstr (address->selector, ==, cases[i].selector);
    g_assert_cmpstr (address->search, ==, cases[i].search);
  }
}

static void
test_invalid_addresses (void)
{
  static const char * const cases[] = {
    "", "http://example.invalid/", "gopher:///1", "gopher://user@example.invalid/1",
    "gopher://example.invalid:0/1", "gopher://example.invalid:65536/1",
    "gopher://example.invalid/0%00", "gopher://example.invalid/0%0D",
    "gopher://example.invalid/0%0A", "gopher://example.invalid/0%09",
    "gopher://example.invalid/7search%09term%09extra",
    "gopher://example.invalid/1bad%ZZ", "gopher://example.invalid/1?query",
    "gopher://-invalid.example/1", "gopher://example..invalid/1",
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyGopherAddress) address = ephy_gopher_address_parse (cases[i], &error);

    g_assert_null (address);
    g_assert_nonnull (error);
  }
}

static void
test_menu (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (EphyGopherMenuItem) item =
    ephy_gopher_menu_item_parse ("1<Documents>\t/docs?old#new\texample.invalid\t70", &error);
  g_autoptr (EphyGopherAddress) address = NULL;

  g_assert_no_error (error);
  g_assert_nonnull (item);
  g_assert_cmpstr (item->label, ==, "<Documents>");
  g_assert_cmpstr (item->uri, ==, "gopher://example.invalid/1/docs%3Fold%23new");
  address = ephy_gopher_address_parse (item->uri, &error);
  g_assert_no_error (error);
  g_assert_nonnull (address);
  g_assert_cmpstr (address->selector, ==, "/docs?old#new");

  g_clear_pointer (&item, ephy_gopher_menu_item_free);
  item = ephy_gopher_menu_item_parse ("iInformation\t\tinvalid\t0", &error);
  g_assert_no_error (error);
  g_assert_nonnull (item);
  g_assert_null (item->uri);

  g_clear_pointer (&item, ephy_gopher_menu_item_free);
  item = ephy_gopher_menu_item_parse ("hExternal\tURL:https://example.invalid\texample.invalid\t70", &error);
  g_assert_no_error (error);
  g_assert_nonnull (item);
  g_assert_null (item->uri);
}

static void
test_invalid_menu (void)
{
  static const char * const cases[] = {
    ".", "1Missing fields", "1Bad\t/path\texample.invalid\t0",
    "1Bad\t/path\texample.invalid\t70junk",
    "1Bad\t/path\texample.invalid\t65536",
    "1Bad\t/path\texample.invalid\t70\n",
    "1Bad\t/path\tuser@example.invalid\t70",
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyGopherMenuItem) item = ephy_gopher_menu_item_parse (cases[i], &error);

    g_assert_null (item);
    g_assert_nonnull (error);
  }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/gopher/address/valid", test_addresses);
  g_test_add_func ("/gopher/address/invalid", test_invalid_addresses);
  g_test_add_func ("/gopher/menu/valid", test_menu);
  g_test_add_func ("/gopher/menu/invalid", test_invalid_menu);
  return g_test_run ();
}
