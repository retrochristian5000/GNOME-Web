/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <stdbool.h>

typedef enum {
  EPHY_GIT_FEATURE_THREADS = 1u << 0,
  EPHY_GIT_FEATURE_HTTPS = 1u << 1,
  EPHY_GIT_FEATURE_SSH = 1u << 2,
} EphyGitFeature;

typedef struct {
  int major;
  int minor;
  int patch;
  unsigned int features;
} EphyGitBackendInfo;

typedef enum {
  EPHY_GIT_REFERENCE_UNAVAILABLE,
  EPHY_GIT_REFERENCE_INVALID,
  EPHY_GIT_REFERENCE_VALID,
  EPHY_GIT_REFERENCE_ERROR,
} EphyGitReferenceStatus;

/* Offline query. Owns a balanced libgit2 initialization reference for
 * the duration of the call. Returns false and zeroes info if unavailable. */
bool ephy_git_backend_get_info (EphyGitBackendInfo *info);

/* Validate a full reference name (refs/...), not a revspec or a path.
 * Does not resolve the reference or access any repository. */
EphyGitReferenceStatus ephy_git_backend_validate_reference (const char *name);
