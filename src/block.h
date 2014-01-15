/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
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
 *
 */

#ifndef __UL_BLOCK_OBJECT_H__
#define __UL_BLOCK_OBJECT_H__

#include <gio/gio.h>

#include <gudev/gudev.h>

#include "types.h"

G_BEGIN_DECLS

#define UL_TYPE_BLOCK         (ul_block_get_type ())
#define UL_BLOCK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_BLOCK, UlBlock))
#define UL_IS_BLOCK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_BLOCK))

GType              ul_block_get_type         (void) G_GNUC_CONST;

const gchar *      ul_block_get_object_path  (UlBlock *self);

GUdevDevice *      ul_block_get_udev         (UlBlock *self);

const gchar *      ul_block_get_device       (UlBlock *self);

const gchar **     ul_block_get_symlinks     (UlBlock *self);

const gchar *      ul_block_get_id_type      (UlBlock *self);

const gchar *      ul_block_get_id_usage     (UlBlock *self);

const gchar *      ul_block_get_id_version   (UlBlock *self);

const gchar *      ul_block_get_id_label     (UlBlock *self);

const gchar *      ul_block_get_id_uuid      (UlBlock *self);

void               ul_block_trigger_uevent   (UlBlock *self);

gboolean           ul_block_is_unused        (UlBlock *self,
                                              GError **error);

void               ul_block_update_lv        (UlBlock *self,
                                              UlLogicalVolume *lv);

void               ul_block_update_pv        (UlBlock *self,
                                              UlVolumeGroup *group,
                                              GVariant *pv_info);

LvmLogicalVolumeBlock  *ul_block_get_logical_volume_block  (UlBlock *self);

LvmPhysicalVolumeBlock *ul_block_get_physical_volume_block (UlBlock *self);

G_END_DECLS

#endif /* __UL_BLOCK_OBJECT_H__ */
