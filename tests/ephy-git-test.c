/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-git.h"

static void
test_valid (void)
{
  static const struct {
    const char *uri;
    const char *host;
    guint16 port;
    const char *path;
  } cases[] = {
    { "git://example.invalid/project.git", "example.invalid", 9418, "/project.git" },
    { "GIT://example.invalid:9419/team/project", "example.invalid", 9419, "/team/project" },
    { "git://[::1]/project.git", "::1", 9418, "/project.git" },
    { "git://127.0.0.1:9419/project.git", "127.0.0.1", 9419, "/project.git" },
    { "git://example.invalid/~alice/project.git", "example.invalid", 9418, "/~alice/project.git" },
    { "git://example.invalid/my%20project.git", "example.invalid", 9418, "/my project.git" },
    { "git://example.invalid/a/../project", "example.invalid", 9418, "/a/../project" },
    { "git://example.invalid/%2520", "example.invalid", 9418, "/%20" },
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyGitAddress) address = ephy_git_address_parse (cases[i].uri, &error);

    g_assert_no_error (error);
    g_assert_nonnull (address);
    g_assert_cmpstr (address->host, ==, cases[i].host);
    g_assert_cmpuint (address->port, ==, cases[i].port);
    g_assert_cmpstr (address->path, ==, cases[i].path);
  }
}

static void
test_invalid (void)
{
  static const char * const cases[] = {
    NULL, "", "https://example.invalid/project.git",
    "ssh://example.invalid/project.git", "file:///project.git",
    "ext::helper", "git@example.invalid:project.git",
    "git://example.invalid", "git://example.invalid/",
    "git:///project.git", "git://user@example.invalid/project",
    "git://user:secret@example.invalid/project",
    "git://example.invalid:/project", "git://example.invalid:0/project",
    "git://example.invalid:65536/project", "git://-example.invalid/project",
    "git://example..invalid/project", "git://%65xample.invalid/project",
    "git://example.invalid/project?query", "git://example.invalid/project#fragment",
    "git://example.invalid/a%00", "git://example.invalid/a%0A",
    "git://example.invalid/a%0D", "git://example.invalid/a%7F",
    "git://example.invalid/a%ZZ", "git://example.invalid/a b",
    "git://example.invalid/a%5Cb", "git://example.invalid/a%FF",
  };

  for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
    g_autoptr (GError) error = NULL;
    g_autoptr (EphyGitAddress) address = ephy_git_address_parse (cases[i], &error);

    g_assert_null (address);
    g_assert_nonnull (error);
  }
}

static void
test_length (void)
{
  g_autofree char *path = g_strnfill (8192, 'a');
  g_autofree char *uri = g_strconcat ("git://example.invalid/", path, NULL);
  g_autoptr (GError) error = NULL;
  g_autoptr (EphyGitAddress) address = ephy_git_address_parse (uri, &error);

  g_assert_null (address);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/git/address/valid", test_valid);
  g_test_add_func ("/git/address/invalid", test_invalid);
  g_test_add_func ("/git/address/length-limit", test_length);
  return g_test_run ();
}
