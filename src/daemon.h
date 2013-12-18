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

#ifndef __UL_DAEMON_H__
#define __UL_DAEMON_H__

#include <gio/gio.h>

#include "com.redhat.lvm2.h"

#include "job.h"

G_BEGIN_DECLS

#define UL_TYPE_DAEMON         (ul_daemon_get_type ())
#define UL_DAEMON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_DAEMON, UlDaemon))
#define UL_IS_DAEMON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_DAEMON))

typedef struct _UlBlock UlBlock;
typedef struct _UlDaemon UlDaemon;
typedef struct _UlManager UlManager;

GType                      ul_daemon_get_type            (void) G_GNUC_CONST;

UlDaemon *                 ul_daemon_get                 (void);

UlBlock *                  ul_daemon_find_block          (UlDaemon *self,
                                                          dev_t block_device_number);

gpointer                   ul_daemon_find_thing          (UlDaemon *self,
                                                          const gchar *object_path,
                                                          GType type_of_thing);

GList *                    ul_daemon_get_jobs            (UlDaemon *self);

GList *                    ul_daemon_get_blocks          (UlDaemon *self);

UlManager *                ul_daemon_get_manager         (UlDaemon *self);

gchar *                    ul_daemon_get_resource_path   (UlDaemon *self,
                                                          gboolean arch_specific,
                                                          const gchar *path);

void                       ul_daemon_publish             (UlDaemon *self,
                                                          const gchar *path,
                                                          gboolean uniquely,
                                                          gpointer thing);

void                       ul_daemon_unpublish           (UlDaemon *self,
                                                          const gchar *path,
                                                          gpointer thing);

UlJob *                    ul_daemon_launch_spawned_job  (UlDaemon *self,
                                                          gpointer object_or_interface,
                                                          const gchar *job_operation,
                                                          uid_t job_started_by_uid,
                                                          GCancellable *cancellable,
                                                          uid_t run_as_uid,
                                                          uid_t run_as_euid,
                                                          const gchar *input_string,
                                                          const gchar *first_arg,
                                                          ...) G_GNUC_NULL_TERMINATED;

UlJob *                    ul_daemon_launch_spawned_jobv (UlDaemon *self,
                                                          gpointer object_or_interface,
                                                          const gchar *job_operation,
                                                          uid_t job_started_by_uid,
                                                          GCancellable *cancellable,
                                                          uid_t run_as_uid,
                                                          uid_t run_as_euid,
                                                          const gchar *input_string,
                                                          const gchar **argv);

UlJob *                    ul_daemon_launch_threaded_job (UlDaemon *daemon,
                                                          gpointer object_or_interface,
                                                          const gchar *job_operation,
                                                          uid_t job_started_by_uid,
                                                          UlJobFunc job_func,
                                                          gpointer user_data,
                                                          GDestroyNotify user_data_free_func,
                                                          GCancellable *cancellable);

GPid                       ul_daemon_spawn_for_variant   (UlDaemon *self,
                                                          const gchar **argv,
                                                          const GVariantType *type,
                                                          void (*callback) (GPid, GVariant *, GError *, gpointer),
                                                          gpointer user_data);

G_END_DECLS

#endif /* __UL_DAEMON_H__ */
