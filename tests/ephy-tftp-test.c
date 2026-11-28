/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-tftp.h"

static void
test_valid_addresses (void)
{
  static const struct {
    const char *uri;
    const char *host;
    const char *filename;
    EphyTftpMode mode;
  } cases[] = {
    { "tftp://example.invalid/readme.txt", "example.invalid", "readme.txt", EPHY_TFTP_MODE_OCTET },
    { "TFTP://example.invalid/notes;MODE=NETASCII", "example.invalid", "notes", EPHY_TFTP_MODE_NETASCII },
    { "tftp://example.invalid/file;mode=octet", "example.invalid", "file", EPHY_TFTP_MODE_OCTET },
    { "tftp://[::1]/notes", "::1", "notes", EPHY_TFTP_MODE_OCTET },
    { "tftp://127.0.0.1/notes", "127.0.0.1", "notes", EPHY_TFTP_MODE_OCTET },
    { "tftp://example.invalid/docs%2Fread%20me", "example.invalid", "docs/read me", EPHY_TFTP_MODE_OCTET },
    { "tftp://example.invalid/file%3Bmode%3Dnetascii", "example.invalid", "file;mode=netascii", EPHY_TFTP_MODE_OCTET },
    { "tftp://example.invalid/%252F", "example.invalid", "%2F", EPHY_TFTP_MODE_OCTET },
    { "tftp://example.invalid/%2E%2E", "example.invalid", "..", EPHY_TFTP_MODE_OCTET },
    { "tftp://example.invalid/file%3F%23", "example.invalid", "file?#", EPHY_TFTP_MODE_OCTET },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyTftpAddress) address = ephy_tftp_address_parse (cases[i].uri, &error);

    g_assert_no_error (error);
    g_assert_nonnull (address);
    g_assert_cmpstr (address->host, ==, cases[i].host);
    g_assert_cmpuint (address->port, ==, 69);
    g_assert_cmpstr (address->filename, ==, cases[i].filename);
    g_assert_cmpint (address->mode, ==, cases[i].mode);
  }
}

static void
test_invalid_addresses (void)
{
  static const char * const cases[] = {
    NULL, "", "ftp://example.invalid/file",
    "tftp://example.invalid", "tftp://example.invalid/",
    "tftp:///file", "tftp://user@example.invalid/file",
    "tftp://example.invalid:69/file", "tftp://example.invalid:/file",
    "tftp://[::1]:69/file", "tftp://::1/file",
    "tftp://-example.invalid/file", "tftp://example..invalid/file",
    "tftp://example.invalid/file?query", "tftp://example.invalid/file#fragment",
    "tftp://example.invalid/dir/file", "tftp://example.invalid/a b",
    "tftp://example.invalid/file;mode=mail", "tftp://example.invalid/file;mode=",
    "tftp://example.invalid/file;mode=octet;mode=netascii",
    "tftp://example.invalid/file%00", "tftp://example.invalid/file%0A",
    "tftp://example.invalid/file%0D", "tftp://example.invalid/file%09",
    "tftp://example.invalid/file%7F", "tftp://example.invalid/file%",
    "tftp://example.invalid/file%2", "tftp://example.invalid/file%ZZ",
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyTftpAddress) address = ephy_tftp_address_parse (cases[i], &error);

    g_assert_null (address);
    g_assert_nonnull (error);
  }
}

static void
test_uri_limit (void)
{
  g_autofree char *filename = g_strnfill (8192, 'a');
  g_autofree char *uri = g_strconcat ("tftp://example.invalid/", filename, NULL);
  g_autoptr (GError) error = NULL;
  g_autoptr (EphyTftpAddress) address = ephy_tftp_address_parse (uri, &error);

  g_assert_null (address);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/tftp/address/valid", test_valid_addresses);
  g_test_add_func ("/tftp/address/invalid", test_invalid_addresses);
  g_test_add_func ("/tftp/address/length-limit", test_uri_limit);
  return g_test_run ();
}
