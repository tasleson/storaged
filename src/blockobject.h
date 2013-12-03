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

#include "volumegroupobject.h"

G_BEGIN_DECLS

#define UL_TYPE_BLOCK_OBJECT         (ul_block_object_get_type ())
#define UL_BLOCK_OBJECT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_BLOCK_OBJECT, UlBlockObject))
#define UL_IS_BLOCK_OBJECT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_BLOCK_OBJECT))

typedef struct _UlBlockObject UlBlockObject;

GType              ul_block_object_get_type         (void) G_GNUC_CONST;

GUdevDevice *      ul_block_object_get_udev         (UlBlockObject *self);

const gchar *      ul_block_object_get_device       (UlBlockObject *self);

const gchar **     ul_block_object_get_symlinks     (UlBlockObject *self);

const gchar *      ul_block_object_get_id_type      (UlBlockObject *self);

const gchar *      ul_block_object_get_id_usage     (UlBlockObject *self);

const gchar *      ul_block_object_get_id_version   (UlBlockObject *self);

const gchar *      ul_block_object_get_id_label     (UlBlockObject *self);

const gchar *      ul_block_object_get_id_uuid      (UlBlockObject *self);

void               ul_block_object_trigger_uevent   (UlBlockObject *self);

gboolean           ul_block_object_is_unused        (UlBlockObject *self,
                                                     GError **error);

gboolean           ul_block_object_wipe             (UlBlockObject *self,
                                                     GError **error);

void               ul_block_object_update_lv        (UlBlockObject *self,
                                                     UlLogicalVolumeObject *lv);

void               ul_block_object_update_pv        (UlBlockObject *self,
                                                     UlVolumeGroupObject *group_object,
                                                     GVariant *pv_info);

G_END_DECLS

#endif /* __UL_BLOCK_OBJECT_H__ */
