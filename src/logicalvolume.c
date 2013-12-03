/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
 * Copyright (C) 2013 Marius Vollmer <marius.vollmer@redhat.com>
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
#include <glib/gi18n-lib.h>

#include "logicalvolume.h"

#include "blockobject.h"
#include "daemon.h"
#include "logicalvolumeobject.h"
#include "util.h"
#include "volumegroup.h"
#include "volumegroupobject.h"

/**
 * SECTION:udiskslinuxlogicalvolume
 * @title: UlLogicalVolume
 * @short_description: Linux implementation of #xUDisksLogicalVolume
 *
 * This type provides an implementation of the #xUDisksLogicalVolume
 * interface on Linux.
 */

typedef struct _UlLogicalVolumeClass   UlLogicalVolumeClass;

/**
 * UlLogicalVolume:
 *
 * The #UlLogicalVolume structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _UlLogicalVolume
{
  LvmLogicalVolumeSkeleton parent_instance;
};

struct _UlLogicalVolumeClass
{
  LvmLogicalVolumeSkeletonClass parent_class;
};

static void logical_volume_iface_init (LvmLogicalVolumeIface *iface);

G_DEFINE_TYPE_WITH_CODE (UlLogicalVolume, ul_logical_volume, LVM_TYPE_LOGICAL_VOLUME_SKELETON,
                         G_IMPLEMENT_INTERFACE (LVM_TYPE_LOGICAL_VOLUME, logical_volume_iface_init)
);

/* ---------------------------------------------------------------------------------------------------- */

static void
ul_logical_volume_init (UlLogicalVolume *self)
{
  g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (self),
                                       G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
}

static void
ul_logical_volume_class_init (UlLogicalVolumeClass *klass)
{

}

/**
 * ul_logical_volume_new:
 *
 * Creates a new #UlLogicalVolume instance.
 *
 * Returns: A new #UlLogicalVolume. Free with g_object_unref().
 */
UlLogicalVolume *
ul_logical_volume_new (void)
{
  return g_object_new (UL_TYPE_LOGICAL_VOLUME, NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * ul_logical_volume_update:
 * @logical_volume: A #UlLogicalVolume.
 * @vg: LVM volume group
 * @lv: LVM logical volume
 *
 * Updates the interface.
 */
void
ul_logical_volume_update (UlLogicalVolume *self,
                          UlVolumeGroupObject *group_object,
                          GVariant *info,
                          gboolean *needs_polling_ret)
{
  LvmLogicalVolume *iface;
  const char *type;
  const char *pool_objpath;
  const char *origin_objpath;
  const gchar *str;
  guint64 num;

  iface = LVM_LOGICAL_VOLUME (self);

  if (g_variant_lookup (info, "name", "&s", &str))
    {
      gchar *decoded = ul_util_decode_lvm_name (str);
      lvm_logical_volume_set_name (iface, str);
      lvm_logical_volume_set_display_name (iface, decoded);
      g_free (decoded);
    }

  if (g_variant_lookup (info, "uuid", "&s", &str))
    lvm_logical_volume_set_uuid (iface, str);

  if (g_variant_lookup (info, "size", "t", &num))
    lvm_logical_volume_set_size (iface, num);

  type = "unsupported";
  if (g_variant_lookup (info, "lv_attr", "&s", &str)
      && str && strlen (str) > 6)
    {
      char volume_type = str[0];
      char target_type = str[6];

      switch (target_type)
        {
        case 's':
          type = "snapshot";
          break;
        case 'm':
          type = "mirror";
          break;
        case 't':
          if (volume_type == 't')
            type = "thin-pool";
          else
            type = "thin";
          *needs_polling_ret = TRUE;
          break;
        case 'r':
          type = "raid";
          break;
        case '-':
          type = "plain";
          break;
        }
    }
  lvm_logical_volume_set_type_ (iface, type);

  if (g_variant_lookup (info, "data_percent", "t", &num)
      && (int64_t)num >= 0)
    lvm_logical_volume_set_data_allocated_ratio (iface, num/100000000.0);

  if (g_variant_lookup (info, "metadata_percent", "t", &num)
      && (int64_t)num >= 0)
    lvm_logical_volume_set_metadata_allocated_ratio (iface, num/100000000.0);

  pool_objpath = "/";
  if (g_variant_lookup (info, "pool_lv", "&s", &str)
      && str != NULL && *str)
    {
      UlLogicalVolumeObject *pool_object = ul_volume_group_object_find_logical_volume_object (group_object, str);
      if (pool_object)
        pool_objpath = g_dbus_object_get_object_path (G_DBUS_OBJECT (pool_object));
    }
  lvm_logical_volume_set_thin_pool (iface, pool_objpath);

  origin_objpath = "/";
  if (g_variant_lookup (info, "origin", "&s", &str)
      && str != NULL && *str)
    {
      UlLogicalVolumeObject *origin_object = ul_volume_group_object_find_logical_volume_object (group_object, str);
      if (origin_object)
        origin_objpath = g_dbus_object_get_object_path (G_DBUS_OBJECT (origin_object));
    }
  lvm_logical_volume_set_origin (iface, origin_objpath);

  lvm_logical_volume_set_volume_group (iface, g_dbus_object_get_object_path (G_DBUS_OBJECT (group_object)));
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_delete (LvmLogicalVolume *volume,
               GDBusMethodInvocation *invocation,
               GVariant *options)
{
  GError *error = NULL;
  UlLogicalVolumeObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  UlVolumeGroupObject *group_object;
  gchar *escaped_group_name = NULL;
  gchar *escaped_name = NULL;
  gchar *error_message = NULL;

  object = UL_LOGICAL_VOLUME_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (volume)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  if (!ul_daemon_get_caller_uid_sync (daemon,
                                      invocation,
                                      NULL /* GCancellable */,
                                      &caller_uid,
                                      &caller_gid,
                                      NULL,
                                      &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
      goto out;
    }

  message = N_("Authentication is required to delete a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  group_object = ul_logical_volume_object_get_volume_group (object);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (group_object));
  escaped_name = ul_util_escape_and_quote (ul_logical_volume_object_get_name (object));

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-lvol-delete", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "lvremove -f %s/%s",
                                          escaped_group_name,
                                          escaped_name))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error deleting logical volume: %s",
                                             error_message);
      goto out;
    }

  lvm_logical_volume_complete_delete (volume, invocation);

 out:
  g_free (error_message);
  g_free (escaped_name);
  g_free (escaped_group_name);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

struct WaitData {
  UlVolumeGroupObject *group_object;
  const gchar *name;
};

static GDBusObject *
wait_for_logical_volume_object (UlDaemon *daemon,
                                gpointer user_data)
{
  struct WaitData *data = user_data;
  return G_DBUS_OBJECT (ul_volume_group_object_find_logical_volume_object (data->group_object,
                                                                           data->name));
}

static const gchar *
wait_for_logical_volume_path (UlVolumeGroupObject *group_object,
                              const gchar *name,
                              GError **error)
{
  struct WaitData data;
  GDBusObject *volume_object;

  data.group_object = group_object;
  data.name = name;
  volume_object = ul_daemon_wait_for_object_sync (ul_daemon_get (),
                                                  wait_for_logical_volume_object,
                                                  &data,
                                                  NULL,
                                                  10, /* timeout_seconds */
                                                  error);
  if (volume_object == NULL)
    return NULL;

  return g_dbus_object_get_object_path (volume_object);
}

static gboolean
handle_rename (LvmLogicalVolume *volume,
               GDBusMethodInvocation *invocation,
               const gchar *new_name,
               GVariant *options)
{
  GError *error = NULL;
  UlLogicalVolumeObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  UlVolumeGroupObject *group_object;
  gchar *escaped_group_name = NULL;
  gchar *escaped_name = NULL;
  gchar *encoded_new_name = NULL;
  gchar *escaped_new_name = NULL;
  gchar *error_message = NULL;
  const gchar *lv_objpath;

  object = UL_LOGICAL_VOLUME_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (volume)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  if (!ul_daemon_get_caller_uid_sync (daemon,
                                      invocation,
                                      NULL /* GCancellable */,
                                      &caller_uid,
                                      &caller_gid,
                                      NULL,
                                      &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
      goto out;
    }

  message = N_("Authentication is required to rename a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  group_object = ul_logical_volume_object_get_volume_group (object);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (group_object));
  escaped_name = ul_util_escape_and_quote (ul_logical_volume_object_get_name (object));
  encoded_new_name = ul_util_encode_lvm_name (new_name, TRUE);
  escaped_new_name = ul_util_escape_and_quote (encoded_new_name);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-rename", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "lvrename %s/%s %s",
                                          escaped_group_name,
                                          escaped_name,
                                          escaped_new_name))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error renaming volume volume: %s",
                                             error_message);
      goto out;
    }

  lv_objpath = wait_for_logical_volume_path (group_object, encoded_new_name, &error);
  if (lv_objpath == NULL)
    {
      g_prefix_error (&error,
                      "Error waiting for logical volume object for %s",
                      new_name);
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_logical_volume_complete_rename (volume, invocation, lv_objpath);

 out:
  g_free (error_message);
  g_free (escaped_name);
  g_free (escaped_group_name);
  g_free (encoded_new_name);
  g_free (escaped_new_name);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_resize (LvmLogicalVolume *volume,
               GDBusMethodInvocation *invocation,
               guint64 new_size,
               int stripes,
               guint64 stripesize,
               GVariant *options)
{
  GError *error = NULL;
  UlLogicalVolumeObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  UlVolumeGroupObject *group_object;
  GString *cmd = NULL;
  gchar *escaped_group_name = NULL;
  gchar *escaped_name = NULL;
  gchar *error_message = NULL;

  object = UL_LOGICAL_VOLUME_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (volume)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  if (!ul_daemon_get_caller_uid_sync (daemon,
                                      invocation,
                                      NULL /* GCancellable */,
                                      &caller_uid,
                                      &caller_gid,
                                      NULL,
                                      &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
      goto out;
    }

  message = N_("Authentication is required to rename a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  group_object = ul_logical_volume_object_get_volume_group (object);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (group_object));
  escaped_name = ul_util_escape_and_quote (ul_logical_volume_object_get_name (object));
  new_size -= new_size % 512;

  cmd = g_string_new ("");
  g_string_append_printf (cmd, "lvresize %s/%s -r -L %" G_GUINT64_FORMAT "b",
                          escaped_group_name, escaped_name, new_size);

  if (stripes > 0)
    g_string_append_printf (cmd, " -i %d", stripes);

  if (stripesize > 0)
    g_string_append_printf (cmd, " -I %" G_GUINT64_FORMAT "b", stripesize);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-resize", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "%s", cmd->str))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error resizing logical volume: %s",
                                             error_message);
      goto out;
    }

  lvm_logical_volume_complete_resize (volume, invocation);

 out:
  if (cmd)
    g_string_free (cmd, TRUE);
  g_free (error_message);
  g_free (escaped_name);
  g_free (escaped_group_name);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusObject *
wait_for_logical_volume_block_object (UlDaemon *daemon,
                                      gpointer user_data)
{
  UlLogicalVolumeObject *volume_object = user_data;
  LvmLogicalVolumeBlock *logical_volume;
  const gchar *volume_objpath;
  GList *objects, *l;
  GDBusObject *ret = NULL;

  volume_objpath = g_dbus_object_get_object_path (G_DBUS_OBJECT (volume_object));

  objects = ul_daemon_get_objects (daemon);
  for (l = objects; l != NULL; l = l->next)
    {
      logical_volume = lvm_object_peek_logical_volume_block (l->data);
      if (logical_volume == NULL)
        continue;

      if (g_strcmp0 (lvm_logical_volume_block_get_logical_volume (logical_volume), volume_objpath) == 0)
        {
          ret = g_object_ref (l->data);
          goto out;
        }
    }

 out:
  g_list_free_full (objects, g_object_unref);
  return ret;
}

static gboolean
handle_activate (LvmLogicalVolume *volume,
                 GDBusMethodInvocation *invocation,
                 GVariant *options)
{
  GError *error = NULL;

  UlLogicalVolumeObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  UlVolumeGroupObject *group_object;
  gchar *escaped_group_name = NULL;
  gchar *escaped_name = NULL;
  gchar *error_message = NULL;
  GDBusObject *block_object = NULL;

  object = UL_LOGICAL_VOLUME_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (volume)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  if (!ul_daemon_get_caller_uid_sync (daemon,
                                               invocation,
                                               NULL /* GCancellable */,
                                               &caller_uid,
                                               &caller_gid,
                                               NULL,
                                               &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
      goto out;
    }

  message = N_("Authentication is required to activate a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  group_object = ul_logical_volume_object_get_volume_group (object);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (group_object));
  escaped_name = ul_util_escape_and_quote (ul_logical_volume_object_get_name (object));

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-lvol-activate", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "lvchange %s/%s -a y",
                                          escaped_group_name,
                                          escaped_name))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error deleting logical volume: %s",
                                             error_message);
      goto out;
    }

  block_object = ul_daemon_wait_for_object_sync (daemon,
                                                 wait_for_logical_volume_block_object,
                                                 object,
                                                 NULL,
                                                 10, /* timeout_seconds */
                                                 &error);
  if (block_object == NULL)
    {
      g_prefix_error (&error,
                      "Error waiting for block object for %s",
                      lvm_logical_volume_get_name (volume));
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_logical_volume_complete_activate (volume, invocation,
                                        g_dbus_object_get_object_path (G_DBUS_OBJECT (block_object)));

 out:
  g_free (error_message);
  g_free (escaped_name);
  g_free (escaped_group_name);
  g_clear_object (&block_object);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_deactivate (LvmLogicalVolume *volume,
                   GDBusMethodInvocation *invocation,
                   GVariant *options)
{
  GError *error = NULL;

  UlLogicalVolumeObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  UlVolumeGroupObject *group_object;
  gchar *escaped_group_name = NULL;
  gchar *escaped_name = NULL;
  gchar *error_message = NULL;

  object = UL_LOGICAL_VOLUME_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (volume)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  if (!ul_daemon_get_caller_uid_sync (daemon,
                                      invocation,
                                      NULL /* GCancellable */,
                                      &caller_uid,
                                      &caller_gid,
                                      NULL,
                                      &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
      goto out;
    }

  message = N_("Authentication is required to deactivate a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  group_object = ul_logical_volume_object_get_volume_group (object);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (group_object));
  escaped_name = ul_util_escape_and_quote (ul_logical_volume_object_get_name (object));

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-lvol-deactivate", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "lvchange %s/%s -a n",
                                          escaped_group_name,
                                          escaped_name))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error deleting logical volume: %s",
                                             error_message);
      goto out;
    }

  lvm_logical_volume_complete_deactivate (volume, invocation);

 out:
  g_free (error_message);
  g_free (escaped_name);
  g_free (escaped_group_name);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_create_snapshot (LvmLogicalVolume *volume,
                        GDBusMethodInvocation *invocation,
                        const gchar *name,
                        guint64 size,
                        GVariant *options)
{
  GError *error = NULL;
  UlLogicalVolumeObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  UlVolumeGroupObject *group_object;
  gchar *encoded_volume_name = NULL;
  gchar *escaped_volume_name = NULL;
  gchar *escaped_group_name = NULL;
  gchar *escaped_origin_name = NULL;
  GString *cmd = NULL;
  gchar *error_message = NULL;
  const gchar *lv_objpath = NULL;

  object = UL_LOGICAL_VOLUME_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (volume)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  if (!ul_daemon_get_caller_uid_sync (daemon,
                                      invocation,
                                      NULL /* GCancellable */,
                                      &caller_uid,
                                      &caller_gid,
                                      NULL,
                                      &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
      goto out;
    }

  message = N_("Authentication is required to create a snapshot of a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  encoded_volume_name = ul_util_encode_lvm_name (name, TRUE);
  escaped_volume_name = ul_util_escape_and_quote (encoded_volume_name);
  group_object = ul_logical_volume_object_get_volume_group (object);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (group_object));
  escaped_origin_name = ul_util_escape_and_quote (ul_logical_volume_object_get_name (object));

  cmd = g_string_new ("lvcreate");
  g_string_append_printf (cmd, " -s %s/%s -n %s",
                          escaped_group_name, escaped_origin_name, escaped_volume_name);

  if (size > 0)
    {
      size -= size % 512;
      g_string_append_printf (cmd, " -L %" G_GUINT64_FORMAT "b", size);
    }

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-lvol-snapshot", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "%s", cmd->str))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error creating snapshot: %s",
                                             error_message);
      goto out;
    }

  lv_objpath = wait_for_logical_volume_path (group_object, encoded_volume_name, &error);
  if (lv_objpath == NULL)
    {
      g_prefix_error (&error,
                     "Error waiting for logical volume object for %s",
                      name);
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_logical_volume_complete_create_snapshot (volume, invocation, lv_objpath);

 out:
  g_free (error_message);
  g_free (encoded_volume_name);
  g_free (escaped_volume_name);
  g_free (escaped_origin_name);
  g_free (escaped_group_name);
  if (cmd)
    g_string_free (cmd, TRUE);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
logical_volume_iface_init (LvmLogicalVolumeIface *iface)
{
  iface->handle_delete = handle_delete;
  iface->handle_rename = handle_rename;
  iface->handle_resize = handle_resize;
  iface->handle_activate = handle_activate;
  iface->handle_deactivate = handle_deactivate;
  iface->handle_create_snapshot = handle_create_snapshot;
}
