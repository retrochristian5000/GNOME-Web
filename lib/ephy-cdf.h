/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

gboolean ephy_cdf_is_file   (GFile        *file);
char    *ephy_cdf_load_file (GFile        *file,
                             GError      **error);
char    *ephy_cdf_to_html   (const char   *data,
                             gsize         length,
                             GError      **error);

G_END_DECLS
