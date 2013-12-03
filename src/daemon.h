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

#include "basejob.h"

G_BEGIN_DECLS

#define UL_TYPE_DAEMON         (ul_daemon_get_type ())
#define UL_DAEMON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_DAEMON, UlDaemon))
#define UL_IS_DAEMON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_DAEMON))

typedef struct _UlDaemon UlDaemon;
typedef struct _UlManager UlManager;

GType                      ul_daemon_get_type            (void) G_GNUC_CONST;

UlDaemon *                 ul_daemon_get                 (void);

GDBusObjectManagerServer * ul_daemon_get_object_manager  (UlDaemon *self);

GList *                    ul_daemon_get_objects         (UlDaemon *self);

LvmObject *                ul_daemon_find_block          (UlDaemon *self,
                                                          dev_t block_device_number);

GDBusObject *              ul_daemon_find_object         (UlDaemon *self,
                                                          const gchar *object_path);

UlManager *                ul_daemon_get_manager         (UlDaemon *self);

/**
 * UlDaemonWaitFunc:
 * @daemon: A #UlDaemon.
 * @user_data: The #gpointer passed to ul_daemon_wait_for_object_sync().
 *
 * Type for callback function used with ul_daemon_wait_for_object_sync().
 *
 * Returns: (transfer full): %NULL if the object to wait for was not found,
 *          otherwise a full reference to a #GDBusObject.
 */
typedef GDBusObject * (* UlDaemonWaitFunc)               (UlDaemon *daemon,
                                                          gpointer user_data);

GDBusObject *         ul_daemon_wait_for_object_sync     (UlDaemon *self,
                                                          UlDaemonWaitFunc wait_func,
                                                          gpointer user_data,
                                                          GDestroyNotify user_data_free_func,
                                                          guint timeout_seconds,
                                                          GError **error);

UlBaseJob *           ul_daemon_launch_spawned_job       (UlDaemon *self,
                                                          LvmObject *object,
                                                          const gchar *job_operation,
                                                          uid_t job_started_by_uid,
                                                          GCancellable *cancellable,
                                                          uid_t run_as_uid,
                                                          uid_t run_as_euid,
                                                          const gchar *input_string,
                                                          const gchar *command_line_format,
                                                          ...) G_GNUC_PRINTF (9, 10);

gboolean              ul_daemon_launch_spawned_job_sync  (UlDaemon *self,
                                                          LvmObject *object,
                                                          const gchar *job_operation,
                                                          uid_t job_started_by_uid,
                                                          GCancellable *cancellable,
                                                          uid_t run_as_uid,
                                                          uid_t run_as_euid,
                                                          gint *out_status,
                                                          gchar **out_message,
                                                          const gchar *input_string,
                                                          const gchar *command_line_format,
                                                          ...) G_GNUC_PRINTF (11, 12);

gboolean              ul_daemon_check_authorization_sync (UlDaemon *self,
                                                          LvmObject *object,
                                                          const gchar *action_id,
                                                          GVariant *options,
                                                          const gchar *message,
                                                          GDBusMethodInvocation *invocation);

gboolean              ul_daemon_get_caller_uid_sync      (UlDaemon *self,
                                                          GDBusMethodInvocation *invocation,
                                                          GCancellable *cancellable,
                                                          uid_t *out_uid,
                                                          gid_t *out_gid,
                                                          gchar **out_user_name,
                                                          GError **error);

gboolean              ul_daemon_get_caller_pid_sync      (UlDaemon *self,
                                                          GDBusMethodInvocation *invocation,
                                                          GCancellable *cancellable,
                                                          pid_t *out_pid,
                                                          GError **error);

G_END_DECLS

#endif /* __UL_DAEMON_H__ */
