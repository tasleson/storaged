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

#ifndef __UL_TYPES_H__
#define __UL_TYPES_H__

#include "com.redhat.lvm2.h"

G_BEGIN_DECLS

typedef struct _UlBlock          UlBlock;
typedef struct _UlLogicalVolume  UlLogicalVolume;
typedef struct _UlPhysicalVolume UlPhysicalVolume;
typedef struct _UlVolumeGroup    UlVolumeGroup;
typedef struct _UlDaemon         UlDaemon;
typedef struct _UlManager        UlManager;
typedef struct _UlJob            UlJob;
typedef struct _UlSpawnedJob     UlSpawnedJob;
typedef struct _UlThreadedJob    UlThreadedJob;

G_END_DECLS

#endif /* __UL_DAEMON_H__ */
