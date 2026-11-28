/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "config.h"
#include "ephy-git-backend.h"

#include <string.h>

#if HAVE_LEGACY_GIT
#include <git2.h>
#endif

bool
ephy_git_backend_get_info (EphyGitBackendInfo *info)
{
#if HAVE_LEGACY_GIT
  int features;
  int status;
#endif

  if (!info)
    return false;

  *info = (EphyGitBackendInfo){ 0 };

#if HAVE_LEGACY_GIT
  if (git_libgit2_init () < 0)
    return false;

  status = git_libgit2_version (&info->major, &info->minor, &info->patch);
  if (status == 0) {
    features = git_libgit2_features ();
    if (features & GIT_FEATURE_THREADS)
      info->features |= EPHY_GIT_FEATURE_THREADS;
    if (features & GIT_FEATURE_HTTPS)
      info->features |= EPHY_GIT_FEATURE_HTTPS;
    if (features & GIT_FEATURE_SSH)
      info->features |= EPHY_GIT_FEATURE_SSH;
  }

  if (git_libgit2_shutdown () < 0 || status != 0) {
    *info = (EphyGitBackendInfo){ 0 };
    return false;
  }
  return true;
#else
  return false;
#endif
}

EphyGitReferenceStatus
ephy_git_backend_validate_reference (const char *name)
{
#if HAVE_LEGACY_GIT
  int valid = 0;
  int status;
#endif

  /* Limit input before invoking the backend. HEAD and revision expressions
   * are intentionally not part of the full-reference selection interface. */
  if (!name || strncmp (name, "refs/", 5) != 0 ||
      strlen (name) > 4096)
    return EPHY_GIT_REFERENCE_INVALID;

#if HAVE_LEGACY_GIT
  if (git_libgit2_init () < 0)
    return EPHY_GIT_REFERENCE_ERROR;

  status = git_reference_name_is_valid (&valid, name);

  if (git_libgit2_shutdown () < 0 || status != 0)
    return EPHY_GIT_REFERENCE_ERROR;

  return valid ? EPHY_GIT_REFERENCE_VALID : EPHY_GIT_REFERENCE_INVALID;
#else
  return EPHY_GIT_REFERENCE_UNAVAILABLE;
#endif
}
