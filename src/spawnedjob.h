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

#ifndef __UL_SPAWNED_JOB_H__
#define __UL_SPAWNED_JOB_H__

#include <gio/gio.h>

#include "daemon.h"

G_BEGIN_DECLS

#define UL_TYPE_SPAWNED_JOB         (ul_spawned_job_get_type ())
#define UL_SPAWNED_JOB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_SPAWNED_JOB, UlSpawnedJob))
#define UL_IS_SPAWNED_JOB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_SPAWNED_JOB))

typedef struct _UlSpawnedJob UlSpawnedJob;

GType              ul_spawned_job_get_type         (void) G_GNUC_CONST;

UlSpawnedJob  *    ul_spawned_job_new              (const gchar *command_line,
                                                    const gchar *input_string,
                                                    uid_t run_as_uid,
                                                    uid_t run_as_euid,
                                                    UlDaemon *daemon,
                                                    GCancellable *cancellable);

const gchar       *ul_spawned_job_get_command_line (UlSpawnedJob *job);

G_END_DECLS

#endif /* __UL_SPAWNED_JOB_H__ */
