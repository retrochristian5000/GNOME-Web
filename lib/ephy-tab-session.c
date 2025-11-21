/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2025 Hyland B.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-tab-session.h"

#include <webkit/webkit.h>
#include <glib.h>
#include "ephy-debug.h"
#include "ephy-file-helpers.h"

static WebKitNetworkSession *session_singleton;
static GHashTable *container_sessions;

struct _EphyTabSession {
  GObject parent_instance;

  WebKitNetworkSession *network_session;
  char *uuid;
  char *profile_dir;
  char *cache_dir;
};

G_DEFINE_FINAL_TYPE (EphyTabSession, ephy_tab_session, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_UUID,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

unsigned int
ephy_tab_session_get_running_sessions_count (void)
{
  if (container_sessions)
    return g_hash_table_size (container_sessions);
  else
    return 0;
}

void
ephy_tab_session_set_network_session_singleton (WebKitNetworkSession *object)
{
  g_assert (!session_singleton);

  session_singleton = object;
  LOG ("SET Network session singleton was set: %p", session_singleton);
}

WebKitNetworkSession *
ephy_tab_session_get_network_session (EphyTabSession *self)
{
  if (!self->uuid)
    return session_singleton;

  return NULL;
}

const char *
ephy_tab_session_get_cache_dir (EphyTabSession *self)
{
  if (self->uuid) {
    if (!self->cache_dir) {
      self->cache_dir = g_strdup_printf ("%s/%s", ephy_cache_dir (), self->uuid);
    }
    return self->cache_dir;
  }

  return ephy_cache_dir ();
}

const char *
ephy_tab_session_get_container_uuid (EphyTabSession *self)
{
  return self->uuid;
}

const char *
ephy_tab_session_get_profile_dir (EphyTabSession *self)
{
  if (self->uuid) {
    if (!self->profile_dir) {
      self->profile_dir = g_strdup_printf ("%s/containers/%s", ephy_profile_dir (), self->uuid);
    }
    return self->profile_dir;
  }

  return ephy_profile_dir ();
}

static void
ephy_tab_session_finalize (GObject *object)
{
  EphyTabSession *self = EPHY_TAB_SESSION (object);
  size_t size;

  if (!self->uuid) {
    LOG ("GOODBYE TabSession (singleton): %p", session_singleton);
    g_assert (session_singleton);
    return;
  }

  LOG ("GOODBYE TabSession (UUID:%s): %p", self->uuid, session_singleton);
  g_hash_table_remove (container_sessions, self->uuid);
  g_free (self->uuid);

  /* Glib.HashTable.unref doesn't have a way to tell if the refcount
   * hits zero.  It just... invalidates the memory. So we'll check
   * beforehand if it's empty (usually indicating this is the last
   * instance, anyway) */
  size = g_hash_table_size (container_sessions);
  /* XXX: This logic is broken anyway. Containers with the same hash do not work. What an oversight :| */
  g_hash_table_unref (container_sessions);
  if (size == 0) {
    LOG ("container_sessions was presumably destroyed!");
    container_sessions = NULL;
  }
}

static void
ephy_tab_session_constructed (GObject *object)
{
  EphyTabSession *self = EPHY_TAB_SESSION (object);
  WebKitNetworkSession *network_session;

  /* If no container UUID is provided, this is the singleton */
  if (!self->uuid) {
    LOG ("HELLO TabSession (singleton): %p (%s)", session_singleton, ephy_tab_session_get_profile_dir (self));
    g_assert (session_singleton);
    return;
  }

  /* otherwise, this is a container */
  if (!container_sessions)
    container_sessions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
  else
    g_hash_table_ref (container_sessions);

  network_session = webkit_network_session_new (ephy_tab_session_get_profile_dir (self),
                                                ephy_tab_session_get_cache_dir (self));
  g_hash_table_insert (container_sessions, self->uuid, network_session);
  /* To avoid hash lookup */
  self->network_session = network_session;
  LOG ("HELLO TabSession (UUID:%s): %p (%s)", self->uuid, self->network_session, ephy_tab_session_get_profile_dir (self));
}

static void
ephy_tab_session_set_uuid (EphyTabSession *self,
                           const char     *uuid)
{
  char *uuid_dup = g_strdup (uuid);
  self->uuid = uuid_dup;
}

/* #ifndef G_DISABLE_ASSERT */
/* G_GNUC_NORETURN */
/* #endif */
static void
ephy_tab_session_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyTabSession *tab_session = EPHY_TAB_SESSION (object);

  /* TODO: No */
  g_value_set_string (value, tab_session->uuid);
}

static void
ephy_tab_session_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyTabSession *tab_session = EPHY_TAB_SESSION (object);

  switch (prop_id) {
    case PROP_UUID:
      ephy_tab_session_set_uuid (tab_session, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_tab_session_class_init (EphyTabSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_tab_session_constructed;
  object_class->finalize = ephy_tab_session_finalize;
  object_class->get_property = ephy_tab_session_get_property;
  object_class->set_property = ephy_tab_session_set_property;

  obj_properties[PROP_UUID] =
    g_param_spec_string ("uuid",
                         NULL, NULL, NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     obj_properties);
}

static void
ephy_tab_session_init (EphyTabSession *self)
{
}

EphyTabSession *
ephy_tab_session_new_from_container (const char *uuid)
{
  EphyTabSession *tab_session;

  tab_session = g_object_new (EPHY_TYPE_TAB_SESSION,
                              "uuid", uuid,
                              NULL);

  return tab_session;
}


EphyTabSession *
ephy_tab_session_new (void)
{
  EphyTabSession *tab_session;

  tab_session = g_object_new (EPHY_TYPE_TAB_SESSION, NULL);

  return tab_session;
}
