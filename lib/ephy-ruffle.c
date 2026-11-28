/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "ephy-ruffle.h"

#include <string.h>

gboolean
ephy_ruffle_is_swf (GFile *file)
{
  g_autofree char *name = g_file_get_basename (file);
  gsize length = name ? strlen (name) : 0;

  return length >= 4 && g_ascii_strcasecmp (name + length - 4, ".swf") == 0;
}

gboolean
ephy_ruffle_open_file (GFile   *file,
                       GError **error)
{
#if HAVE_RUFFLE
  g_autofree char *path = NULL;
  g_autofree char *player = NULL;
  g_autoptr (GFileInfo) info = NULL;
  g_autoptr (GSubprocess) process = NULL;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  path = g_file_get_path (file);
  if (!path || !ephy_ruffle_is_swf (file)) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         "Select a locally saved SWF movie");
    return FALSE;
  }

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                            G_FILE_QUERY_INFO_NONE, NULL, error);
  if (!info)
    return FALSE;
  if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                         "The SWF movie must be a regular file");
    return FALSE;
  }

  player = g_find_program_in_path ("ruffle");
  if (!player)
    player = g_find_program_in_path ("ruffle_desktop");
#ifdef __APPLE__
  if (!player && g_file_test ("/Applications/Ruffle.app/Contents/MacOS/ruffle",
                              G_FILE_TEST_IS_EXECUTABLE))
    player = g_strdup ("/Applications/Ruffle.app/Contents/MacOS/ruffle");
#endif
  if (!player) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                         "Ruffle Desktop is not installed or cannot be found");
    return FALSE;
  }

  /* Only called for a user-selected local movie. No shell, no browser
   * credentials, no browser script bridge and no permissive access flags.
   * These player policy options do not constitute an OS sandbox. */
  process = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE, error,
                              player,
                              "--filesystem-access-mode", "deny",
                              "--open-url-mode", "deny",
                              "--", path, NULL);
  return process != NULL;
#else
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       "Ruffle integration was disabled at build time");
  return FALSE;
#endif
}
