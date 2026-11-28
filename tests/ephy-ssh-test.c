/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-ssh.h"

static void
test_valid (void)
{
  static const struct {
    const char *uri;
    EphySshService service;
    const char *host;
    guint16 port;
    const char *username;
    const char *path;
  } cases[] = {
    { "ssh://example.invalid", EPHY_SSH_SERVICE_TERMINAL, "example.invalid", 22, NULL, NULL },
    { "SSH://alice@example.invalid/", EPHY_SSH_SERVICE_TERMINAL, "example.invalid", 22, "alice", NULL },
    { "ssh://alice@[::1]:2222", EPHY_SSH_SERVICE_TERMINAL, "::1", 2222, "alice", NULL },
    { "sftp://example.invalid", EPHY_SSH_SERVICE_FILE_TRANSFER, "example.invalid", 22, NULL, "/" },
    { "sftp://alice@example.invalid:2222/docs/read%20me", EPHY_SSH_SERVICE_FILE_TRANSFER, "example.invalid", 2222, "alice", "/docs/read me" },
    { "sftp://example.invalid/a/../b", EPHY_SSH_SERVICE_FILE_TRANSFER, "example.invalid", 22, NULL, "/a/../b" },
    { "sftp://example.invalid/%2520", EPHY_SSH_SERVICE_FILE_TRANSFER, "example.invalid", 22, NULL, "/%20" },
    { "ssh://al%69ce@example.invalid", EPHY_SSH_SERVICE_TERMINAL, "example.invalid", 22, "alice", NULL },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphySshAddress) address = ephy_ssh_address_parse (cases[i].uri, &error);

    g_assert_no_error (error);
    g_assert_nonnull (address);
    g_assert_cmpint (address->service, ==, cases[i].service);
    g_assert_cmpstr (address->host, ==, cases[i].host);
    g_assert_cmpuint (address->port, ==, cases[i].port);
    g_assert_cmpstr (address->username, ==, cases[i].username);
    g_assert_cmpstr (address->path, ==, cases[i].path);
  }
}

static void
test_invalid (void)
{
  static const char * const cases[] = {
    NULL, "", "https://example.invalid", "scp://example.invalid/file",
    "ssh:///host", "ssh://@example.invalid", "ssh://alice:secret@example.invalid",
    "ssh://alice%3Asecret@example.invalid", "ssh://-alice@example.invalid",
    "ssh://alice;fingerprint=value@example.invalid",
    "ssh://alice@example.invalid:0", "ssh://example.invalid:65536",
    "ssh://example.invalid:", "ssh://-example.invalid",
    "ssh://example..invalid", "ssh://example.invalid/command",
    "ssh://example.invalid?command=value", "ssh://example.invalid#fragment",
    "ssh://alice%0A@example.invalid", "ssh://alice%00@example.invalid",
    "sftp://example.invalid/a%00", "sftp://example.invalid/a%0D",
    "sftp://example.invalid/a%ZZ", "sftp://example.invalid/a b",
    "ssh://alice@@example.invalid", "ssh://%65xample.invalid",
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphySshAddress) address = ephy_ssh_address_parse (cases[i], &error);

    g_assert_null (address);
    g_assert_nonnull (error);
  }
}

static void
test_length_limit (void)
{
  g_autofree char *path = g_strnfill (8192, 'a');
  g_autofree char *uri = g_strconcat ("sftp://example.invalid/", path, NULL);
  g_autoptr (GError) error = NULL;
  g_autoptr (EphySshAddress) address = ephy_ssh_address_parse (uri, &error);

  g_assert_null (address);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/ssh/address/valid", test_valid);
  g_test_add_func ("/ssh/address/invalid", test_invalid);
  g_test_add_func ("/ssh/address/length-limit", test_length_limit);
  return g_test_run ();
}
