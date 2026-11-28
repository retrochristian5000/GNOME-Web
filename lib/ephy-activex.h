/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

typedef enum {
  EPHY_ACTIVEX_NOT_DECLARED,
  EPHY_ACTIVEX_INVALID,
  EPHY_ACTIVEX_UNKNOWN,
  EPHY_ACTIVEX_FLASH,
  EPHY_ACTIVEX_WINDOWS_MEDIA,
  EPHY_ACTIVEX_QUICKTIME,
} EphyActiveXControl;

typedef enum {
  EPHY_ACTIVEX_FALLBACK_NONE,
  EPHY_ACTIVEX_FALLBACK_RUFFLE,
  EPHY_ACTIVEX_FALLBACK_NATIVE_MEDIA,
} EphyActiveXFallback;

/* Recognize declarations only. The result never grants execution permission.
 * No codebase processing, installation, registry access or COM activation. */
EphyActiveXControl ephy_activex_classify (const char *classid);

/* Return a candidate replacement, not a claim that it is installed or
 * that it implements the original control's scripting interface. */
EphyActiveXFallback ephy_activex_get_fallback (EphyActiveXControl control);
