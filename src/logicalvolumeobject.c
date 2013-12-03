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

#include "logicalvolumeobject.h"

#include "daemon.h"
#include "logicalvolume.h"
#include "volumegroup.h"
#include "volumegroupobject.h"
#include "util.h"

#include <glib/gi18n-lib.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * SECTION:udiskslinuxlogicalvolumeobject
 * @title: UlLogicalVolumeObject
 * @short_description: Object representing a LVM2 logical volume
 */

typedef struct _UlLogicalVolumeObjectClass   UlLogicalVolumeObjectClass;

/**
 * UlLogicalVolumeObject:
 *
 * The #UlLogicalVolumeObject structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _UlLogicalVolumeObject
{
  LvmObjectSkeleton parent_instance;

  gchar *name;
  UlVolumeGroupObject *volume_group;

  LvmLogicalVolume *iface_logical_volume;
};

struct _UlLogicalVolumeObjectClass
{
  LvmObjectSkeletonClass parent_class;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_VOLUME_GROUP,
};

G_DEFINE_TYPE (UlLogicalVolumeObject, ul_logical_volume_object, LVM_TYPE_OBJECT_SKELETON);

static void
ul_logical_volume_object_finalize (GObject *obj)
{
  UlLogicalVolumeObject *self = UL_LOGICAL_VOLUME_OBJECT (obj);

  /* note: we don't hold a ref to self->daemon */

  if (self->iface_logical_volume != NULL)
    g_object_unref (self->iface_logical_volume);

  g_free (self->name);

  G_OBJECT_CLASS (ul_logical_volume_object_parent_class)->finalize (obj);
}

static void
ul_logical_volume_object_get_property (GObject *obj,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
  UlLogicalVolumeObject *self = UL_LOGICAL_VOLUME_OBJECT (obj);

  switch (prop_id)
    {
    case PROP_VOLUME_GROUP:
      g_value_set_object (value, ul_logical_volume_object_get_volume_group (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
ul_logical_volume_object_set_property (GObject *obj,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
  UlLogicalVolumeObject *self = UL_LOGICAL_VOLUME_OBJECT (obj);

  switch (prop_id)
    {
    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_VOLUME_GROUP:
      g_assert (self->volume_group == NULL);
      self->volume_group = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}


static void
ul_logical_volume_object_init (UlLogicalVolumeObject *self)
{

}

static void
ul_logical_volume_object_constructed (GObject *obj)
{
  UlLogicalVolumeObject *self = UL_LOGICAL_VOLUME_OBJECT (obj);
  GString *s;

  G_OBJECT_CLASS (ul_logical_volume_object_parent_class)->constructed (obj);

  /* compute the object path */

  s = g_string_new (g_dbus_object_get_object_path (G_DBUS_OBJECT (self->volume_group)));
  g_string_append_c (s, '/');
  ul_util_safe_append_to_object_path (s, self->name);
  g_dbus_object_skeleton_set_object_path (G_DBUS_OBJECT_SKELETON (self), s->str);
  g_string_free (s, TRUE);

  /* create the DBus interface */
  self->iface_logical_volume = LVM_LOGICAL_VOLUME (ul_logical_volume_new ());
  g_dbus_object_skeleton_add_interface (G_DBUS_OBJECT_SKELETON (self),
                                        G_DBUS_INTERFACE_SKELETON (self->iface_logical_volume));
}

static void
ul_logical_volume_object_class_init (UlLogicalVolumeObjectClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ul_logical_volume_object_finalize;
  gobject_class->constructed = ul_logical_volume_object_constructed;
  gobject_class->set_property = ul_logical_volume_object_set_property;
  gobject_class->get_property = ul_logical_volume_object_get_property;

  /**
   * UlLogicalVolumeObject:name:
   *
   * The name of the logical volume.
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

 /**
   * UlLogicalVolumeObject:volume_group:
   *
   * The volume group.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VOLUME_GROUP,
                                   g_param_spec_object ("volumegroup",
                                                        "Volume Group",
                                                        "The volume group",
                                                        UL_TYPE_VOLUME_GROUP_OBJECT,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

/**
 * ul_logical_volume_object_new:
 *
 * Create a new LogicalVolume object.
 *
 * Returns: A #UlLogicalVolumeObject object. Free with g_object_unref().
 */
UlLogicalVolumeObject *
ul_logical_volume_object_new (UlVolumeGroupObject *volume_group,
                              const gchar *name)
{
  g_return_val_if_fail (UL_IS_VOLUME_GROUP_OBJECT (volume_group), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (UL_TYPE_LOGICAL_VOLUME_OBJECT,
                       "volumegroup", volume_group,
                       "name", name,
                       NULL);
}

UlVolumeGroupObject *
ul_logical_volume_object_get_volume_group (UlLogicalVolumeObject *self)
{
  g_return_val_if_fail (UL_IS_LOGICAL_VOLUME_OBJECT (self), NULL);
  return self->volume_group;
}

const gchar *
ul_logical_volume_object_get_name (UlLogicalVolumeObject *self)
{
  g_return_val_if_fail (UL_IS_LOGICAL_VOLUME_OBJECT (self), NULL);
  return self->name;
}

void
ul_logical_volume_object_update (UlLogicalVolumeObject *self,
                                 GVariant *info,
                                 gboolean *needs_polling_ret)
{
  g_return_if_fail (UL_IS_LOGICAL_VOLUME_OBJECT (self));

  ul_logical_volume_update (UL_LOGICAL_VOLUME (self->iface_logical_volume),
                            self->volume_group,
                            info,
                            needs_polling_ret);
}
