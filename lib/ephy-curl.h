/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <stdbool.h>

typedef enum {
  EPHY_CURL_FTP = 1u << 0,
  EPHY_CURL_FTPS_EXPLICIT = 1u << 1,
  EPHY_CURL_FTPS_IMPLICIT = 1u << 2,
  EPHY_CURL_GOPHER = 1u << 3,
  EPHY_CURL_TFTP = 1u << 4,
  EPHY_CURL_SFTP = 1u << 5,
} EphyCurlCapability;

/* Pure mapping, with no I/O. Protocols are the advertised libcurl names. */
unsigned int ephy_curl_capabilities_from_protocols (const char * const *protocols,
                                                    bool                tls);

/* Call before other threads start, or after a successful curl_global_init
 * owned by the application. This does not initialize libcurl or transfer data.
 * Returns zero when the optional dependency was disabled or unavailable. */
unsigned int ephy_curl_get_capabilities (void);
