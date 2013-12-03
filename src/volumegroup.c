/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
 * Copyright (C) 2013 Marius Vollmer <marius.vollmer@gmail.com>
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

#include "volumegroup.h"

#include "blockobject.h"
#include "daemon.h"
#include "logicalvolume.h"
#include "logicalvolumeobject.h"
#include "manager.h"
#include "util.h"
#include "volumegroupobject.h"

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mntent.h>

/**
 * SECTION:udiskslinuxvolume_group
 * @title: UlVolumeGroup
 * @short_description: Linux implementation of #LvmVolumeGroup
 *
 * This type provides an implementation of the #LvmVolumeGroup interface
 * on Linux.
 */

typedef struct _UlVolumeGroupClass   UlVolumeGroupClass;

/**
 * UlVolumeGroup:
 *
 * The #UlVolumeGroup structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _UlVolumeGroup
{
  LvmVolumeGroupSkeleton parent_instance;
};

struct _UlVolumeGroupClass
{
  LvmVolumeGroupSkeletonClass parent_class;
};

static void volume_group_iface_init (LvmVolumeGroupIface *iface);

G_DEFINE_TYPE_WITH_CODE (UlVolumeGroup, ul_volume_group, LVM_TYPE_VOLUME_GROUP_SKELETON,
                         G_IMPLEMENT_INTERFACE (LVM_TYPE_VOLUME_GROUP, volume_group_iface_init)
);

/* ---------------------------------------------------------------------------------------------------- */

static void
ul_volume_group_init (UlVolumeGroup *self)
{
  g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (self),
                                       G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
}

static void
ul_volume_group_class_init (UlVolumeGroupClass *klass)
{

}

/**
 * ul_volume_group_new:
 *
 * Creates a new #UlVolumeGroup instance.
 *
 * Returns: A new #UlVolumeGroup. Free with g_object_unref().
 */
LvmVolumeGroup *
ul_volume_group_new (void)
{
  return g_object_new (UL_TYPE_VOLUME_GROUP, NULL);
}

/**
 * ul_volume_group_update:
 * @self: A #UlVolumeGroup.
 * @object: The enclosing #UlVolumeGroupObject instance.
 *
 * Updates the interface.
 */
void
ul_volume_group_update (UlVolumeGroup *self,
                        GVariant *info,
                        gboolean *needs_polling_ret)
{
  LvmVolumeGroup *iface = LVM_VOLUME_GROUP (self);
  const gchar *str;
  guint64 num;

  if (g_variant_lookup (info, "name", "&s", &str))
    {
      gchar *decoded = ul_util_decode_lvm_name (str);
      lvm_volume_group_set_name (iface, str);
      lvm_volume_group_set_display_name (iface, decoded);
      g_free (decoded);
    }

  if (g_variant_lookup (info, "uuid", "&s", &str))
    lvm_volume_group_set_uuid (iface, str);

  if (g_variant_lookup (info, "size", "t", &num))
    lvm_volume_group_set_size (iface, num);

  if (g_variant_lookup (info, "free-size", "t", &num))
    lvm_volume_group_set_free_size (iface, num);

  if (g_variant_lookup (info, "extent-size", "t", &num))
    lvm_volume_group_set_extent_size (iface, num);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_poll (LvmVolumeGroup *group,
             GDBusMethodInvocation *invocation)
{
  UlVolumeGroupObject *object = NULL;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
  g_return_val_if_fail (object != NULL, FALSE);

  ul_volume_group_object_poll (object);
  lvm_volume_group_complete_poll (group, invocation);

  g_object_unref (object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_delete (LvmVolumeGroup *group,
               GDBusMethodInvocation *invocation,
               GVariant *arg_options)
{
  GError *error = NULL;
  UlVolumeGroupObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  gchar *escaped_name = NULL;
  gchar *error_message = NULL;
  gboolean opt_wipe = FALSE;
  GList *objects_to_wipe = NULL;
  GList *l;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  // Find physical volumes to wipe.

  g_variant_lookup (arg_options, "wipe", "b", &opt_wipe);
  if (opt_wipe)
    {
      GList *objects = ul_daemon_get_objects (daemon);
      for (l = objects; l; l = l->next)
        {
          LvmPhysicalVolumeBlock *physical_volume;

          physical_volume = lvm_object_peek_physical_volume_block (l->data);
          if (physical_volume
              && g_strcmp0 (lvm_physical_volume_block_get_volume_group (physical_volume),
                            g_dbus_object_get_object_path (G_DBUS_OBJECT (object))) == 0)
            objects_to_wipe = g_list_append (objects_to_wipe, g_object_ref (l->data));
        }
      g_list_free_full (objects, g_object_unref);
    }

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

  message = N_("Authentication is required to delete a volume group");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           arg_options,
                                           message,
                                           invocation))
    goto out;

  escaped_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (object));

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-delete", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "vgremove -f %s",
                                          escaped_name))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error deleting volume group: %s",
                                             error_message);
      goto out;
    }

  for (l = objects_to_wipe; l; l = l->next)
    ul_block_object_wipe (l->data, NULL);

  lvm_volume_group_complete_delete (group, invocation);

 out:
  g_list_free_full (objects_to_wipe, g_object_unref);
  g_free (error_message);
  g_free (escaped_name);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusObject *
wait_for_volume_group_object (UlDaemon *daemon,
                              gpointer user_data)
{
  const gchar *name = user_data;
  UlManager *manager;

  manager = ul_daemon_get_manager (daemon);
  return G_DBUS_OBJECT (ul_manager_find_volume_group_object (manager, name));
}

static gboolean
handle_rename (LvmVolumeGroup *_group,
               GDBusMethodInvocation *invocation,
               const gchar *new_name,
               GVariant *options)
{
  GError *error = NULL;
  UlVolumeGroup *group = UL_VOLUME_GROUP (_group);
  UlVolumeGroupObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  gchar *escaped_name = NULL;
  gchar *encoded_new_name = NULL;
  gchar *escaped_new_name = NULL;
  gchar *error_message = NULL;
  GDBusObject *group_object = NULL;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
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

  message = N_("Authentication is required to rename a volume group");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  escaped_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (object));
  encoded_new_name = ul_util_encode_lvm_name (new_name, FALSE);
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
                                          "vgrename %s %s",
                                          escaped_name,
                                          escaped_new_name))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error renaming volume group: %s",
                                             error_message);
      goto out;
    }

  group_object = ul_daemon_wait_for_object_sync (daemon,
                                                 wait_for_volume_group_object,
                                                 (gpointer)encoded_new_name,
                                                 NULL,
                                                 10, /* timeout_seconds */
                                                 &error);
  if (group_object == NULL)
    {
      g_prefix_error (&error,
                      "Error waiting for volume group object for %s",
                      new_name);
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_volume_group_complete_rename (_group,
                                       invocation,
                                       g_dbus_object_get_object_path (G_DBUS_OBJECT (group_object)));

 out:
  g_free (error_message);
  g_free (escaped_name);
  g_free (encoded_new_name);
  g_free (escaped_new_name);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_add_device (LvmVolumeGroup *group,
                   GDBusMethodInvocation  *invocation,
                   const gchar *new_member_device_objpath,
                   GVariant *options)
{
  UlDaemon *daemon;
  UlVolumeGroupObject *object;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  const gchar *new_member_device_file = NULL;
  gchar *escaped_new_member_device_file = NULL;
  GError *error = NULL;
  gchar *error_message = NULL;
  GDBusObject *new_member_device_object = NULL;
  UlBlockObject *new_member_device = NULL;
  gchar *escaped_name = NULL;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  error = NULL;
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

  new_member_device_object = ul_daemon_find_object (daemon, new_member_device_objpath);
  if (new_member_device_object == NULL)
    {
      g_dbus_method_invocation_return_error (invocation, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                                             "No device for given object path");
      goto out;
    }

  if (UL_IS_BLOCK_OBJECT (new_member_device_object))
    {
      new_member_device = UL_BLOCK_OBJECT (new_member_device_object);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                                             "The given object is not a block");
      goto out;
    }

  message = N_("Authentication is required to add a device to a volume group");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  if (!ul_block_object_is_unused (new_member_device, &error))
    {
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  if (!ul_block_object_wipe (new_member_device, &error))
    {
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  escaped_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (object));
  new_member_device_file = ul_block_object_get_device (new_member_device);
  escaped_new_member_device_file = ul_util_escape_and_quote (new_member_device_file);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-add-device", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "vgextend %s %s",
                                          escaped_name,
                                          escaped_new_member_device_file))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error adding %s to volume group: %s",
                                             new_member_device_file,
                                             error_message);
      goto out;
    }

  lvm_volume_group_complete_add_device (group, invocation);

 out:
  g_free (error_message);
  g_free (escaped_name);
  g_free (escaped_new_member_device_file);
  g_clear_object (&new_member_device_object);
  g_clear_object (&new_member_device);
  g_clear_object (&object);
  return TRUE; /* returning TRUE means that we handled the method invocation */
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_remove_device (LvmVolumeGroup *group,
                      GDBusMethodInvocation *invocation,
                      const gchar *member_device_objpath,
                      GVariant *options)
{
  UlDaemon *daemon;
  UlVolumeGroupObject *object;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  const gchar *member_device_file = NULL;
  gchar *escaped_member_device_file = NULL;
  GError *error = NULL;
  gchar *error_message = NULL;
  GDBusObject *member_device_object = NULL;
  UlBlockObject *member_device = NULL;
  gchar *escaped_name = NULL;
  gboolean opt_wipe = FALSE;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  g_variant_lookup (options, "wipe", "b", &opt_wipe);

  error = NULL;
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

  member_device_object = ul_daemon_find_object (daemon, member_device_objpath);
  if (member_device_object == NULL)
    {
      g_dbus_method_invocation_return_error (invocation, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                                             "No device for given object path");
      goto out;
    }

  if (UL_IS_BLOCK_OBJECT (member_device_object))
    {
      member_device = UL_BLOCK_OBJECT (member_device_object);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                                             "The given object is not a block");
      goto out;
    }

  message = N_("Authentication is required to remove a device from a volume group");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  escaped_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (object));
  member_device_file = ul_block_object_get_device (member_device);
  escaped_member_device_file = ul_util_escape_and_quote (member_device_file);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-rem-device", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "vgreduce %s %s",
                                          escaped_name,
                                          escaped_member_device_file))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error remove %s from volume group: %s",
                                             member_device_file,
                                             error_message);
      goto out;
    }

  if (opt_wipe)
    {
      if (!ul_daemon_launch_spawned_job_sync (daemon,
                                              LVM_OBJECT (member_device_object),
                                              "format-erase", caller_uid,
                                              NULL, /* GCancellable */
                                              0,    /* uid_t run_as_uid */
                                              0,    /* uid_t run_as_euid */
                                              NULL, /* gint *out_status */
                                              &error_message,
                                              NULL,  /* input_string */
                                              "wipefs -a %s",
                                              escaped_member_device_file))
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 UDISKS_ERROR,
                                                 UDISKS_ERROR_FAILED,
                                                 "Error wiping  %s after removal from volume group %s: %s",
                                                 member_device_file,
                                                 ul_volume_group_object_get_name (object),
                                                 error_message);
          goto out;
        }
    }

  lvm_volume_group_complete_remove_device (group, invocation);

 out:
  g_free (error_message);
  g_free (escaped_name);
  g_free (escaped_member_device_file);
  g_clear_object (&member_device_object);
  g_clear_object (&member_device);
  g_clear_object (&object);
  return TRUE; /* returning TRUE means that we handled the method invocation */
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_empty_device (LvmVolumeGroup *group,
                     GDBusMethodInvocation *invocation,
                     const gchar *member_device_objpath,
                     GVariant *options)
{
  UlDaemon *daemon;
  UlVolumeGroupObject *object;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  const gchar *member_device_file = NULL;
  gchar *escaped_member_device_file = NULL;
  GError *error = NULL;
  gchar *error_message = NULL;
  GDBusObject *member_device_object = NULL;
  UlBlockObject *member_device = NULL;
  gboolean no_block = FALSE;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
  g_return_val_if_fail (object != NULL, FALSE);

  daemon = ul_daemon_get ();

  g_variant_lookup (options, "no-block", "b", &no_block);

  error = NULL;
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

  member_device_object = ul_daemon_find_object (daemon, member_device_objpath);
  if (member_device_object == NULL)
    {
      g_dbus_method_invocation_return_error (invocation, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                                             "No device for given object path");
      goto out;
    }

  if (UL_IS_BLOCK_OBJECT (member_device_object))
    {
      member_device = UL_BLOCK_OBJECT (member_device_object);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                                             "The given object is not a block");
      goto out;
    }

  message = N_("Authentication is required to empty a device in a volume group");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  member_device_file = ul_block_object_get_device (member_device);
  escaped_member_device_file = ul_util_escape_and_quote (member_device_file);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (member_device_object),
                                          "lvm-vg-empty-device", caller_uid,
                                          NULL, /* GCancellable */
                                          0,    /* uid_t run_as_uid */
                                          0,    /* uid_t run_as_euid */
                                          NULL, /* gint *out_status */
                                          &error_message,
                                          NULL,  /* input_string */
                                          "pvmove %s%s",
                                          no_block? "-b " : "",
                                          escaped_member_device_file))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error emptying %s: %s",
                                             member_device_file,
                                             error_message);
      goto out;
    }

  if (!no_block)
    lvm_volume_group_complete_remove_device (group, invocation);

 out:
  g_free (error_message);
  g_free (escaped_member_device_file);
  g_clear_object (&member_device_object);
  g_clear_object (&member_device);
  g_clear_object (&object);
  return TRUE; /* returning TRUE means that we handled the method invocation */
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
  UlDaemon *daemon;
  GDBusObject *volume_object;

  data.group_object = group_object;
  data.name = name;
  daemon = ul_daemon_get ();
  volume_object = ul_daemon_wait_for_object_sync (daemon,
                                                  wait_for_logical_volume_object,
                                                  &data,
                                                  NULL,
                                                  10, /* timeout_seconds */
                                                  error);
  if (volume_object == NULL)
    return NULL;

  return g_dbus_object_get_object_path (G_DBUS_OBJECT (volume_object));
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_create_plain_volume (LvmVolumeGroup *group,
                            GDBusMethodInvocation *invocation,
                            const gchar *arg_name,
                            guint64 arg_size,
                            gint arg_stripes,
                            guint64 arg_stripesize,
                            GVariant *options)
{
  GError *error = NULL;
  UlVolumeGroupObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  gchar *encoded_volume_name = NULL;
  gchar *escaped_volume_name = NULL;
  gchar *escaped_group_name = NULL;
  GString *cmd = NULL;
  gchar *error_message = NULL;
  const gchar *lv_objpath;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
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

  message = N_("Authentication is required to create a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  encoded_volume_name = ul_util_encode_lvm_name (arg_name, TRUE);
  escaped_volume_name = ul_util_escape_and_quote (encoded_volume_name);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (object));
  arg_size -= arg_size % 512;

  cmd = g_string_new ("");
  g_string_append_printf (cmd, "lvcreate %s -L %" G_GUINT64_FORMAT "b -n %s",
                          escaped_group_name, arg_size, escaped_volume_name);

  if (arg_stripes > 0)
    g_string_append_printf (cmd, " -i %d", arg_stripes);

  if (arg_stripesize > 0)
    g_string_append_printf (cmd, " -I %" G_GUINT64_FORMAT "b", arg_stripesize);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-create-volume", caller_uid,
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
                                             "Error creating volume: %s",
                                             error_message);
      goto out;
    }

  lv_objpath = wait_for_logical_volume_path (object, encoded_volume_name, &error);
  if (lv_objpath == NULL)
    {
      g_prefix_error (&error,
                      "Error waiting for logical volume object for %s",
                      arg_name);
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_volume_group_complete_create_plain_volume (group, invocation, lv_objpath);

 out:
  g_free (error_message);
  g_free (escaped_group_name);
  g_free (encoded_volume_name);
  g_free (escaped_volume_name);
  g_string_free (cmd, TRUE);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_create_thin_pool_volume (LvmVolumeGroup *group,
                                GDBusMethodInvocation *invocation,
                                const gchar *arg_name,
                                guint64 arg_size,
                                GVariant *options)
{
  GError *error = NULL;
  UlVolumeGroupObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  gchar *encoded_volume_name = NULL;
  gchar *escaped_volume_name = NULL;
  gchar *escaped_group_name = NULL;
  GString *cmd = NULL;
  gchar *error_message = NULL;
  const gchar *lv_objpath;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
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

  message = N_("Authentication is required to create a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  encoded_volume_name = ul_util_encode_lvm_name (arg_name, TRUE);
  escaped_volume_name = ul_util_escape_and_quote (encoded_volume_name);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (object));
  arg_size -= arg_size % 512;

  cmd = g_string_new ("");
  g_string_append_printf (cmd, "lvcreate %s -T -L %" G_GUINT64_FORMAT "b --thinpool %s",
                          escaped_group_name, arg_size, escaped_volume_name);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-create-volume", caller_uid,
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
                                             "Error creating volume: %s",
                                             error_message);
      goto out;
    }

  lv_objpath = wait_for_logical_volume_path (object, encoded_volume_name, &error);
  if (lv_objpath == NULL)
    {
      g_prefix_error (&error,
                      "Error waiting for logical volume object for %s",
                      arg_name);
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_volume_group_complete_create_thin_pool_volume (group, invocation, lv_objpath);

 out:
  g_free (error_message);
  g_free (encoded_volume_name);
  g_free (escaped_volume_name);
  g_free (escaped_group_name);
  g_string_free (cmd, TRUE);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_create_thin_volume (LvmVolumeGroup *group,
                           GDBusMethodInvocation *invocation,
                           const gchar *arg_name,
                           guint64 arg_size,
                           const gchar *arg_pool,
                           GVariant *options)
{
  GError *error = NULL;
  UlVolumeGroupObject *object = NULL;
  UlDaemon *daemon;
  const gchar *action_id;
  const gchar *message;
  uid_t caller_uid;
  gid_t caller_gid;
  UlLogicalVolumeObject *pool_object;
  gchar *encoded_volume_name = NULL;
  gchar *escaped_volume_name = NULL;
  gchar *escaped_group_name = NULL;
  gchar *escaped_pool_name = NULL;
  GString *cmd = NULL;
  gchar *error_message = NULL;
  const gchar *lv_objpath;

  object = UL_VOLUME_GROUP_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (group)));
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

  message = N_("Authentication is required to create a logical volume");
  action_id = "com.redhat.lvm2.manage-lvm";
  if (!ul_daemon_check_authorization_sync (daemon,
                                           LVM_OBJECT (object),
                                           action_id,
                                           options,
                                           message,
                                           invocation))
    goto out;

  pool_object = (UlLogicalVolumeObject *)ul_daemon_find_object (daemon, arg_pool);
  if (pool_object == NULL || !UL_IS_LOGICAL_VOLUME_OBJECT (pool_object))
    {
      g_dbus_method_invocation_return_error (invocation, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                                             "Not a logical volume");
      goto out;
    }

  encoded_volume_name = ul_util_encode_lvm_name (arg_name, TRUE);
  escaped_volume_name = ul_util_escape_and_quote (encoded_volume_name);
  escaped_group_name = ul_util_escape_and_quote (ul_volume_group_object_get_name (object));
  arg_size -= arg_size % 512;
  escaped_pool_name = ul_util_escape_and_quote (ul_logical_volume_object_get_name (pool_object));

  cmd = g_string_new ("");
  g_string_append_printf (cmd, "lvcreate %s --thinpool %s -V %" G_GUINT64_FORMAT "b -n %s",
                          escaped_group_name, escaped_pool_name, arg_size, escaped_volume_name);

  if (!ul_daemon_launch_spawned_job_sync (daemon,
                                          LVM_OBJECT (object),
                                          "lvm-vg-create-volume", caller_uid,
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
                                             "Error creating volume: %s",
                                             error_message);
      goto out;
    }

  lv_objpath = wait_for_logical_volume_path (object, encoded_volume_name, &error);
  if (lv_objpath == NULL)
    {
      g_prefix_error (&error,
                      "Error waiting for logical volume object for %s",
                      arg_name);
      g_dbus_method_invocation_take_error (invocation, error);
      goto out;
    }

  lvm_volume_group_complete_create_thin_pool_volume (group, invocation, lv_objpath);

 out:
  g_free (error_message);
  g_free (encoded_volume_name);
  g_free (escaped_volume_name);
  g_free (escaped_group_name);
  g_free (escaped_pool_name);
  g_string_free (cmd, TRUE);
  g_clear_object (&pool_object);
  g_clear_object (&object);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
volume_group_iface_init (LvmVolumeGroupIface *iface)
{
  iface->handle_poll = handle_poll;

  iface->handle_delete = handle_delete;
  iface->handle_rename = handle_rename;

  iface->handle_add_device = handle_add_device;
  iface->handle_remove_device = handle_remove_device;
  iface->handle_empty_device = handle_empty_device;

  iface->handle_create_plain_volume = handle_create_plain_volume;
  iface->handle_create_thin_pool_volume = handle_create_thin_pool_volume;
  iface->handle_create_thin_volume = handle_create_thin_volume;
}
