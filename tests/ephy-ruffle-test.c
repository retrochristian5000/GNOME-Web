/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "ephy-ruffle.h"

static void
test_suffix (void)
{
  g_autoptr (GFile) swf = g_file_new_for_path ("movie.SWF");
  g_autoptr (GFile) other = g_file_new_for_path ("movie.swf.html");
  g_autoptr (GFile) remote = g_file_new_for_uri ("https://example.invalid/movie.swf");

  g_assert_true (ephy_ruffle_is_swf (swf));
  g_assert_false (ephy_ruffle_is_swf (other));
  g_assert_true (ephy_ruffle_is_swf (remote));
}

static void
test_remote_rejected (void)
{
  g_autoptr (GFile) remote = g_file_new_for_uri ("https://example.invalid/movie.swf");
  g_autoptr (GError) error = NULL;

  g_assert_false (ephy_ruffle_open_file (remote, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
}

static void
test_other_rejected (void)
{
  g_autoptr (GFile) other = g_file_new_for_path ("movie.swf.html");
  g_autoptr (GError) error = NULL;

  g_assert_false (ephy_ruffle_open_file (other, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/ruffle/suffix", test_suffix);
  g_test_add_func ("/ruffle/remote-rejected", test_remote_rejected);
  g_test_add_func ("/ruffle/other-rejected", test_other_rejected);
  return g_test_run ();
}
