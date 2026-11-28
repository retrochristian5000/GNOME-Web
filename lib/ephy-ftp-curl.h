/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <curl/curl.h>
#include <gio/gio.h>

/* Only for an explicitly authorized native transfer. The application must
 * initialize libcurl first. resolved_proxy is a trusted resolver result,
 * not URI input. No networking occurs during preparation.
 *
 * The caller owns the returned fresh easy handle, must install a bounded
 * data sink and cancellation before starting it, and must clean it up.
 * The default sink rejects all response data. */
CURL *ephy_ftp_curl_prepare (const char *uri,
                            gboolean    require_explicit_tls,
                            const char *resolved_proxy,
                            GError    **error);
