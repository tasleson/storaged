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

#include "basejob.h"
#include "blockobject.h"
#include "daemon.h"
#include "manager.h"
#include "spawnedjob.h"
#include "volumegroupobject.h"

#include <glib/gi18n-lib.h>

#include <polkit/polkit.h>

#include <pwd.h>
#include <stdio.h>

/**
 * SECTION:udisksdaemon
 * @title: UlDaemon
 * @short_description: Main daemon object
 *
 * Object holding all global state.
 */

/* The only daemon that should be created is tracked here */
static UlDaemon *default_daemon = NULL;

typedef struct _UlDaemonClass   UlDaemonClass;

/**
 * UlDaemon:
 *
 * The #UlDaemon structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _UlDaemon
{
  GObject parent_instance;
  GDBusConnection *connection;
  GDBusObjectManagerServer *object_manager;
  UlManager *manager;

  /* may be NULL if polkit is masked */
  PolkitAuthority *authority;

  /* The libdir if overridden */
  gchar *resource_dir;
};

struct _UlDaemonClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_OBJECT_MANAGER,
  PROP_RESOURCE_DIR,
};

G_DEFINE_TYPE (UlDaemon, ul_daemon, G_TYPE_OBJECT);

static void
ul_daemon_finalize (GObject *object)
{
  UlDaemon *self = UL_DAEMON (object);

  default_daemon = NULL;

  g_clear_object (&self->authority);
  g_object_unref (self->object_manager);
  g_object_unref (self->connection);
  g_object_unref (self->manager);
  g_free (self->resource_dir);

  G_OBJECT_CLASS (ul_daemon_parent_class)->finalize (object);
}

static void
ul_daemon_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  UlDaemon *self = UL_DAEMON (object);

  switch (prop_id)
    {
    case PROP_OBJECT_MANAGER:
      g_value_set_object (value, ul_daemon_get_object_manager (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ul_daemon_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  UlDaemon *self = UL_DAEMON (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_assert (self->connection == NULL);
      self->connection = g_value_dup_object (value);
      break;

    case PROP_RESOURCE_DIR:
      g_assert (self->resource_dir == NULL);
      self->resource_dir = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ul_daemon_init (UlDaemon *self)
{
  g_assert (default_daemon == NULL);
  default_daemon = self;
}

static void
ul_daemon_constructed (GObject *object)
{
  UlDaemon *self = UL_DAEMON (object);
  GDBusObjectSkeleton *skeleton;
  GError *error;

  G_OBJECT_CLASS (ul_daemon_parent_class)->constructed (object);

  error = NULL;
  self->authority = polkit_authority_get_sync (NULL, &error);
  if (self->authority == NULL)
    {
      g_warning ("Error initializing polkit authority: %s (%s, %d)",
                 error->message, g_quark_to_string (error->domain), error->code);
      g_error_free (error);
    }

  /* Yes, we use the same paths as the main udisks daemon on purpose */
  self->object_manager = g_dbus_object_manager_server_new ("/org/freedesktop/UDisks2");

  /* Export the ObjectManager */
  g_dbus_object_manager_server_set_connection (self->object_manager, self->connection);

  self->manager = ul_manager_new ();

  skeleton = G_DBUS_OBJECT_SKELETON (lvm_object_skeleton_new ("/org/freedesktop/UDisks2/Manager"));
  g_dbus_object_skeleton_add_interface (skeleton, G_DBUS_INTERFACE_SKELETON (self->manager));
  g_dbus_object_manager_server_export (self->object_manager, skeleton);
  g_object_unref (skeleton);
}

static void
ul_daemon_class_init (UlDaemonClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ul_daemon_finalize;
  gobject_class->constructed = ul_daemon_constructed;
  gobject_class->set_property = ul_daemon_set_property;
  gobject_class->get_property = ul_daemon_get_property;

  /**
   * UlDaemon:connection:
   *
   * The #GDBusConnection the daemon is for.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        "Connection",
                                                        "The D-Bus connection the daemon is for",
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * UlDaemon:object-manager:
   *
   * The #GDBusObjectManager used by the daemon
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_MANAGER,
                                   g_param_spec_object ("object-manager",
                                                        "Object Manager",
                                                        "The D-Bus Object Manager server used by the daemon",
                                                        G_TYPE_DBUS_OBJECT_MANAGER_SERVER,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_RESOURCE_DIR,
                                   g_param_spec_string ("resource-dir",
                                                        "Resource Directory",
                                                        "Override directory to use resources from",
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

UlDaemon *
ul_daemon_get (void)
{
  g_assert (default_daemon != NULL);
  return default_daemon;
}

GDBusObjectManagerServer *
ul_daemon_get_object_manager (UlDaemon *self)
{
  g_return_val_if_fail (UL_IS_DAEMON (self), NULL);
  return self->object_manager;
}

UlManager *
ul_daemon_get_manager (UlDaemon *self)
{
  g_return_val_if_fail (UL_IS_DAEMON (self), NULL);
  return self->manager;
}

gchar *
ul_daemon_get_resource_path (UlDaemon *self,
                             gboolean arch_specific,
                             const gchar *file)
{
  g_return_val_if_fail (UL_IS_DAEMON (self), NULL);

  if (self->resource_dir)
      return g_build_filename (self->resource_dir, file, NULL);
  else if (arch_specific)
      return g_build_filename (PACKAGE_LIB_DIR, "udisks2", file, NULL);
  else
      return g_build_filename (PACKAGE_DATA_DIR, "udisks2", file, NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_job_completed (UDisksJob *job,
                  gboolean success,
                  const gchar *message,
                  gpointer user_data)
{
  UlDaemon *self = UL_DAEMON (user_data);
  UDisksObjectSkeleton *object;

  object = UDISKS_OBJECT_SKELETON (g_dbus_interface_get_object (G_DBUS_INTERFACE (job)));
  g_assert (object != NULL);

  /* Unexport job */
  g_dbus_object_manager_server_unexport (self->object_manager,
                                         g_dbus_object_get_object_path (G_DBUS_OBJECT (object)));
  g_object_unref (object);

  /* free the allocated job object */
  g_object_unref (job);

  /* returns the reference we took when connecting to the
   * UDisksJob::completed signal in ul_daemon_launch_{spawned,threaded}_job()
   * below
   */
  g_object_unref (self);
}

/* ---------------------------------------------------------------------------------------------------- */

static guint job_id = 0;

/* ---------------------------------------------------------------------------------------------------- */

/**
 * ul_daemon_launch_spawned_job:
 * @self: A #UlDaemon.
 * @object: (allow-none): A #LvmObject to add to the job or %NULL.
 * @job_operation: The operation for the job.
 * @job_started_by_uid: The user who started the job.
 * @cancellable: A #GCancellable or %NULL.
 * @run_as_uid: The #uid_t to run the command as.
 * @run_as_euid: The effective #uid_t to run the command as.
 * @input_string: A string to write to stdin of the spawned program or %NULL.
 * @command_line_format: printf()-style format for the command line to spawn.
 * @...: Arguments for @command_line_format.
 *
 * Launches a new job for @command_line_format.
 *
 * The job is started immediately - connect to the
 * #UDisksSpawnedJob::spawned-job-completed or #UDisksJob::completed
 * signals to get notified when the job is done.
 *
 * The returned object will be exported on the bus until the
 * #UDisksJob::completed signal is emitted on the object. It is not
 * valid to use the returned object after this signal fires.
 *
 * Returns: A #UDisksSpawnedJob object. Do not free, the object
 * belongs to @manager.
 */
UlBaseJob *
ul_daemon_launch_spawned_job (UlDaemon *self,
                              LvmObject *object,
                              const gchar *job_operation,
                              uid_t job_started_by_uid,
                              GCancellable *cancellable,
                              uid_t run_as_uid,
                              uid_t run_as_euid,
                              const gchar *input_string,
                              const gchar *command_line_format,
                              ...)
{
  va_list var_args;
  gchar *command_line;
  UlSpawnedJob *job;
  GDBusObjectSkeleton *job_object;
  gchar *job_object_path;

  g_return_val_if_fail (UL_IS_DAEMON (self), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (command_line_format != NULL, NULL);

  va_start (var_args, command_line_format);
  command_line = g_strdup_vprintf (command_line_format, var_args);
  va_end (var_args);
  job = ul_spawned_job_new (command_line, input_string, run_as_uid, run_as_euid, self, cancellable);
  g_free (command_line);

  if (object != NULL)
    ul_base_job_add_object (UL_BASE_JOB (job), object);

  /* TODO: protect job_id by a mutex */
  job_object_path = g_strdup_printf ("/org/freedesktop/UDisks2/jobs/%d", job_id++);
  job_object = g_dbus_object_skeleton_new (job_object_path);
  g_dbus_object_skeleton_add_interface (job_object, G_DBUS_INTERFACE_SKELETON (job));
  g_free (job_object_path);

  udisks_job_set_cancelable (UDISKS_JOB (job), TRUE);
  udisks_job_set_operation (UDISKS_JOB (job), job_operation);
  udisks_job_set_started_by_uid (UDISKS_JOB (job), job_started_by_uid);

  g_dbus_object_manager_server_export (self->object_manager, G_DBUS_OBJECT_SKELETON (job_object));
  g_signal_connect_after (job,
                          "completed",
                          G_CALLBACK (on_job_completed),
                          g_object_ref (self));

  return UL_BASE_JOB (job);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainContext *context;
  GMainLoop *loop;
  gboolean success;
  gint status;
  gchar *message;
} SpawnedJobSyncData;

static gboolean
spawned_job_sync_on_spawned_job_completed (UlSpawnedJob *job,
                                           GError *error,
                                           gint status,
                                           GString *standard_output,
                                           GString *standard_error,
                                           gpointer user_data)
{
  SpawnedJobSyncData *data = user_data;
  data->status = status;
  return FALSE; /* let other handlers run */
}

static void
spawned_job_sync_on_completed (UDisksJob *job,
                               gboolean success,
                               const gchar *message,
                               gpointer user_data)
{
  SpawnedJobSyncData *data = user_data;
  data->success = success;
  data->message = g_strdup (message);
  g_main_loop_quit (data->loop);
}

/**
 * ul_daemon_launch_spawned_job_sync:
 * @self: A #UlDaemon.
 * @object: (allow-none): A #LvmObject to add to the job or %NULL.
 * @job_operation: The operation for the job.
 * @job_started_by_uid: The user who started the job.
 * @cancellable: A #GCancellable or %NULL.
 * @run_as_uid: The #uid_t to run the command as.
 * @run_as_euid: The effective #uid_t to run the command as.
 * @input_string: A string to write to stdin of the spawned program or %NULL.
 * @out_status: Return location for the @status parameter of the #UDisksSpawnedJob::spawned-job-completed signal.
 * @out_message: Return location for the @message parameter of the #UDisksJob::completed signal.
 * @command_line_format: printf()-style format for the command line to spawn.
 * @...: Arguments for @command_line_format.
 *
 * Like ul_daemon_launch_spawned_job() but blocks the calling
 * thread until the job completes.
 *
 * Returns: The @success parameter of the #UDisksJob::completed signal.
 */
gboolean
ul_daemon_launch_spawned_job_sync (UlDaemon *self,
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
                                   ...)
{
  va_list var_args;
  gchar *command_line;
  UlBaseJob *job;
  SpawnedJobSyncData data;

  g_return_val_if_fail (UL_IS_DAEMON (self), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (command_line_format != NULL, FALSE);

  data.context = g_main_context_new ();
  g_main_context_push_thread_default (data.context);
  data.loop = g_main_loop_new (data.context, FALSE);
  data.success = FALSE;
  data.status = 0;
  data.message = NULL;

  va_start (var_args, command_line_format);
  command_line = g_strdup_vprintf (command_line_format, var_args);
  va_end (var_args);
  job = ul_daemon_launch_spawned_job (self,
                                      object,
                                      job_operation,
                                      job_started_by_uid,
                                      cancellable,
                                      run_as_uid,
                                      run_as_euid,
                                      input_string,
                                      "%s",
                                      command_line);
  g_signal_connect (job,
                    "spawned-job-completed",
                    G_CALLBACK (spawned_job_sync_on_spawned_job_completed),
                    &data);
  g_signal_connect_after (job,
                          "completed",
                          G_CALLBACK (spawned_job_sync_on_completed),
                          &data);

  g_main_loop_run (data.loop);

  if (out_status != NULL)
    *out_status = data.status;

  if (out_message != NULL)
    *out_message = data.message;
  else
    g_free (data.message);

  g_free (command_line);
  g_main_loop_unref (data.loop);
  g_main_context_pop_thread_default (data.context);
  g_main_context_unref (data.context);

  /* note: the job object is freed in the ::completed handler */

  return data.success;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * ul_daemon_wait_for_object_sync:
 * @self: A #UlDaemon.
 * @wait_func: Function to check for desired object.
 * @user_data: User data to pass to @wait_func.
 * @user_data_free_func: (allow-none): Function to free @user_data or %NULL.
 * @timeout_seconds: Maximum time to wait for the object (in seconds) or 0 to never wait.
 * @error: (allow-none): Return location for error or %NULL.
 *
 * Blocks the calling thread until an object picked by @wait_func is
 * available or until @timeout_seconds has passed (in which case the
 * function fails with %UDISKS_ERROR_TIMED_OUT).
 *
 * Note that @wait_func will be called from time to time - for example
 * if there is a device event.
 *
 * Returns: (transfer full): The object picked by @wait_func or %NULL if @error is set.
 */

typedef struct {
  GMainContext *context;
  GMainLoop *loop;
  gboolean timed_out;
} WaitData;

static gboolean
wait_on_timed_out (gpointer user_data)
{
  WaitData *data = user_data;
  data->timed_out = TRUE;
  g_main_loop_quit (data->loop);
  return FALSE; /* remove the source */
}

static gboolean
wait_on_recheck (gpointer user_data)
{
  WaitData *data = user_data;
  g_main_loop_quit (data->loop);
  return FALSE; /* remove the source */
}

GDBusObject *
ul_daemon_wait_for_object_sync (UlDaemon *self,
                                UlDaemonWaitFunc wait_func,
                                gpointer user_data,
                                GDestroyNotify user_data_free_func,
                                guint timeout_seconds,
                                GError **error)
{
  GDBusObject *ret;
  WaitData data;

  /* TODO: support GCancellable */

  g_return_val_if_fail (UL_IS_DAEMON (self), NULL);
  g_return_val_if_fail (wait_func != NULL, NULL);

  ret = NULL;

  memset (&data, '\0', sizeof (data));
  data.context = NULL;
  data.loop = NULL;

  g_object_ref (self);

 again:
  ret = wait_func (self, user_data);

  if (ret == NULL && timeout_seconds > 0)
    {
      GSource *source;

      /* sit and wait for up to @timeout_seconds if the object isn't there already */
      if (data.context == NULL)
        {
          /* TODO: this will deadlock if we are calling from the main thread... */
          data.context = g_main_context_new ();
          data.loop = g_main_loop_new (data.context, FALSE);

          source = g_timeout_source_new_seconds (timeout_seconds);
          g_source_set_priority (source, G_PRIORITY_DEFAULT);
          g_source_set_callback (source, wait_on_timed_out, &data, NULL);
          g_source_attach (source, data.context);
          g_source_unref (source);
        }

      /* TODO: do something a bit more elegant than checking every 250ms ... it's
       *       probably going to involve having each UDisksProvider emit a "changed"
       *       signal when it's time to recheck... for now this works.
       */
      source = g_timeout_source_new (250);
      g_source_set_priority (source, G_PRIORITY_DEFAULT);
      g_source_set_callback (source, wait_on_recheck, &data, NULL);
      g_source_attach (source, data.context);
      g_source_unref (source);

      g_main_loop_run (data.loop);

      if (data.timed_out)
        {
          g_set_error (error,
                       UDISKS_ERROR, UDISKS_ERROR_FAILED,
                       "Timed out waiting for object");
        }
      else
        {
          goto again;
        }
    }

  if (user_data_free_func != NULL)
    user_data_free_func (user_data);

  g_object_unref (self);

  if (data.loop != NULL)
    g_main_loop_unref (data.loop);
  if (data.context != NULL)
    g_main_context_unref (data.context);

  return ret;
}

GDBusObject *
ul_daemon_find_object (UlDaemon *daemon,
                       const gchar *object_path)
{
  return g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (daemon->object_manager),
                                           object_path);
}

GList *
ul_daemon_get_objects (UlDaemon *daemon)
{
  return g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (daemon->object_manager));
}


/* Need this until we can depend on a libpolkit with this bugfix
 *
 * http://cgit.freedesktop.org/polkit/commit/?h=wip/js-rule-files&id=224f7b892478302dccbe7e567b013d3c73d376fd
 */
static void
_safe_polkit_details_insert (PolkitDetails *details, const gchar *key, const gchar *value)
{
  if (value != NULL && strlen (value) > 0)
    polkit_details_insert (details, key, value);
}

static gboolean
check_authorization_no_polkit (UlDaemon *daemon,
                               LvmObject *object,
                               const gchar *action_id,
                               GVariant *options,
                               const gchar *message,
                               GDBusMethodInvocation *invocation)
{
  gboolean ret = FALSE;
  uid_t caller_uid = -1;
  GError *error = NULL;

  if (!ul_daemon_get_caller_uid_sync (daemon,
                                      invocation,
                                      NULL,         /* GCancellable* */
                                      &caller_uid,
                                      NULL,         /* gid_t *out_gid */
                                      NULL,         /* gchar **out_user_name */
                                      &error))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             UDISKS_ERROR,
                                             UDISKS_ERROR_FAILED,
                                             "Error getting uid for caller with bus name %s: %s (%s, %d)",
                                             g_dbus_method_invocation_get_sender (invocation),
                                             error->message, g_quark_to_string (error->domain), error->code);
      g_clear_error (&error);
      goto out;
    }

  /* only allow root */
  if (caller_uid == 0)
    {
      ret = TRUE;
    }
  else
    {
      g_dbus_method_invocation_return_error_literal (invocation,
                                                     UDISKS_ERROR,
                                                     UDISKS_ERROR_NOT_AUTHORIZED,
                                                     "Not authorized to perform operation (polkit authority not available and caller is not uid 0)");
    }

 out:
  return ret;
}

/**
 * udisks_daemon_util_check_authorization_sync:
 * @daemon: A #UDisksDaemon.
 * @object: (allow-none): The #GDBusObject that the call is on or %NULL.
 * @action_id: The action id to check for.
 * @options: (allow-none): A #GVariant to check for the <quote>auth.no_user_interaction</quote> option or %NULL.
 * @message: The message to convey (use N_).
 * @invocation: The invocation to check for.
 *
 * Checks if the caller represented by @invocation is authorized for
 * the action identified by @action_id, optionally displaying @message
 * if authentication is needed. Additionally, if the caller is not
 * authorized, the appropriate error is already returned to the caller
 * via @invocation.
 *
 * The calling thread is blocked for the duration of the authorization
 * check which could be a very long time since it may involve
 * presenting an authentication dialog and having a human user use
 * it. If <quote>auth.no_user_interaction</quote> in @options is %TRUE
 * no authentication dialog will be presented and the check is not
 * expected to take a long time.
 *
 * See <xref linkend="udisks-polkit-details"/> for the variables that
 * can be used in @message but note that not all variables can be used
 * in all checks. For example, any check involving a #xUDisksDrive or a
 * #xUDisksBlock object can safely include the fragment
 * <quote>$(drive)</quote> since it will always expand to the name of
 * the drive, e.g. <quote>INTEL SSDSA2MH080G1GC (/dev/sda1)</quote> or
 * the block device file e.g. <quote>/dev/vg_lucifer/lv_root</quote>
 * or <quote>/dev/sda1</quote>. However this won't work for operations
 * that isn't on a drive or block device, for example calls on the
 * <link linkend="gdbus-interface-org-freedesktop-UDisks2-Manager.top_of_page">Manager</link>
 * object.
 *
 * Returns: %TRUE if caller is authorized, %FALSE if not.
 */
gboolean
ul_daemon_check_authorization_sync (UlDaemon *daemon,
                                    LvmObject *object,
                                    const gchar *action_id,
                                    GVariant *options,
                                    const gchar *message,
                                    GDBusMethodInvocation *invocation)
{
  PolkitSubject *subject = NULL;
  PolkitDetails *details = NULL;
  PolkitCheckAuthorizationFlags flags = POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE;
  PolkitAuthorizationResult *result = NULL;
  GError *error = NULL;
  gboolean ret = FALSE;
  UDisksPartition *partition = NULL;
  gboolean auth_no_user_interaction = FALSE;

  if (daemon->authority == NULL)
    {
      ret = check_authorization_no_polkit (daemon, object, action_id, options, message, invocation);
      goto out;
    }

  subject = polkit_system_bus_name_new (g_dbus_method_invocation_get_sender (invocation));
  if (options != NULL)
    {
      g_variant_lookup (options,
                        "auth.no_user_interaction",
                        "b",
                        &auth_no_user_interaction);
    }
  if (!auth_no_user_interaction)
    flags = POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION;

  details = polkit_details_new ();
  polkit_details_insert (details, "polkit.message", message);
  polkit_details_insert (details, "polkit.gettext_domain", "udisks2");

  /* Find drive associated with the block device, if any */
  if (UL_IS_BLOCK_OBJECT (object))
    {
      UlBlockObject *block = UL_BLOCK_OBJECT (object);
      _safe_polkit_details_insert (details, "id.type",    ul_block_object_get_id_type (block));
      _safe_polkit_details_insert (details, "id.usage",   ul_block_object_get_id_usage (block));
      _safe_polkit_details_insert (details, "id.version", ul_block_object_get_id_version (block));
      _safe_polkit_details_insert (details, "id.label",   ul_block_object_get_id_label (block));
      _safe_polkit_details_insert (details, "id.uuid",    ul_block_object_get_id_uuid (block));
    }

  if (UL_IS_VOLUME_GROUP_OBJECT (object))
    {
      UlVolumeGroupObject *group = UL_VOLUME_GROUP_OBJECT (object);
      _safe_polkit_details_insert (details, "lvm.volumegroup", ul_volume_group_object_get_name (group));
    }

  error = NULL;
  result = polkit_authority_check_authorization_sync (daemon->authority,
                                                      subject,
                                                      action_id,
                                                      details,
                                                      flags,
                                                      NULL, /* GCancellable* */
                                                      &error);
  if (result == NULL)
    {
      if (error->domain != POLKIT_ERROR)
        {
          /* assume polkit authority is not available (e.g. could be the service
           * manager returning org.freedesktop.systemd1.Masked)
           */
          g_error_free (error);
          ret = check_authorization_no_polkit (daemon, object, action_id, options, message, invocation);
        }
      else
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 UDISKS_ERROR,
                                                 UDISKS_ERROR_FAILED,
                                                 "Error checking authorization: %s (%s, %d)",
                                                 error->message,
                                                 g_quark_to_string (error->domain),
                                                 error->code);
          g_error_free (error);
        }
      goto out;
    }
  if (!polkit_authorization_result_get_is_authorized (result))
    {
      if (polkit_authorization_result_get_dismissed (result))
        g_dbus_method_invocation_return_error_literal (invocation,
                                                       UDISKS_ERROR,
                                                       UDISKS_ERROR_NOT_AUTHORIZED_DISMISSED,
                                                       "The authentication dialog was dismissed");
      else
        g_dbus_method_invocation_return_error_literal (invocation,
                                                       UDISKS_ERROR,
                                                       polkit_authorization_result_get_is_challenge (result) ?
                                                       UDISKS_ERROR_NOT_AUTHORIZED_CAN_OBTAIN :
                                                       UDISKS_ERROR_NOT_AUTHORIZED,
                                                       "Not authorized to perform operation");
      goto out;
    }

  ret = TRUE;

 out:
  g_clear_object (&partition);
  g_clear_object (&subject);
  g_clear_object (&details);
  g_clear_object (&result);
  return ret;
}

/**
 * ul_daemon_get_caller_uid_sync:
 * @daemon: A #UDisksDaemon.
 * @invocation: A #GDBusMethodInvocation.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @out_uid: (out): Return location for resolved uid or %NULL.
 * @out_gid: (out) (allow-none): Return location for resolved gid or %NULL.
 * @out_user_name: (out) (allow-none): Return location for resolved user name or %NULL.
 * @error: Return location for error.
 *
 * Gets the UNIX user id (and possibly group id and user name) of the
 * peer represented by @invocation.
 *
 * Returns: %TRUE if the user id (and possibly group id) was obtained, %FALSE otherwise
 */
gboolean
ul_daemon_get_caller_uid_sync (UlDaemon *daemon,
                               GDBusMethodInvocation *invocation,
                               GCancellable *cancellable,
                               uid_t *out_uid,
                               gid_t *out_gid,
                               gchar **out_user_name,
                               GError **error)
{
  gboolean ret;
  const gchar *caller;
  GVariant *value;
  GError *local_error;
  uid_t uid;

  /* TODO: cache this on @daemon */

  ret = FALSE;

  caller = g_dbus_method_invocation_get_sender (invocation);

  local_error = NULL;
  value = g_dbus_connection_call_sync (g_dbus_method_invocation_get_connection (invocation),
                                       "org.freedesktop.DBus",  /* bus name */
                                       "/org/freedesktop/DBus", /* object path */
                                       "org.freedesktop.DBus",  /* interface */
                                       "GetConnectionUnixUser", /* method */
                                       g_variant_new ("(s)", caller),
                                       G_VARIANT_TYPE ("(u)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1, /* timeout_msec */
                                       cancellable,
                                       &local_error);
  if (value == NULL)
    {
      g_set_error (error,
                   UDISKS_ERROR,
                   UDISKS_ERROR_FAILED,
                   "Error determining uid of caller %s: %s (%s, %d)",
                   caller,
                   local_error->message,
                   g_quark_to_string (local_error->domain),
                   local_error->code);
      g_error_free (local_error);
      goto out;
    }

  {
    G_STATIC_ASSERT (sizeof (uid_t) == sizeof (guint32));
  }

  g_variant_get (value, "(u)", &uid);
  if (out_uid != NULL)
    *out_uid = uid;

  if (out_gid != NULL || out_user_name != NULL)
    {
      struct passwd pwstruct;
      gchar pwbuf[8192];
      static struct passwd *pw;
      int rc;

      rc = getpwuid_r (uid, &pwstruct, pwbuf, sizeof pwbuf, &pw);
      if (rc == 0 && pw == NULL)
        {
          g_set_error (error,
                       UDISKS_ERROR,
                       UDISKS_ERROR_FAILED,
                       "User with uid %d does not exist", (gint) uid);
        }
      else if (pw == NULL)
        {
          g_set_error (error,
                       UDISKS_ERROR,
                       UDISKS_ERROR_FAILED,
                       "Error looking up passwd struct for uid %d: %m", (gint) uid);
          goto out;
        }
      if (out_gid != NULL)
        *out_gid = pw->pw_gid;
      if (out_user_name != NULL)
        *out_user_name = g_strdup (pwstruct.pw_name);
    }

  ret = TRUE;

 out:
  return ret;
}

struct VariantReaderData {
  const GVariantType *type;
  void (*callback) (GPid pid, GVariant *result, GError *error, gpointer user_data);
  gpointer user_data;
  GPid pid;
  GIOChannel *output_channel;
  GByteArray *output;
  gint output_watch;
};

static gboolean
variant_reader_child_output (GIOChannel *source,
                             GIOCondition condition,
                             gpointer user_data)
{
  struct VariantReaderData *data = user_data;
  guint8 buf[1024];
  gsize bytes_read;

  g_io_channel_read_chars (source, (gchar *)buf, sizeof buf, &bytes_read, NULL);
  g_byte_array_append (data->output, buf, bytes_read);
  return TRUE;
}

static void
variant_reader_watch_child (GPid     pid,
                            gint     status,
                            gpointer user_data)
{
  struct VariantReaderData *data = user_data;
  guint8 *buf;
  gsize buf_size;
  GVariant *result;
  GError *error = NULL;

  data->pid = 0;

  if (!g_spawn_check_exit_status (status, &error))
    {
      data->callback (pid, NULL, error, data->user_data);
      g_error_free (error);
      g_byte_array_free (data->output, TRUE);
    }
  else
    {
      if (g_io_channel_read_to_end (data->output_channel, (gchar **)&buf, &buf_size, NULL) == G_IO_STATUS_NORMAL)
        {
          g_byte_array_append (data->output, buf, buf_size);
          g_free (buf);
        }

      result = g_variant_new_from_data (data->type,
                                        data->output->data,
                                        data->output->len,
                                        TRUE,
                                        g_free, NULL);
      g_byte_array_free (data->output, FALSE);
      data->callback (pid, result, NULL, data->user_data);
      g_variant_unref (result);
    }
}

static void
variant_reader_destroy (gpointer user_data)
{
  struct VariantReaderData *data = user_data;

  g_source_remove (data->output_watch);
  g_io_channel_unref (data->output_channel);
  g_free (data);
}

GPid
ul_daemon_spawn_for_variant (UlDaemon *daemon,
                             const gchar **argv,
                             const GVariantType *type,
                             void (*callback) (GPid, GVariant *, GError *, gpointer),
                             gpointer user_data)
{
  GError *error;
  struct VariantReaderData *data;
  gchar *prog = NULL;
  GPid pid;
  gint output_fd;
  gchar *cmd;

  /*
   * This is so we can override the location of udisks-lvm-helper
   * during testing.
   */

  if (!strchr (argv[0], '/'))
    {
      prog = ul_daemon_get_resource_path (daemon, TRUE, argv[0]);
      argv[0] = prog;
    }

  cmd = g_strjoinv (" ", (gchar **)argv);
  g_debug ("spawning for variant: %s", cmd);
  g_free (cmd);

  if (!g_spawn_async_with_pipes (NULL,
                                 (gchar **)argv,
                                 NULL,
                                 G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL,
                                 NULL,
                                 &pid,
                                 NULL,
                                 &output_fd,
                                 NULL,
                                 &error))
    {
      callback (0, NULL, error, user_data);
      g_error_free (error);
      return 0;
    }

  data = g_new0 (struct VariantReaderData, 1);

  data->type = type;
  data->callback = callback;
  data->user_data = user_data;

  data->pid = pid;
  data->output = g_byte_array_new ();
  data->output_channel = g_io_channel_unix_new (output_fd);
  g_io_channel_set_encoding (data->output_channel, NULL, NULL);
  g_io_channel_set_flags (data->output_channel, G_IO_FLAG_NONBLOCK, NULL);
  data->output_watch = g_io_add_watch (data->output_channel, G_IO_IN, variant_reader_child_output, data);

  g_child_watch_add_full (G_PRIORITY_DEFAULT_IDLE,
                          pid, variant_reader_watch_child, data, variant_reader_destroy);

  g_free (prog);
  return pid;
}
