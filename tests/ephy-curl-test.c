/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "config.h"
#include "ephy-curl.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#if HAVE_LEGACY_CURL
#include <curl/curl.h>
#endif

int
main (void)
{
  static const char * const all[] = { "ftp", "ftps", "gopher", "tftp", "sftp", NULL };
  static const char * const ftp[] = { "ftp", NULL };
  static const char * const unrelated[] = { "http", "https", "file", "scp", "gophers", "ftp-extra", NULL };
  unsigned int plain = EPHY_CURL_FTP | EPHY_CURL_GOPHER | EPHY_CURL_TFTP | EPHY_CURL_SFTP;
  unsigned int secure = EPHY_CURL_FTPS_EXPLICIT | EPHY_CURL_FTPS_IMPLICIT;

#if HAVE_LEGACY_CURL
  const curl_version_info_data *info;
#endif

  assert (ephy_curl_capabilities_from_protocols (NULL, true) == 0);
  assert (ephy_curl_capabilities_from_protocols (all, false) == plain);
  assert (ephy_curl_capabilities_from_protocols (all, true) == (plain | secure));
  assert (ephy_curl_capabilities_from_protocols (ftp, true) == (EPHY_CURL_FTP | EPHY_CURL_FTPS_EXPLICIT));
  assert (ephy_curl_capabilities_from_protocols (unrelated, true) == 0);

#if HAVE_LEGACY_CURL
  assert (curl_global_init (CURL_GLOBAL_DEFAULT) == CURLE_OK);
  info = curl_version_info (CURLVERSION_NOW);
  assert (info != NULL);
  assert (ephy_curl_get_capabilities () ==
          ephy_curl_capabilities_from_protocols (info->protocols,
                                                (info->features & CURL_VERSION_SSL) != 0));
  curl_global_cleanup ();
#else
  assert (ephy_curl_get_capabilities () == 0);
#endif

  puts ("Legacy curl capability checks passed");
  return 0;
}
