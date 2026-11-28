/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-ftp-curl.h"

#include "ephy-curl.h"
#include "ephy-ftp.h"

#include <string.h>

static size_t
reject_data (char   *data,
             size_t  size,
             size_t  count,
             void   *user_data)
{
  return 0;
}

static char *
build_transfer_url (const EphyFtpAddress *address,
                    const char          *scheme)
{
  g_autoptr (GString) path = g_string_new ("/");

  for (gsize i = 0; address->components[i]; i++) {
    g_autofree char *encoded = g_uri_escape_string (address->components[i], NULL, FALSE);

    if (i > 0)
      g_string_append_c (path, '/');
    g_string_append (path, encoded);
  }

  if (address->directory_hint && path->str[path->len - 1] != '/')
    g_string_append_c (path, '/');

  return g_uri_join (G_URI_FLAGS_ENCODED_PATH, scheme, NULL,
                     address->host, address->port, path->str, NULL, NULL);
}

static gboolean
valid_proxy (const char *route)
{
  g_autoptr (GUri) proxy = NULL;
  const char *scheme;
  const char *path;

  if (!route || !*route)
    return FALSE;
  if (strcmp (route, "direct://") == 0)
    return TRUE;

  proxy = g_uri_parse (route, G_URI_FLAGS_NONE, NULL);
  if (!proxy || !g_uri_get_host (proxy) || !*g_uri_get_host (proxy) ||
      g_uri_get_userinfo (proxy) || g_uri_get_query (proxy) ||
      g_uri_get_fragment (proxy) || g_uri_get_port (proxy) == 0)
    return FALSE;

  scheme = g_uri_get_scheme (proxy);
  path = g_uri_get_path (proxy);
  if (path && *path && strcmp (path, "/") != 0)
    return FALSE;

  /* Do not silently reinterpret unsupported resolver routes. */
  return g_strcmp0 (scheme, "http") == 0 ||
         g_strcmp0 (scheme, "https") == 0 ||
         g_strcmp0 (scheme, "socks5h") == 0;
}

CURL *
ephy_ftp_curl_prepare (const char *uri,
                       gboolean    require_explicit_tls,
                       const char *resolved_proxy,
                       GError    **error)
{
  g_autoptr (EphyFtpAddress) address = NULL;
  g_autofree char *url = NULL;
  const char *scheme;
  unsigned int required;
  CURL *handle;
  CURLcode status;

  address = ephy_ftp_address_parse (uri, require_explicit_tls, error);
  if (!address)
    return NULL;

  if (!valid_proxy (resolved_proxy)) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         "FTP requires an explicitly resolved supported proxy route");
    return NULL;
  }

  /* Named credentials need a native authentication flow, not URL secrets
   * or an implicit empty password. Anonymous reads are the initial scope. */
  if (address->username && strcmp (address->username, "anonymous") != 0) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         "Named FTP authentication is not implemented");
    return NULL;
  }

  scheme = address->security == EPHY_FTP_SECURITY_IMPLICIT_TLS ? "ftps" : "ftp";
  required = address->security == EPHY_FTP_SECURITY_IMPLICIT_TLS ? EPHY_CURL_FTPS_IMPLICIT :
             address->security == EPHY_FTP_SECURITY_EXPLICIT_TLS ? EPHY_CURL_FTPS_EXPLICIT :
             EPHY_CURL_FTP;
  if (!(ephy_curl_get_capabilities () & required)) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         "Installed libcurl does not support the requested FTP security mode");
    return NULL;
  }

  url = build_transfer_url (address, scheme);
  handle = curl_easy_init ();
  if (!handle) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Could not initialize FTP transfer");
    return NULL;
  }

#define SET_OPTION(option, value) G_STMT_START { \
  status = curl_easy_setopt (handle, option, value); \
  if (status != CURLE_OK) \
    goto failed; \
} G_STMT_END

  SET_OPTION (CURLOPT_PROTOCOLS_STR, scheme);
  SET_OPTION (CURLOPT_REDIR_PROTOCOLS_STR, scheme);
  SET_OPTION (CURLOPT_FOLLOWLOCATION, 0L);
  SET_OPTION (CURLOPT_URL, url);
  SET_OPTION (CURLOPT_PATH_AS_IS, 1L);
  SET_OPTION (CURLOPT_NETRC, (long)CURL_NETRC_IGNORED);
  SET_OPTION (CURLOPT_USERNAME, "anonymous");
  SET_OPTION (CURLOPT_PASSWORD, "anonymous@");
  SET_OPTION (CURLOPT_USE_SSL, address->security == EPHY_FTP_SECURITY_PLAINTEXT ?
              (long)CURLUSESSL_NONE : (long)CURLUSESSL_ALL);
  SET_OPTION (CURLOPT_SSL_VERIFYPEER, 1L);
  SET_OPTION (CURLOPT_SSL_VERIFYHOST, 2L);
  SET_OPTION (CURLOPT_PROXY_SSL_VERIFYPEER, 1L);
  SET_OPTION (CURLOPT_PROXY_SSL_VERIFYHOST, 2L);
  SET_OPTION (CURLOPT_FTP_FILEMETHOD, (long)CURLFTPMETHOD_MULTICWD);
  SET_OPTION (CURLOPT_FTP_SKIP_PASV_IP, 1L);
  SET_OPTION (CURLOPT_FTP_USE_EPSV, 1L);
  SET_OPTION (CURLOPT_TRANSFERTEXT, address->transfer == EPHY_FTP_TRANSFER_ASCII ? 1L : 0L);
  SET_OPTION (CURLOPT_DIRLISTONLY, address->transfer == EPHY_FTP_TRANSFER_DIRECTORY ? 1L : 0L);
  SET_OPTION (CURLOPT_UPLOAD, 0L);
  SET_OPTION (CURLOPT_PROXY, strcmp (resolved_proxy, "direct://") == 0 ? "" : resolved_proxy);
  /* Ignore environment bypass lists; the supplied route is authoritative. */
  SET_OPTION (CURLOPT_NOPROXY, "");
  SET_OPTION (CURLOPT_HTTPPROXYTUNNEL, 1L);
  SET_OPTION (CURLOPT_NOSIGNAL, 1L);
  SET_OPTION (CURLOPT_CONNECTTIMEOUT_MS, 15000L);
  SET_OPTION (CURLOPT_LOW_SPEED_LIMIT, 1L);
  SET_OPTION (CURLOPT_LOW_SPEED_TIME, 30L);
  SET_OPTION (CURLOPT_WRITEFUNCTION, reject_data);

#undef SET_OPTION

  return handle;

failed:
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Could not configure FTP transfer: %s", curl_easy_strerror (status));
  curl_easy_cleanup (handle);
  return NULL;
}
