/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-ftp.h"

static void
test_valid (void)
{
  static const struct {
    const char *uri;
    gboolean explicit_tls;
    EphyFtpSecurity security;
    EphyFtpTransfer transfer;
    guint16 port;
    gboolean directory;
    const char *username;
  } cases[] = {
    { "ftp://example.invalid", FALSE, EPHY_FTP_SECURITY_PLAINTEXT, EPHY_FTP_TRANSFER_AUTO, 21, TRUE, NULL },
    { "FTP://anonymous@example.invalid/pub/", FALSE, EPHY_FTP_SECURITY_PLAINTEXT, EPHY_FTP_TRANSFER_AUTO, 21, TRUE, "anonymous" },
    { "ftp://example.invalid/file;type=a", TRUE, EPHY_FTP_SECURITY_EXPLICIT_TLS, EPHY_FTP_TRANSFER_ASCII, 21, FALSE, NULL },
    { "ftps://example.invalid/file;TYPE=I", FALSE, EPHY_FTP_SECURITY_IMPLICIT_TLS, EPHY_FTP_TRANSFER_BINARY, 990, FALSE, NULL },
    { "ftps://example.invalid:2121/pub;type=d", TRUE, EPHY_FTP_SECURITY_IMPLICIT_TLS, EPHY_FTP_TRANSFER_DIRECTORY, 2121, TRUE, NULL },
    { "ftp://[::1]:2121/file", FALSE, EPHY_FTP_SECURITY_PLAINTEXT, EPHY_FTP_TRANSFER_AUTO, 2121, FALSE, NULL },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyFtpAddress) address = ephy_ftp_address_parse (cases[i].uri, cases[i].explicit_tls, &error);

    g_assert_no_error (error);
    g_assert_nonnull (address);
    g_assert_cmpint (address->security, ==, cases[i].security);
    g_assert_cmpint (address->transfer, ==, cases[i].transfer);
    g_assert_cmpuint (address->port, ==, cases[i].port);
    g_assert_cmpint (address->directory_hint, ==, cases[i].directory);
    g_assert_cmpstr (address->username, ==, cases[i].username);
  }
}

static void
test_components (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (EphyFtpAddress) address =
    ephy_ftp_address_parse ("ftp://example.invalid/%2Fpub/a/../file%3Btype%3Da/%2520;type=i", FALSE, &error);

  g_assert_no_error (error);
  g_assert_nonnull (address);
  g_assert_cmpstr (address->host, ==, "example.invalid");
  g_assert_cmpuint (g_strv_length (address->components), ==, 5);
  g_assert_cmpstr (address->components[0], ==, "/pub");
  g_assert_cmpstr (address->components[1], ==, "a");
  g_assert_cmpstr (address->components[2], ==, "..");
  g_assert_cmpstr (address->components[3], ==, "file;type=a");
  g_assert_cmpstr (address->components[4], ==, "%20");
  g_assert_cmpint (address->transfer, ==, EPHY_FTP_TRANSFER_BINARY);
}

static void
test_invalid (void)
{
  static const char * const cases[] = {
    NULL, "", "sftp://example.invalid/file", "tftp://example.invalid/file",
    "ftp:///file", "ftp://example.invalid:", "ftp://example.invalid:0/file",
    "ftp://example.invalid:65536/file", "ftp://-example.invalid/file",
    "ftp://example..invalid/file", "ftp://@example.invalid/file",
    "ftp://user:secret@example.invalid/file", "ftp://user%3Asecret@example.invalid/file",
    "ftp://user%0A@example.invalid/file", "ftp://user@@example.invalid/file",
    "ftp://%65xample.invalid/file", "ftp://example.invalid/file?query",
    "ftp://example.invalid/file#fragment", "ftp://example.invalid/file;type=x",
    "ftp://example.invalid/file;type=i;type=a", "ftp://example.invalid/;type=i",
    "ftp://example.invalid/pub/;type=a", "ftp://example.invalid/file%00",
    "ftp://example.invalid/file%0D", "ftp://example.invalid/file%0A",
    "ftp://example.invalid/file%ZZ", "ftp://example.invalid/a b",
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyFtpAddress) address = ephy_ftp_address_parse (cases[i], FALSE, &error);

    g_assert_null (address);
    g_assert_nonnull (error);
  }
}

static void
test_length (void)
{
  g_autofree char *path = g_strnfill (8192, 'a');
  g_autofree char *uri = g_strconcat ("ftp://example.invalid/", path, NULL);
  g_autoptr (GError) error = NULL;
  g_autoptr (EphyFtpAddress) address = ephy_ftp_address_parse (uri, FALSE, &error);

  g_assert_null (address);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/ftp/address/valid", test_valid);
  g_test_add_func ("/ftp/address/components", test_components);
  g_test_add_func ("/ftp/address/invalid", test_invalid);
  g_test_add_func ("/ftp/address/length-limit", test_length);
  return g_test_run ();
}
