/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "ephy-password-generator.h"

#include <string.h>

static void
test_generate (void)
{
  for (unsigned int i = 0; i < 32; i++) {
    g_autofree char *password = ephy_generate_secure_password ();

    /* Do not include a password in assertion output. */
    g_assert_true (password != NULL && *password != '\0');
#if USE_NATIVE_PASSWORD_GENERATOR
    gboolean upper = FALSE, lower = FALSE, digit = FALSE, symbol = FALSE;

    g_assert_true (strlen (password) == 24);
    for (const char *p = password; *p; p++) {
      if (g_ascii_isupper (*p))
        upper = TRUE;
      else if (g_ascii_islower (*p))
        lower = TRUE;
      else if (g_ascii_isdigit (*p))
        digit = TRUE;
      else if (*p == '!' || *p == '@')
        symbol = TRUE;
      else
        g_assert_not_reached ();
    }
    g_assert_true (upper && lower && digit && symbol);
#endif
  }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/password-generator/generate", test_generate);
  return g_test_run ();
}
