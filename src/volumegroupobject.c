/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
 * Copyright (C) 2013 Marius Vollmer <marius.vollmer@gmail.com>
 * Copyright (C) 2013 Red Hat Inc <stefw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "daemon.h"
#include "blockobject.h"
#include "logicalvolumeobject.h"
#include "util.h"
#include "volumegroup.h"
#include "volumegroupobject.h"

#include <glib/gi18n-lib.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * SECTION:udiskslinuxvolumegroupobject
 * @title: UlVolumeGroupObject
 * @short_description: Object representing a LVM volume group
 */

typedef struct _UlVolumeGroupObjectClass UlVolumeGroupObjectClass;

/**
 * UlVolumeGroupObject:
 *
 * The #UlVolumeGroupObject structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _UlVolumeGroupObject
{
  LvmObjectSkeleton parent_instance;

  gchar *name;

  GHashTable *logical_volumes;
  GPid poll_pid;
  guint poll_timeout_id;
  gboolean poll_requested;

  /* interface */
  LvmVolumeGroup *iface_volume_group;
};

struct _UlVolumeGroupObjectClass
{
  LvmObjectSkeletonClass parent_class;
};

enum
{
  PROP_0,
  PROP_NAME,
};

G_DEFINE_TYPE (UlVolumeGroupObject, ul_volume_group_object, LVM_TYPE_OBJECT_SKELETON);

static void
ul_volume_group_object_finalize (GObject *obj)
{
  UlVolumeGroupObject *self = UL_VOLUME_GROUP_OBJECT (obj);

  if (self->iface_volume_group != NULL)
    g_object_unref (self->iface_volume_group);

  g_hash_table_unref (self->logical_volumes);
  g_free (self->name);

  G_OBJECT_CLASS (ul_volume_group_object_parent_class)->finalize (obj);
}

static void
ul_volume_group_object_get_property (GObject *obj,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
  UlVolumeGroupObject *self = UL_VOLUME_GROUP_OBJECT (obj);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ul_volume_group_object_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
ul_volume_group_object_set_property (GObject *obj,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
  UlVolumeGroupObject *self = UL_VOLUME_GROUP_OBJECT (obj);

  switch (prop_id)
    {
    case PROP_NAME:
      g_assert (self->name == NULL);
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}


static void
ul_volume_group_object_init (UlVolumeGroupObject *self)
{

}

static void
ul_volume_group_object_constructed (GObject *obj)
{
  UlVolumeGroupObject *self = UL_VOLUME_GROUP_OBJECT (obj);
  GString *s;

  G_OBJECT_CLASS (ul_volume_group_object_parent_class)->constructed (obj);

  self->logical_volumes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify) g_object_unref);

  /* Compute the object path */
  s = g_string_new ("/org/freedesktop/UDisks2/lvm/");
  ul_util_safe_append_to_object_path (s, self->name);
  g_dbus_object_skeleton_set_object_path (G_DBUS_OBJECT_SKELETON (self), s->str);
  g_string_free (s, TRUE);

  /* Create the DBus interface */
  self->iface_volume_group = ul_volume_group_new ();
  g_dbus_object_skeleton_add_interface (G_DBUS_OBJECT_SKELETON (self),
                                        G_DBUS_INTERFACE_SKELETON (self->iface_volume_group));
}

static void
ul_volume_group_object_class_init (UlVolumeGroupObjectClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ul_volume_group_object_finalize;
  gobject_class->constructed = ul_volume_group_object_constructed;
  gobject_class->set_property = ul_volume_group_object_set_property;
  gobject_class->get_property = ul_volume_group_object_get_property;

  /**
   * UlVolumeGroupObject:name:
   *
   * The name of the volume group.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "The name of the volume group",
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

/**
 * ul_volume_group_object_new:
 * @name: The name of the volume group.
 *
 * Create a new VolumeGroup object.
 *
 * Returns: A #UlVolumeGroupObject object. Free with g_object_unref().
 */
UlVolumeGroupObject *
ul_volume_group_object_new (const gchar   *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (UL_TYPE_VOLUME_GROUP_OBJECT,
                       "name", name,
                       NULL);
}

static gboolean
lv_is_pvmove_volume (const gchar *name)
{
  return name && g_str_has_prefix (name, "pvmove");
}

static gboolean
lv_is_visible (const gchar *name)
{
  // XXX - get this from lvm2app

  return (name
          && strstr (name, "_mlog") == NULL
          && strstr (name, "_mimage") == NULL
          && strstr (name, "_rimage") == NULL
          && strstr (name, "_rmeta") == NULL
          && strstr (name, "_tdata") == NULL
          && strstr (name, "_tmeta") == NULL
          && !g_str_has_prefix (name, "pvmove")
          && !g_str_has_prefix (name, "snapshot"));
}

static void
update_progress_for_device (const gchar *operation,
                            const gchar *dev,
                            double progress)
{
  GDBusObjectManager *object_manager;
  UlDaemon *daemon;
  GList *objects, *l;

  daemon = ul_daemon_get ();
  object_manager = G_DBUS_OBJECT_MANAGER (ul_daemon_get_object_manager (daemon));
  objects = g_dbus_object_manager_get_objects (object_manager);

  for (l = objects; l; l = l->next)
    {
      UDisksJob *job = NULL;
      const gchar *const *job_objects;
      int i;

      if (UDISKS_IS_OBJECT (l->data))
        job = udisks_object_peek_job (l->data);
      if (job == NULL)
        continue;

      if (g_strcmp0 (udisks_job_get_operation (job), operation) != 0)
        continue;

      job_objects = udisks_job_get_objects (job);
      for (i = 0; job_objects[i]; i++)
        {
          GDBusObject *object;
          gboolean found = FALSE;

          object = ul_daemon_find_object (daemon, job_objects[i]);
          if (object && UL_IS_BLOCK_OBJECT (object))
            {
              if (g_strcmp0 (ul_block_object_get_device (UL_BLOCK_OBJECT (object)), dev) == 0)
                {
                  found = TRUE;
                }
              else
                {
                  const gchar **symlinks;
                  int j;

                  symlinks = ul_block_object_get_symlinks (UL_BLOCK_OBJECT (object));
                  for (j = 0; symlinks[j]; j++)
                    {
                      if (g_strcmp0 (symlinks[j], dev) == 0)
                        {
                          found = TRUE;
                          break;
                        }
                    }
                }
            }

          if (found)
            {
              udisks_job_set_progress (job, progress);
              udisks_job_set_progress_valid (job, TRUE);
            }
        }
    }

  g_list_free_full (objects, g_object_unref);
}

static void
update_operations (const gchar *lv_name,
                   GVariant *lv_info,
                   gboolean *needs_polling_ret)
{
  const gchar *move_pv;
  guint64 copy_percent;

  if (lv_is_pvmove_volume (lv_name)
      && g_variant_lookup (lv_info, "move_pv", "&s", &move_pv)
      && g_variant_lookup (lv_info, "copy_percent", "t", &copy_percent))
    {
      update_progress_for_device ("lvm-vg-empty-device",
                                  move_pv,
                                  copy_percent/100000000.0);
      *needs_polling_ret = TRUE;
    }
}


static void
update_block (UlVolumeGroupObject *self,
              UlBlockObject *block,
              GHashTable *new_lvs,
              GHashTable *new_pvs)
{
  GVariant *pv_info;

  // XXX - move this elsewhere?
  {
    GUdevDevice *device;
    UlLogicalVolumeObject *lv_object;
    const gchar *block_vg_name;
    const gchar *block_lv_name;

    device = ul_block_object_get_udev (block);
    if (device)
      {
        block_vg_name = g_udev_device_get_property (device, "DM_VG_NAME");
        block_lv_name = g_udev_device_get_property (device, "DM_LV_NAME");

        if (g_strcmp0 (block_vg_name, ul_volume_group_object_get_name (self)) == 0
            && (lv_object = g_hash_table_lookup (new_lvs, block_lv_name)))
          {
            ul_block_object_update_lv (block, lv_object);
          }
        g_object_unref (device);
      }
  }

  pv_info = g_hash_table_lookup (new_pvs, ul_block_object_get_device (block));
  if (!pv_info)
    {
      const gchar *const *symlinks;
      int i;
      symlinks = ul_block_object_get_symlinks (block);
      for (i = 0; symlinks[i]; i++)
        {
          pv_info = g_hash_table_lookup (new_pvs, symlinks[i]);
          if (pv_info)
            break;
        }
    }

  if (pv_info)
    {
      ul_block_object_update_pv (block, self, pv_info);
    }
  else
    {
      LvmPhysicalVolumeBlock *pv = lvm_object_peek_physical_volume_block (LVM_OBJECT (block));
      if (pv && g_strcmp0 (lvm_physical_volume_block_get_volume_group (pv),
                           g_dbus_object_get_object_path (G_DBUS_OBJECT (self))) == 0)
        ul_block_object_update_pv (block, NULL, NULL);
    }
}

static void
update_with_variant (GPid pid,
                     GVariant *info,
                     GError *error,
                     gpointer user_data)
{
  UlVolumeGroupObject *self = user_data;
  UlDaemon *daemon;
  GDBusObjectManagerServer *manager;
  GVariantIter *iter;
  GHashTableIter volume_iter;
  gpointer key, value;
  GHashTable *new_lvs;
  GHashTable *new_pvs;
  GList *objects, *l;
  gboolean needs_polling = FALSE;

  daemon = ul_daemon_get ();
  manager = ul_daemon_get_object_manager (daemon);

  if (error)
    {
      g_message ("Failed to update LVM volume group %s: %s",
                 ul_volume_group_object_get_name (self),
                 error->message);
      g_object_unref (self);
      return;
    }

  ul_volume_group_update (UL_VOLUME_GROUP (self->iface_volume_group), info, &needs_polling);

  if (!g_dbus_object_manager_server_is_exported (manager, G_DBUS_OBJECT_SKELETON (self)))
    g_dbus_object_manager_server_export_uniquely (manager, G_DBUS_OBJECT_SKELETON (self));

  new_lvs = g_hash_table_new (g_str_hash, g_str_equal);

  if (g_variant_lookup (info, "lvs", "aa{sv}", &iter))
    {
      GVariant *lv_info = NULL;
      while (g_variant_iter_loop (iter, "@a{sv}", &lv_info))
        {
          const gchar *name;
          UlLogicalVolumeObject *volume;

          g_variant_lookup (lv_info, "name", "&s", &name);

          update_operations (name, lv_info, &needs_polling);

          if (lv_is_pvmove_volume (name))
            needs_polling = TRUE;

          if (!lv_is_visible (name))
            continue;

          volume = g_hash_table_lookup (self->logical_volumes, name);
          if (volume == NULL)
            {
              volume = ul_logical_volume_object_new (self, name);
              ul_logical_volume_object_update (volume, lv_info, &needs_polling);
              g_dbus_object_manager_server_export_uniquely (manager, G_DBUS_OBJECT_SKELETON (volume));
              g_hash_table_insert (self->logical_volumes, g_strdup (name), g_object_ref (volume));
            }
          else
            ul_logical_volume_object_update (volume, lv_info, &needs_polling);

          g_hash_table_insert (new_lvs, (gchar *)name, volume);
        }
      g_variant_iter_free (iter);
    }

  g_hash_table_iter_init (&volume_iter, self->logical_volumes);
  while (g_hash_table_iter_next (&volume_iter, &key, &value))
    {
      const gchar *name = key;
      UlLogicalVolumeObject *volume = value;

      if (!g_hash_table_contains (new_lvs, name))
        {
          g_dbus_object_manager_server_unexport (manager,
                                                 g_dbus_object_get_object_path (G_DBUS_OBJECT (volume)));
          g_hash_table_iter_remove (&volume_iter);
        }
    }

  lvm_volume_group_set_needs_polling (self->iface_volume_group, needs_polling);

  /* Update block objects. */

  new_pvs = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)g_variant_unref);
  if (g_variant_lookup (info, "pvs", "aa{sv}", &iter))
    {
      const gchar *name;
      GVariant *pv_info;
      while (g_variant_iter_next (iter, "@a{sv}", &pv_info))
        {
          if (g_variant_lookup (pv_info, "device", "&s", &name))
            g_hash_table_insert (new_pvs, (gchar *)name, pv_info);
          else
            g_variant_unref (pv_info);
        }
    }

  objects = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (manager));
  for (l = objects; l != NULL; l = l->next)
    {
      if (UL_IS_BLOCK_OBJECT (l->data))
        update_block (self, l->data, new_lvs, new_pvs);
    }
  g_list_free_full (objects, g_object_unref);

  g_hash_table_destroy (new_lvs);
  g_hash_table_destroy (new_pvs);

  g_object_unref (self);
}

void
ul_volume_group_object_update (UlVolumeGroupObject *self)
{
  const gchar *args[] = {
      "udisks-lvm-helper", "-b",
      "show", self->name, NULL
  };

  ul_daemon_spawn_for_variant (ul_daemon_get (), args, G_VARIANT_TYPE ("a{sv}"),
                               update_with_variant, g_object_ref (self));
}

static void
poll_with_variant (GPid pid,
                   GVariant *info,
                   GError *error,
                   gpointer user_data)
{
  UlVolumeGroupObject *self = user_data;
  GVariantIter *iter;
  gboolean needs_polling;

  if (pid != self->poll_pid)
    {
      g_object_unref (self);
      return;
    }

  self->poll_pid = 0;

  if (error)
    {
      g_message ("Failed to poll LVM volume group %s: %s",
                 ul_volume_group_object_get_name (self),
                 error->message);
      g_object_unref (self);
      return;
    }

  ul_volume_group_update (UL_VOLUME_GROUP (self->iface_volume_group), info, &needs_polling);

  if (g_variant_lookup (info, "lvs", "aa{sv}", &iter))
    {
      GVariant *lv_info = NULL;
      while (g_variant_iter_loop (iter, "@a{sv}", &lv_info))
        {
          const gchar *name;
          UlLogicalVolumeObject *volume;

          g_variant_lookup (lv_info, "name", "&s", &name);
          update_operations (name, lv_info, &needs_polling);
          volume = g_hash_table_lookup (self->logical_volumes, name);
          if (volume)
            ul_logical_volume_object_update (volume, lv_info, &needs_polling);
        }
      g_variant_iter_free (iter);
    }

  g_object_unref (self);
}

static void poll_now (UlVolumeGroupObject *self);

static gboolean
poll_in_main_thread (gpointer user_data)
{
  UlVolumeGroupObject *self = user_data;

  if (self->poll_timeout_id)
    self->poll_requested = TRUE;
  else
    poll_now (self);

  g_object_unref (self);
  return FALSE;
}

static gboolean
poll_timeout (gpointer user_data)
{
  UlVolumeGroupObject *self = user_data;

  self->poll_timeout_id = 0;
  if (self->poll_requested)
    {
      self->poll_requested = FALSE;
      poll_now (self);
    }

  g_object_unref (self);
  return FALSE;
}

static void
poll_now (UlVolumeGroupObject *self)
{
  const gchar *args[] = {
      "udisks-lvm-helper",
      "-b", "show", self->name, NULL
  };

  self->poll_timeout_id = g_timeout_add (5000, poll_timeout, g_object_ref (self));

  if (self->poll_pid)
    kill (self->poll_pid, SIGINT);

  self->poll_pid = ul_daemon_spawn_for_variant (ul_daemon_get (), args, G_VARIANT_TYPE ("a{sv}"),
                                                poll_with_variant, g_object_ref (self));
}

void
ul_volume_group_object_poll (UlVolumeGroupObject *self)
{
  g_idle_add (poll_in_main_thread, g_object_ref (self));
}

void
ul_volume_group_object_destroy (UlVolumeGroupObject *self)
{
  GHashTableIter volume_iter;
  gpointer key, value;

  g_hash_table_iter_init (&volume_iter, self->logical_volumes);
  while (g_hash_table_iter_next (&volume_iter, &key, &value))
    {
      UlLogicalVolumeObject *volume = value;
      g_dbus_object_manager_server_unexport (ul_daemon_get_object_manager (ul_daemon_get ()),
                                             g_dbus_object_get_object_path (G_DBUS_OBJECT (volume)));
    }
}

UlLogicalVolumeObject *
ul_volume_group_object_find_logical_volume_object (UlVolumeGroupObject *self,
                                                   const gchar *name)
{
  return g_hash_table_lookup (self->logical_volumes, name);
}

/**
 * ul_volume_group_object_get_name:
 * @self: A #UlVolumeGroupObject.
 *
 * Gets the name for @object.
 *
 * Returns: (transfer none): The name for object. Do not free, the
 *          string belongs to object.
 */
const gchar *
ul_volume_group_object_get_name (UlVolumeGroupObject *self)
{
  g_return_val_if_fail (UL_IS_VOLUME_GROUP_OBJECT (self), NULL);
  return self->name;
}
