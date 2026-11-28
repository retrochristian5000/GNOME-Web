/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

gboolean ephy_ruffle_is_swf    (GFile   *file);
gboolean ephy_ruffle_open_file (GFile   *file,
                                GError **error);

G_END_DECLS
