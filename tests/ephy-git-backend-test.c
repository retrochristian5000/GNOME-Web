/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "config.h"
#include "ephy-git-backend.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if HAVE_LEGACY_GIT
#include <git2.h>
#endif

int
main (void)
{
  EphyGitBackendInfo info = { -1, -1, -1, ~0u };
  char oversized[4100];

#if HAVE_LEGACY_GIT
  int major, minor, patch;
  int features;
  int initial_count;
  unsigned int expected = 0;
  static const char * const invalid[] = {
    "refs/heads/a..b", "refs/heads/main~1", "refs/heads/a:b",
    "refs/heads/a.lock", "refs/heads/a b", "refs/heads/a//b",
    "refs/heads/a@{b", "refs/heads/a*", "refs/heads/a\nb",
  };
#endif

  assert (!ephy_git_backend_get_info (NULL));
  assert (ephy_git_backend_validate_reference (NULL) == EPHY_GIT_REFERENCE_INVALID);
  assert (ephy_git_backend_validate_reference ("HEAD") == EPHY_GIT_REFERENCE_INVALID);
  assert (ephy_git_backend_validate_reference ("main") == EPHY_GIT_REFERENCE_INVALID);
  assert (ephy_git_backend_validate_reference ("../refs/heads/main") == EPHY_GIT_REFERENCE_INVALID);

  memset (oversized, 'a', sizeof (oversized));
  memcpy (oversized, "refs/", 5);
  oversized[sizeof (oversized) - 1] = '\0';
  assert (ephy_git_backend_validate_reference (oversized) == EPHY_GIT_REFERENCE_INVALID);

#if HAVE_LEGACY_GIT
  initial_count = git_libgit2_init ();
  assert (initial_count > 0);
  assert (git_libgit2_version (&major, &minor, &patch) == 0);
  features = git_libgit2_features ();
  if (features & GIT_FEATURE_THREADS)
    expected |= EPHY_GIT_FEATURE_THREADS;
  if (features & GIT_FEATURE_HTTPS)
    expected |= EPHY_GIT_FEATURE_HTTPS;
  if (features & GIT_FEATURE_SSH)
    expected |= EPHY_GIT_FEATURE_SSH;

  for (int i = 0; i < 3; i++) {
    assert (ephy_git_backend_get_info (&info));
    assert (info.major == major && info.minor == minor && info.patch == patch);
    assert (info.features == expected);
    assert (ephy_git_backend_validate_reference ("refs/heads/main") == EPHY_GIT_REFERENCE_VALID);
    assert (ephy_git_backend_validate_reference ("refs/tags/v1.0") == EPHY_GIT_REFERENCE_VALID);
    for (size_t j = 0; j < sizeof (invalid) / sizeof (invalid[0]); j++)
      assert (ephy_git_backend_validate_reference (invalid[j]) == EPHY_GIT_REFERENCE_INVALID);
  }

  assert (git_libgit2_shutdown () == initial_count - 1);
#else
  assert (!ephy_git_backend_get_info (&info));
  assert (info.major == 0 && info.minor == 0 && info.patch == 0 && info.features == 0);
  assert (ephy_git_backend_validate_reference ("refs/heads/main") == EPHY_GIT_REFERENCE_UNAVAILABLE);
#endif

  puts ("Git backend offline checks passed");
  return 0;
}
