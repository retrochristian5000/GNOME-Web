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

#include "ephy-settings.h"


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
  g_variant_builder_add  (&result, "{sv}", "name", g_variant_new_string (container_name));
  /* color enum */
  g_variant_builder_add  (&result, "{sv}", "color", g_variant_new_uint16 (0));
  
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

gboolean
try_create_container (const char *container_name,
                      const char *color)
{
  g_autofree gchar *UUID = g_uuid_string_random();
	create_gsetting (UUID, container_name, color);
	return TRUE;
}

