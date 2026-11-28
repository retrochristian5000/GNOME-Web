/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Returns an allocated password (free with g_free), or NULL on failure. */
char *ephy_generate_secure_password (void);

G_END_DECLS
