/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "ephy-password-generator.h"

#if USE_NATIVE_PASSWORD_GENERATOR
#include <Security/Security.h>
#else
#include <stdlib.h>
#include <pwquality.h>
#endif

char *
ephy_generate_secure_password (void)
{
#if USE_NATIVE_PASSWORD_GENERATOR
  static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@";
  enum { PASSWORD_LENGTH = 24, MAX_ATTEMPTS = 128 };
  g_autofree char *password = g_malloc (PASSWORD_LENGTH + 1);

  G_STATIC_ASSERT (sizeof (alphabet) == 65);

  /* Six independent random bits per character, without modulo bias.
   * Reject whole candidates missing a character class. Even the subset
   * with upper/lower/digit/symbol in the first four positions contains
   * 26 * 26 * 10 * 2 * 64^20 possible passwords (> 2^128).
   */
  for (unsigned int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
    unsigned int classes = 0;
    int status = SecRandomCopyBytes (kSecRandomDefault, PASSWORD_LENGTH, password);

    if (status != errSecSuccess) {
      g_warning ("Failed to generate secure random password (Security status %d)", status);
      return NULL;
    }

    for (unsigned int i = 0; i < PASSWORD_LENGTH; i++) {
      unsigned int index = ((unsigned char)password[i]) & 63u;

      password[i] = alphabet[index];
      if (index < 26)
        classes |= 1u;
      else if (index < 52)
        classes |= 2u;
      else if (index < 62)
        classes |= 4u;
      else
        classes |= 8u;
    }

    if (classes == 15u) {
      password[PASSWORD_LENGTH] = '\0';
      return g_steal_pointer (&password);
    }
  }

  g_warning ("Failed to generate a password with all required character classes");
  return NULL;
#else
  pwquality_settings_t *settings = pwquality_default_settings ();
  char *result = NULL;
  char *password;
  int ret;

  if (!settings) {
    g_warning ("Failed to initialize secure password generation");
    return NULL;
  }

  ret = pwquality_generate (settings, 128, &result);
  pwquality_free_settings (settings);

  if (ret < 0 || !result) {
    g_warning ("Failed to generate secure random password: %s",
               ret < 0 ? pwquality_strerror (NULL, 0, ret, NULL) : "no password returned");
    free (result);
    return NULL;
  }

  password = g_strdup (result);
  free (result);
  return password;
#endif
}
