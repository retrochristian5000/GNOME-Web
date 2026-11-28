/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "config.h"
#include "ephy-curl.h"

#include <string.h>

#if HAVE_LEGACY_CURL
#include <curl/curl.h>
#endif

unsigned int
ephy_curl_capabilities_from_protocols (const char * const *protocols,
                                      bool                tls)
{
  unsigned int capabilities = 0;

  if (!protocols)
    return 0;

  for (const char * const *p = protocols; *p; p++) {
    if (strcmp (*p, "ftp") == 0)
      capabilities |= EPHY_CURL_FTP | (tls ? EPHY_CURL_FTPS_EXPLICIT : 0);
    else if (strcmp (*p, "ftps") == 0 && tls)
      capabilities |= EPHY_CURL_FTPS_IMPLICIT;
    else if (strcmp (*p, "gopher") == 0)
      capabilities |= EPHY_CURL_GOPHER;
    else if (strcmp (*p, "tftp") == 0)
      capabilities |= EPHY_CURL_TFTP;
    else if (strcmp (*p, "sftp") == 0)
      capabilities |= EPHY_CURL_SFTP;
  }

  return capabilities;
}

unsigned int
ephy_curl_get_capabilities (void)
{
#if HAVE_LEGACY_CURL
  const curl_version_info_data *info = curl_version_info (CURLVERSION_NOW);

  if (!info)
    return 0;

  return ephy_curl_capabilities_from_protocols (info->protocols,
                                                (info->features & CURL_VERSION_SSL) != 0);
#else
  return 0;
#endif
}
