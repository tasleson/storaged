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

#include "block.h"
#include "logicalvolume.h"
#include "physicalvolume.h"
#include "volumegroup.h"

#include "daemon.h"

#include "com.redhat.lvm2.h"

#include <glib/gi18n-lib.h>

#include <udisks/udisks.h>

#include <gudev/gudev.h>

#include <fcntl.h>

struct _UlBlock
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
} UlBlockClass;

enum
{
  PROP_0,
  PROP_UDEV_CLIENT,
  PROP_REAL_BLOCK,
};

G_DEFINE_TYPE (UlBlock, ul_block, LVM_TYPE_OBJECT_SKELETON);

static void
ul_block_finalize (GObject *object)
{
  UlBlock *self = UL_BLOCK (object);

  g_clear_object (&self->real_block);
  g_clear_object (&self->udev_client);
  g_clear_object (&self->iface_physical_volume);
  g_clear_object (&self->iface_logical_volume);

  G_OBJECT_CLASS (ul_block_parent_class)->finalize (object);
}

static void
ul_block_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  UlBlock *self = UL_BLOCK (object);

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
ul_block_init (UlBlock *self)
{

}

static void
ul_block_class_init (UlBlockClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ul_block_finalize;
  gobject_class->set_property = ul_block_set_property;

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

const gchar *
ul_block_get_object_path (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return g_dbus_object_get_object_path (G_DBUS_OBJECT (self));
}

GUdevDevice *
ul_block_get_udev (UlBlock *self)
{
  dev_t num;

  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);

  num = udisks_block_get_device_number (self->real_block);
  return g_udev_client_query_by_device_number (self->udev_client,
                                               G_UDEV_DEVICE_TYPE_BLOCK, num);
}

const gchar *
ul_block_get_device (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return udisks_block_get_device (self->real_block);
}

const gchar **
ul_block_get_symlinks (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return (const gchar **)udisks_block_get_symlinks (self->real_block);
}

const gchar *
ul_block_get_id_type (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return udisks_block_get_id_type (self->real_block);
}

const gchar *
ul_block_get_id_usage (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return udisks_block_get_id_usage (self->real_block);
}

const gchar *
ul_block_get_id_version (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return udisks_block_get_id_version (self->real_block);
}

const gchar *
ul_block_get_id_label (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return udisks_block_get_id_label (self->real_block);
}

const gchar *
ul_block_get_id_uuid (UlBlock *self)
{
  g_return_val_if_fail (UL_IS_BLOCK (self), NULL);
  return udisks_block_get_id_uuid (self->real_block);
}

gboolean
ul_block_is_unused (UlBlock *self,
                    GError **error)
{
  const gchar *device_file;
  int fd;

  g_return_val_if_fail (UL_IS_BLOCK (self), FALSE);

  device_file = ul_block_get_device (self);
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

void
ul_block_update_lv (UlBlock *self,
                    UlLogicalVolume *lv)
{
  const gchar *logical_volume_path;
  UlDaemon *daemon;

  g_return_if_fail (UL_IS_BLOCK (self));

  daemon = ul_daemon_get ();

  if (lv == NULL)
    {
      if (self->iface_logical_volume)
        {
          ul_daemon_unpublish (daemon, ul_block_get_object_path (self), self->iface_logical_volume);
          g_object_unref (self->iface_logical_volume);
          self->iface_logical_volume = NULL;
        }
    }
  else
    {
      logical_volume_path = ul_logical_volume_get_object_path (lv);
      if (self->iface_logical_volume)
        {
          lvm_logical_volume_block_set_logical_volume (self->iface_logical_volume,
                                                       logical_volume_path);
        }
      else
        {
          self->iface_logical_volume = lvm_logical_volume_block_skeleton_new ();
          lvm_logical_volume_block_set_logical_volume (self->iface_logical_volume,
                                                       logical_volume_path);
          ul_daemon_publish (daemon, ul_block_get_object_path (self), FALSE, self->iface_logical_volume);
        }
    }
}

void
ul_block_update_pv (UlBlock *self,
                    UlVolumeGroup *group,
                    GVariant *pv_info)
{
  UlDaemon *daemon;

  g_return_if_fail (UL_IS_BLOCK (self));

  daemon = ul_daemon_get ();

  if (group)
    {
     if (self->iface_physical_volume == NULL)
        {
          self->iface_physical_volume = ul_physical_volume_new ();
          ul_physical_volume_update (self->iface_physical_volume, group, pv_info);
          ul_daemon_publish (daemon, ul_block_get_object_path (self), FALSE, self->iface_physical_volume);
        }
      else
        {
          ul_physical_volume_update (self->iface_physical_volume, group, pv_info);
        }
    }
  else
    {
      if (self->iface_physical_volume != NULL)
        {
          ul_daemon_unpublish (daemon, ul_block_get_object_path (self), self->iface_physical_volume);
          g_object_unref (self->iface_physical_volume);
          self->iface_physical_volume = NULL;
        }
    }

}

void
ul_block_trigger_uevent (UlBlock *self)
{
  GUdevDevice *device;
  gchar* path = NULL;
  gint fd = -1;

  g_return_if_fail (UL_IS_BLOCK (self));

  /* TODO: would be nice with a variant to wait until the request uevent has been received by ourselves */

  device = ul_block_get_udev (self);
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
