/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <webkit/webkit.h>

G_BEGIN_DECLS

void ephy_ruffle_web_register (WebKitWebContext *context);
gboolean ephy_ruffle_web_available (WebKitWebContext *context);

G_END_DECLS
