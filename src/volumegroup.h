/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
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

#ifndef __UL_VOLUME_GROUP_H__
#define __UL_VOLUME_GROUP_H__

#include "types.h"

G_BEGIN_DECLS

#define UL_TYPE_VOLUME_GROUP         (ul_volume_group_get_type ())
#define UL_VOLUME_GROUP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_VOLUME_GROUP, UlVolumeGroup))
#define UL_IS_VOLUME_GROUP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_VOLUME_GROUP))

GType              ul_volume_group_get_type            (void) G_GNUC_CONST;

UlVolumeGroup *    ul_volume_group_new                 (const gchar *name);

const gchar *      ul_volume_group_get_name            (UlVolumeGroup *self);

const gchar *      ul_volume_group_get_object_path     (UlVolumeGroup *self);

void               ul_volume_group_update              (UlVolumeGroup *self);

void               ul_volume_group_poll                (UlVolumeGroup *self);

UlLogicalVolume *  ul_volume_group_find_logical_volume (UlVolumeGroup *self,
                                                        const gchar *name);

void               ul_volume_group_update_block        (UlVolumeGroup *self,
                                                        UlBlock       *block);

G_END_DECLS

#endif /* __UL_VOLUME_GROUP_H__ */
