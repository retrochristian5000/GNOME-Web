/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ephy-containers.h"

#include "ephy-debug.h"
#include "ephy-pixbuf-utils.h"
#include "ephy-settings.h"

/* TODO: Free these */
static GIcon *color_icons[ADW_ACCENT_COLOR_SLATE + 1];

static gboolean
create_gsetting (const char *UUID,
                 const char *container_name,
                 const char *color)
{
  g_autoptr (GVariantIter) iter = NULL;
  const gchar *name;
  GVariant *variant;
  GVariantBuilder builder, result;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sa{sv}}"));
  g_variant_builder_init (&result, G_VARIANT_TYPE ("a{sv}"));
  /* name */
  g_variant_builder_add (&result, "{sv}", "name", g_variant_new_string (container_name));
  /* color enum */
  g_variant_builder_add (&result, "{sv}", "color", g_variant_new_uint16 (0));

  g_settings_get (EPHY_SETTINGS_CONTAINERS, EPHY_PREFS_CONTAINERS_CONTAINER_MAP, "a{sa{sv}}", &iter);

  while (g_variant_iter_next (iter, "{s@a{sv}}", &name, &variant)) {
    g_variant_builder_add (&builder, "{s@a{sv}}", name, g_variant_ref (variant));
  }

  g_variant_builder_add (&builder, "{s@a{sv}}", UUID, g_variant_builder_end (&result));

  return g_settings_set_value (EPHY_SETTINGS_CONTAINERS,
                               EPHY_PREFS_CONTAINERS_CONTAINER_MAP,
                               g_variant_builder_end (&builder));
}

GPtrArray *
get_container_uuids ()
{
  g_autoptr (GVariantIter) iter = NULL;
  const gchar *name;
  g_autoptr (GPtrArray) names = NULL;
  GVariant *variant;

  g_settings_get (EPHY_SETTINGS_CONTAINERS, EPHY_PREFS_CONTAINERS_CONTAINER_MAP, "a{sa{sv}}", &iter);

  names = g_ptr_array_new_with_free_func (g_free);

  while (g_variant_iter_next (iter, "{s@a{sv}}", &name, &variant)) {
    g_ptr_array_add (names, g_strdup (name));

    g_variant_unref (variant);
  }

  g_ptr_array_ref (names);
  return names;
}

GPtrArray *
get_container_names ()
{
  g_autoptr (GVariantIter) iter = NULL;
  const gchar *name, *container_name;
  g_autoptr (GPtrArray) names = NULL;
  GVariant *variant;

  g_settings_get (EPHY_SETTINGS_CONTAINERS, EPHY_PREFS_CONTAINERS_CONTAINER_MAP, "a{sa{sv}}", &iter);

  names = g_ptr_array_new_with_free_func (g_free);

  while (g_variant_iter_next (iter, "{s@a{sv}}", &name, &variant)) {
    container_name = NULL;
    g_variant_lookup (variant, "name", "&s", &container_name);

    if (container_name)
      g_ptr_array_add (names, g_strdup (container_name));
    else
      g_warning ("No container name for UUID %s", name);

    g_variant_unref (variant);
  }

  g_ptr_array_ref (names);
  return names;
}

AdwAccentColor
get_container_color_by_uuid (const char *container_uuid)
{
  g_autoptr (GVariantIter) iter = NULL;
  const gchar *name;
  uint16_t color = 0;
  GVariant *variant;

  g_settings_get (EPHY_SETTINGS_CONTAINERS, EPHY_PREFS_CONTAINERS_CONTAINER_MAP, "a{sa{sv}}", &iter);

  while (g_variant_iter_next (iter, "{s@a{sv}}", &name, &variant)) {
    if (g_strcmp0 (container_uuid, name) == 0) {
      g_variant_lookup (variant, "color", "q", &color);
      return color;
    }
    g_variant_unref (variant);
  }

  g_warning ("Accent color not set for container %s, setting to blue", container_uuid);

  return ADW_ACCENT_COLOR_BLUE;
}

GIcon *
get_container_color_icon_by_uuid (const char *container_uuid)
{
  AdwAccentColor color = get_container_color_by_uuid (container_uuid);
  GdkRGBA rgba;

  g_assert (color >= 0 && color <= ADW_ACCENT_COLOR_SLATE);

  adw_accent_color_to_rgba (color, &rgba);

  g_warning ("Container color: %d (%f, %f, %f)", color, rgba.red, rgba.green, rgba.blue);
  if (!color_icons[color])
    color_icons[color] = G_ICON (ephy_pixbuf_colored_icon_pixbuf (rgba));

  return color_icons[color];
}

gboolean
try_create_container (const char *container_name,
                      const char *color)
{
  g_autofree gchar *UUID = g_uuid_string_random ();
  create_gsetting (UUID, container_name, color);
  return TRUE;
}
