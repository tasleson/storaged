/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
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

#include "blockobject.h"
#include "physicalvolume.h"
#include "volumegroupobject.h"

#include "daemon.h"

#include "com.redhat.lvm2.h"

#include <glib/gi18n-lib.h>

#include <udisks/udisks.h>

#include <gudev/gudev.h>

#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>

struct _UlBlockObject
{
  LvmObjectSkeleton parent;
  UDisksBlock *real_block;
  GUdevClient *udev_client;
  UlPhysicalVolume *iface_physical_volume;
  LvmLogicalVolumeBlock *iface_logical_volume;
};

typedef struct
{
  LvmObjectSkeletonClass parent;
} UlBlockObjectClass;

enum
{
  PROP_0,
  PROP_UDEV_CLIENT,
  PROP_REAL_BLOCK,
};

G_DEFINE_TYPE (UlBlockObject, ul_block_object, LVM_TYPE_OBJECT_SKELETON);

static void
ul_block_object_finalize (GObject *object)
{
  UlBlockObject *self = UL_BLOCK_OBJECT (object);

  g_clear_object (&self->real_block);
  g_clear_object (&self->udev_client);
  g_clear_object (&self->iface_physical_volume);
  g_clear_object (&self->iface_logical_volume);

  G_OBJECT_CLASS (ul_block_object_parent_class)->finalize (object);
}

static void
ul_block_object_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  UlBlockObject *self = UL_BLOCK_OBJECT (object);

  switch (prop_id)
    {
    case PROP_REAL_BLOCK:
      self->real_block = g_value_dup_object (value);
      g_assert (self->real_block != NULL);
      break;

    case PROP_UDEV_CLIENT:
      self->udev_client = g_value_dup_object (value);
      g_assert (self->udev_client != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ul_block_object_init (UlBlockObject *self)
{

}

static void
ul_block_object_class_init (UlBlockObjectClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ul_block_object_finalize;
  gobject_class->set_property = ul_block_object_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_REAL_BLOCK,
                                   g_param_spec_object ("real-block", "Real Block", "Real Block",
                                                        UDISKS_TYPE_BLOCK,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_UDEV_CLIENT,
                                   g_param_spec_object ("udev-client", "GUDev Client", "GUDev Client",
                                                        G_UDEV_TYPE_CLIENT,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

GUdevDevice *
ul_block_object_get_udev (UlBlockObject *self)
{
  dev_t num;

  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);

  num = udisks_block_get_device_number (self->real_block);
  return g_udev_client_query_by_device_number (self->udev_client,
                                               G_UDEV_DEVICE_TYPE_BLOCK, num);
}

const gchar *
ul_block_object_get_device (UlBlockObject *self)
{
  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);
  return udisks_block_get_device (self->real_block);
}

const gchar **
ul_block_object_get_symlinks (UlBlockObject *self)
{
  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);
  return (const gchar **)udisks_block_get_symlinks (self->real_block);
}

const gchar *
ul_block_object_get_id_type (UlBlockObject *self)
{
  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);
  return udisks_block_get_id_type (self->real_block);
}

const gchar *
ul_block_object_get_id_usage (UlBlockObject *self)
{
  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);
  return udisks_block_get_id_usage (self->real_block);
}

const gchar *
ul_block_object_get_id_version (UlBlockObject *self)
{
  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);
  return udisks_block_get_id_version (self->real_block);
}

const gchar *
ul_block_object_get_id_label (UlBlockObject *self)
{
  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);
  return udisks_block_get_id_label (self->real_block);
}

const gchar *
ul_block_object_get_id_uuid (UlBlockObject *self)
{
  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), NULL);
  return udisks_block_get_id_uuid (self->real_block);
}

gboolean
ul_block_object_is_unused (UlBlockObject *self,
                           GError **error)
{
  const gchar *device_file;
  int fd;

  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), FALSE);

  device_file = ul_block_object_get_device (self);
  fd = open (device_file, O_RDONLY | O_EXCL);
  if (fd < 0)
    {
      g_set_error (error, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                   "Error opening device %s: %m",
                   device_file);
      return FALSE;
    }
  close (fd);
  return TRUE;
}

static gboolean
was_block_object_partitioned (UlBlockObject *self)
{
  UDisksObject *real_object;
  real_object = UDISKS_OBJECT (g_dbus_interface_get_object (G_DBUS_INTERFACE (self->real_block)));
  return (real_object && udisks_object_peek_partition_table (real_object));
}

static gboolean
run_sync (const gchar *prog, ...)
{
  va_list ap;
  GError **error;
  enum { max_argc = 20 };
  const gchar *argv[max_argc+1];
  int argc = 0;
  const gchar *arg;
  gchar *standard_output;
  gchar *standard_error;
  gint exit_status;

  argv[argc++] = prog;
  va_start (ap, prog);
  while ((arg = va_arg (ap, const gchar *)))
    {
      if (argc < max_argc)
        argv[argc] = arg;
      argc++;
    }
  error = va_arg (ap, GError **);
  va_end (ap);

  if (argc > max_argc)
    {
      g_set_error (error, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                   "Too many arguments.");
      return FALSE;
    }

  argv[argc] = NULL;
  if (!g_spawn_sync (NULL,
                     (gchar **)argv,
                     NULL,
                     G_SPAWN_SEARCH_PATH,
                     NULL,
                     NULL,
                     &standard_output,
                     &standard_error,
                     &exit_status,
                     error))
    return FALSE;

  if (!g_spawn_check_exit_status (exit_status, error))
    {
      g_prefix_error (error, "stdout: '%s', stderr: '%s', ", standard_output, standard_error);
      g_free (standard_output);
      g_free (standard_error);
      return FALSE;
    }

  g_free (standard_output);
  g_free (standard_error);
  return TRUE;
}


gboolean
ul_block_object_wipe (UlBlockObject *self,
                      GError **error)
{
  LvmPhysicalVolumeBlock *physical_volume;
  const gchar *volume_group_objpath;
  UlVolumeGroupObject *volume_group;
  gchar *volume_group_name = NULL;
  gboolean was_partitioned;

  const gchar *device_file;
  int fd = -1;
  gchar zeroes[512];
  gboolean ret = TRUE;

  g_return_val_if_fail (UL_IS_BLOCK_OBJECT (self), FALSE);

  physical_volume = lvm_object_get_physical_volume_block (LVM_OBJECT (self));
  if (physical_volume)
    {
      volume_group_objpath = lvm_physical_volume_block_get_volume_group (physical_volume);
      volume_group = UL_VOLUME_GROUP_OBJECT (ul_daemon_find_object (ul_daemon_get (), volume_group_objpath));
      if (volume_group)
        {
          volume_group_name = g_strdup (ul_volume_group_object_get_name (volume_group));
          g_object_unref (volume_group);
        }

      g_object_unref (physical_volume);
    }

  was_partitioned = was_block_object_partitioned (self);

  device_file = ul_block_object_get_device (self);

  /* Remove partition table */
  memset (zeroes, 0, 512);
  fd = open (device_file, O_RDWR | O_EXCL);
  if (fd < 0)
    {
      g_set_error (error, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                   "Error opening device %s: %m",
                   device_file);
      ret = FALSE;
      goto out;
    }

  if (write (fd, zeroes, 512) != 512)
    {
      g_set_error (error, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                   "Error erasing device %s: %m",
                   device_file);
      ret = FALSE;
      goto out;
    }

  if (was_partitioned && ioctl (fd, BLKRRPART, NULL) < 0)
    {
      g_set_error (error, UDISKS_ERROR, UDISKS_ERROR_FAILED,
                   "Error removing partition devices of %s: %m",
                   device_file);
      ret = FALSE;
      goto out;
    }
  close (fd);
  fd = -1;

  // wipe other labels
  if (!run_sync ("wipefs", "-a", device_file, NULL, error))
    {
      ret = FALSE;
      goto out;
    }

  // Try to bring affected volume group back into consistency.
  if (volume_group_name != NULL)
    run_sync ("vgreduce", volume_group_name, "--removemissing", NULL, NULL);

 out:
  if (fd >= 0)
    close (fd);
  g_free (volume_group_name);
  return ret;

}

void
ul_block_object_update_lv (UlBlockObject *self,
                           UlLogicalVolumeObject *lv)
{
  const gchar *path;

  g_return_if_fail (UL_IS_BLOCK_OBJECT (self));

  if (lv == NULL)
    {
      if (self->iface_logical_volume)
        {
          g_dbus_object_skeleton_remove_interface (G_DBUS_OBJECT_SKELETON (self),
                                                   G_DBUS_INTERFACE_SKELETON (self->iface_logical_volume));
          g_object_unref (self->iface_logical_volume);
          self->iface_logical_volume = NULL;
        }
    }
  else
    {
      path = g_dbus_object_get_object_path (G_DBUS_OBJECT (lv));
      if (self->iface_logical_volume)
        {
          lvm_logical_volume_block_set_logical_volume (self->iface_logical_volume, path);
        }
      else
        {
          self->iface_logical_volume = lvm_logical_volume_block_skeleton_new ();
          lvm_logical_volume_block_set_logical_volume (self->iface_logical_volume, path);
          g_dbus_object_skeleton_add_interface (G_DBUS_OBJECT_SKELETON (self),
                                                G_DBUS_INTERFACE_SKELETON (self->iface_logical_volume));
        }
    }
}

void
ul_block_object_update_pv (UlBlockObject *self,
                           UlVolumeGroupObject *group_object,
                           GVariant *pv_info)
{
  g_return_if_fail (UL_IS_BLOCK_OBJECT (self));

  if (group_object)
    {
     if (self->iface_physical_volume == NULL)
        {
          self->iface_physical_volume = ul_physical_volume_new ();
          ul_physical_volume_update (self->iface_physical_volume, group_object, pv_info);
          g_dbus_object_skeleton_add_interface (G_DBUS_OBJECT_SKELETON (self),
                                                G_DBUS_INTERFACE_SKELETON (self->iface_physical_volume));
        }
      else
        {
          ul_physical_volume_update (self->iface_physical_volume, group_object, pv_info);
        }
    }
  else
    {
      if (self->iface_physical_volume != NULL)
        {
          g_dbus_object_skeleton_remove_interface (G_DBUS_OBJECT_SKELETON (self),
                                                   G_DBUS_INTERFACE_SKELETON (self->iface_physical_volume));
          g_object_unref (self->iface_physical_volume);
          self->iface_physical_volume = NULL;
        }
    }

}

void
ul_block_object_trigger_uevent (UlBlockObject *self)
{
  GUdevDevice *device;
  gchar* path = NULL;
  gint fd = -1;

  g_return_if_fail (UL_IS_BLOCK_OBJECT (self));

  /* TODO: would be nice with a variant to wait until the request uevent has been received by ourselves */

  device = ul_block_object_get_udev (self);
  if (device == NULL)
    {
      g_debug ("skipping trigger of udev event for block object");
      return;
    }

  path = g_strconcat (g_udev_device_get_sysfs_path (device), "/uevent", NULL);
  g_debug ("trigerring udev event '%s' for %s", "change", g_udev_device_get_name (device));
  g_object_unref (device);

  fd = open (path, O_WRONLY);
  if (fd < 0)
    {
      g_message ("Error opening %s: %m", path);
      goto out;
    }

  if (write (fd, "change", sizeof "change" - 1) != sizeof "change" - 1)
    {
      g_message ("Error writing 'change' to file %s: %m", path);
      goto out;
    }

 out:
  if (fd >= 0)
    close (fd);
  g_free (path);
}
