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

#ifndef __UL_LOGICAL_VOLUME_OBJECT_H__
#define __UL_LOGICAL_VOLUME_OBJECT_H__

#include "volumegroupobject.h"

G_BEGIN_DECLS

#define UL_TYPE_LOGICAL_VOLUME_OBJECT  (ul_logical_volume_object_get_type ())
#define UL_LOGICAL_VOLUME_OBJECT(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_LOGICAL_VOLUME_OBJECT, UlLogicalVolumeObject))
#define UL_IS_LOGICAL_VOLUME_OBJECT(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_LOGICAL_VOLUME_OBJECT))

GType                    ul_logical_volume_object_get_type         (void) G_GNUC_CONST;

UlLogicalVolumeObject *  ul_logical_volume_object_new              (UlVolumeGroupObject *vg_object,
                                                                    const gchar *name);

UlVolumeGroupObject *    ul_logical_volume_object_get_volume_group (UlLogicalVolumeObject *object);

const gchar *            ul_logical_volume_object_get_name         (UlLogicalVolumeObject *object);

void                     ul_logical_volume_object_update           (UlLogicalVolumeObject *object,
                                                                    GVariant *info,
                                                                    gboolean *needs_polling_ret);

G_END_DECLS

#endif /* __UL_LOGICAL_VOLUME_OBJECT_H__ */
