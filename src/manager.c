/*
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
 * Copyright (C) 2013 Red Hat Inc.
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
 */

#include "config.h"

#include "manager.h"

#include "blockobject.h"
#include "daemon.h"
#include "util.h"
#include "volumegroupobject.h"

#include <gudev/gudev.h>
#include <glib/gi18n.h>

struct _UlManager
{
  LvmManagerSkeleton parent;

  UDisksClient *udisks_client;
  GUdevClient *gudev_client;

  /* maps from volume group name to UlVolumeGroupObject
     instances.
  */
  GHashTable *name_to_volume_group;
  GHashTable *real_block_to_lvm_block;

  gint lvm_delayed_update_id;
};

typedef struct
{
  LvmManagerSkeletonClass parent;
} UlManagerClass;

static void lvm_manager_iface_init (LvmManagerIface *iface);

G_DEFINE_TYPE_WITH_CODE (UlManager, ul_manager, LVM_TYPE_MANAGER_SKELETON,
                         G_IMPLEMENT_INTERFACE (LVM_TYPE_MANAGER, lvm_manager_iface_init);
);

static void
lvm_update_from_variant (GPid pid,
                         GVariant *volume_groups,
                         GError *error,
                         gpointer user_data)
{
  UlManager *self = user_data;
  GDBusObjectManagerServer *manager;
  GVariantIter var_iter;
  GHashTableIter vg_name_iter;
  gpointer key, value;
  const gchar *name;

  manager = ul_daemon_get_object_manager (ul_daemon_get ());

  // Remove obsolete groups
  g_hash_table_iter_init (&vg_name_iter, self->name_to_volume_group);
  while (g_hash_table_iter_next (&vg_name_iter, &key, &value))
    {
      const gchar *vg;
      UlVolumeGroupObject *group;
      gboolean found = FALSE;

      name = key;
      group = value;

      g_variant_iter_init (&var_iter, volume_groups);
      while (g_variant_iter_next (&var_iter, "&s", &vg))
        {
          if (g_strcmp0 (vg, name) == 0)
            {
              found = TRUE;
              break;
            }
        }

      if (!found)
        {
          ul_volume_group_object_destroy (group);
          g_dbus_object_manager_server_unexport (manager,
                                                 g_dbus_object_get_object_path (G_DBUS_OBJECT (group)));
          g_hash_table_iter_remove (&vg_name_iter);
        }
    }

  /* Add new groups and update existing groups */
  g_variant_iter_init (&var_iter, volume_groups);
  while (g_variant_iter_next (&var_iter, "&s", &name))
    {
      UlVolumeGroupObject *group;
      group = g_hash_table_lookup (self->name_to_volume_group, name);

      if (group == NULL)
        {
          group = ul_volume_group_object_new (name);
          g_hash_table_insert (self->name_to_volume_group, g_strdup (name), group);
        }
      ul_volume_group_object_update (group);
    }
}

static void
lvm_update (UlManager *self)
{
  const gchar *args[] = { "/usr/lib/udisks2/udisks-lvm", "-b", "list", NULL };
  ul_util_spawn_for_variant (args, G_VARIANT_TYPE("as"), lvm_update_from_variant, self);
}

static gboolean
delayed_lvm_update (gpointer user_data)
{
  UlManager *self = UL_MANAGER (user_data);

  lvm_update (self);
  self->lvm_delayed_update_id = 0;

  return FALSE;
}

static void
trigger_delayed_lvm_update (UlManager *self)
{
  if (self->lvm_delayed_update_id > 0)
    return;

  self->lvm_delayed_update_id =
    g_timeout_add (100, delayed_lvm_update, self);
}

static void
do_delayed_lvm_update_now (UlManager *self)
{
  if (self->lvm_delayed_update_id > 0)
    {
      g_source_remove (self->lvm_delayed_update_id);
      self->lvm_delayed_update_id = 0;
      lvm_update (self);
    }
}

static gboolean
is_logical_volume (GUdevDevice *device)
{
  const gchar *dm_vg_name = g_udev_device_get_property (device, "DM_VG_NAME");
  return dm_vg_name && *dm_vg_name;
}

static gboolean
has_physical_volume_label (GUdevDevice *device)
{
  const gchar *id_fs_type = g_udev_device_get_property (device, "ID_FS_TYPE");
  return g_strcmp0 (id_fs_type, "LVM2_member") == 0;
}

static LvmObject *
find_block (UlManager *self,
            dev_t device_number)
{
  LvmObject *our_block = NULL;
  UDisksBlock *real_block;
  GDBusObject *object;
  const gchar *path;

  real_block = udisks_client_get_block_for_dev (self->udisks_client, device_number);
  if (real_block != NULL)
    {
      object = g_dbus_interface_get_object (G_DBUS_INTERFACE (real_block));
      path = g_dbus_object_get_object_path (object);

      our_block = LVM_OBJECT (ul_daemon_find_object (ul_daemon_get (), path));
      g_object_unref (real_block);
    }

  return our_block;
}

static gboolean
is_recorded_as_physical_volume (UlManager *self,
                                GUdevDevice *device)
{
  LvmObject *object;
  gboolean ret = FALSE;

  object = find_block (self, g_udev_device_get_device_number (device));
  if (object != NULL)
    {
      ret = (lvm_object_peek_physical_volume_block (object) != NULL);
      g_object_unref (object);
    }

  return ret;
}

static void
handle_block_uevent_for_lvm (UlManager *self,
                             const gchar *action,
                             GUdevDevice *device)
{
  if (is_logical_volume (device)
      || has_physical_volume_label (device)
      || is_recorded_as_physical_volume (self, device))
    trigger_delayed_lvm_update (self);
}

static void
on_uevent (GUdevClient *client,
           const gchar *action,
           GUdevDevice *device,
           gpointer user_data)
{
  handle_block_uevent_for_lvm (user_data, action, device);
}

static void
ul_manager_init (UlManager *self)
{
  const gchar *subsystems[] = {"block", "iscsi_connection", "scsi", NULL};

  /* get ourselves an udev client */
  self->gudev_client = g_udev_client_new (subsystems);

  g_signal_connect (self->gudev_client, "uevent", G_CALLBACK (on_uevent), self);
}

static void
ul_manager_constructed (GObject *object)
{
  UlManager *self = UL_MANAGER (object);

  self->name_to_volume_group = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                      (GDestroyNotify) g_object_unref);

  do_delayed_lvm_update_now (self);

  G_OBJECT_CLASS (ul_manager_parent_class)->constructed (object);
}

static void
ul_manager_finalize (GObject *object)
{
  UlManager *self = UL_MANAGER (object);

  g_object_unref (self->gudev_client);
  g_hash_table_unref (self->name_to_volume_group);

  G_OBJECT_CLASS (ul_manager_parent_class)->finalize (object);
}

static void
ul_manager_class_init (UlManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ul_manager_constructed;
  object_class->finalize = ul_manager_finalize;
}

typedef struct {
  UlManager *manager;
  const gchar *name;
} WaitVolumeGroup;

UlVolumeGroupObject *
ul_manager_find_volume_group_object (UlManager *self,
                                     const gchar *name)
{
  return g_hash_table_lookup (self->name_to_volume_group, name);
}

static GDBusObject *
wait_for_volume_group_object (UlDaemon *daemon,
                              gpointer user_data)
{
  WaitVolumeGroup *waitvg = user_data;
  return G_DBUS_OBJECT (ul_manager_find_volume_group_object (waitvg->manager, waitvg->name));
}

static gboolean
handle_volume_group_create (LvmManager *manager,
                            GDBusMethodInvocation *invocation,
                            const gchar *const *arg_blocks,
                            const gchar *arg_name,
                            guint64 arg_extent_size,
                            GVariant *arg_options)
{
  UlManager *self = UL_MANAGER (manager);
  uid_t caller_uid;
  GError *error = NULL;
  const gchar *message;
  const gchar *action_id;
  GList *blocks = NULL;
  GList *l;
  guint n;
  gchar *encoded_name = NULL;
  gchar *escaped_name = NULL;
  GString *str = NULL;
  gint status;
  gchar *error_message = NULL;
  GDBusObject *group_object = NULL;
  WaitVolumeGroup waitvg;

  error = NULL;
  if (!ul_daemon_get_caller_uid_sync (ul_daemon_get (), invocation,
                                      NULL /* GCancellable */,
                                      &caller_uid, NULL, NULL, &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_clear_error (&error);
      goto out;
    }

  message = N_("Authentication is required to create a volume group");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (ul_daemon_get (),
                                           NULL,
                                           action_id,
                                           arg_options,
                                           message,
                                           invocation))
    goto out;

  /* Collect and validate block objects
   *
   * Also, check we can open the block devices at the same time - this
   * is to avoid start deleting half the block devices while the other
   * half is already in use.
   */
  for (n = 0; arg_blocks != NULL && arg_blocks[n] != NULL; n++)
    {
      GDBusObject *object = NULL;

      object = ul_daemon_find_object (ul_daemon_get (), arg_blocks[n]);

      /* Assumes ref, do this early for memory management */
      blocks = g_list_prepend (blocks, object);

      if (object == NULL)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 UDISKS_ERROR,
                                                 UDISKS_ERROR_FAILED,
                                                 "Invalid object path %s at index %d",
                                                 arg_blocks[n], n);
          goto out;
        }

      if (!UL_IS_BLOCK_OBJECT (object))
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 UDISKS_ERROR,
                                                 UDISKS_ERROR_FAILED,
                                                 "Object path %s for index %d is not a block device",
                                                 arg_blocks[n], n);
          goto out;
        }

      if (!ul_block_object_is_unused (UL_BLOCK_OBJECT (object), &error))
        {
          g_dbus_method_invocation_take_error (invocation, error);
          goto out;
        }
    }

  blocks = g_list_reverse (blocks);

  /* wipe existing devices */
  for (l = blocks; l != NULL; l = l->next)
    {
      if (!ul_block_object_wipe (l->data, &error))
        {
          g_dbus_method_invocation_take_error (invocation, error);
          goto out;
        }
    }

  /* Create the volume group... */
  encoded_name = ul_util_encode_lvm_name (arg_name, FALSE);
  escaped_name = ul_util_escape_and_quote (encoded_name);
  str = g_string_new ("vgcreate");
  g_string_append_printf (str, " %s", escaped_name);
  if (arg_extent_size > 0)
    g_string_append_printf (str, " -s %" G_GUINT64_FORMAT "b", arg_extent_size);
  for (l = blocks; l != NULL; l = l->next)
    {
      UlBlockObject *block = l->data;
      gchar *escaped_device;
      escaped_device = ul_util_escape_and_quote (ul_block_object_get_device (block));
      g_string_append_printf (str, " %s", escaped_device);
      g_free (escaped_device);
    }

  if (!ul_daemon_launch_spawned_job_sync (ul_daemon_get (),
                                          NULL,
                                          "lvm-vg-create", caller_uid,
                                          NULL, /* cancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          &status,
                                          &error_message,
                                          NULL, /* input_string */
                                          "%s",
                                          str->str))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error creating volume group: %s",
                                             error_message);
      g_free (error_message);
      goto out;
    }

  for (l = blocks; l != NULL; l = l->next)
    ul_block_object_trigger_uevent (l->data);

  waitvg.manager = self;
  waitvg.name = encoded_name;

  /* ... then, sit and wait for the object to show up */
  group_object = ul_daemon_wait_for_object_sync (ul_daemon_get (),
                                                 wait_for_volume_group_object,
                                                 &waitvg,
                                                 NULL,
                                                 10, /* timeout_seconds */
                                                 &error);
  if (group_object == NULL)
    {
      g_prefix_error (&error,
                      "Error waiting for volume group object for %s",
                      arg_name);
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_manager_complete_volume_group_create (manager, invocation,
                                            g_dbus_object_get_object_path (G_DBUS_OBJECT (group_object)));

 out:
  if (str != NULL)
    g_string_free (str, TRUE);
  g_list_free_full (blocks, g_object_unref);
  g_free (escaped_name);
  g_free (encoded_name);

  return TRUE; /* returning TRUE means that we handled the method invocation */
}

static void
lvm_manager_iface_init (LvmManagerIface *iface)
{
  iface->handle_volume_group_create = handle_volume_group_create;
}

UlManager *
ul_manager_new (void)
{
  return g_object_new (UL_TYPE_MANAGER, NULL);
}
