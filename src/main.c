/*
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
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
 */

#include "config.h"

#include "daemon.h"

#include "util.h"

#include <glib/gi18n.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include <syslog.h>

/* ---------------------------------------------------------------------------------------------------- */

static GMainLoop *loop = NULL;
static gboolean opt_replace = FALSE;
static gboolean opt_debug = FALSE;
static GOptionEntry opt_entries[] =
{
  {"replace", 'r', 0, G_OPTION_ARG_NONE, &opt_replace, "Replace existing daemon", NULL},
  {"debug", 'd', 0, G_OPTION_ARG_NONE, &opt_debug, "Print debug information on stderr", NULL},
  {NULL }
};

static UlDaemon *the_daemon = NULL;

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
  the_daemon = g_object_new (UL_TYPE_DAEMON, "connection", connection, NULL);
  g_debug ("Connected to the system bus");
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
  if (the_daemon == NULL)
    {
      g_critical ("Failed to connect to the system message bus");
    }
  else
    {
      g_info ("Lost (or failed to acquire) the name %s on the system message bus", name);
    }
  g_main_loop_quit (loop);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{
  g_info ("Acquired the name %s on the system message bus", name);
}

static gboolean
on_sigint (gpointer user_data)
{
  g_info ("Caught SIGINT. Initiating shutdown");
  g_main_loop_quit (loop);
  return FALSE;
}

static void
log_handler (const gchar *log_domain,
                GLogLevelFlags log_level,
                const gchar *message,
                gpointer user_data)
{
  static gboolean have_called_openlog = FALSE;
  const gchar *domains;
  int priority;

  if (!have_called_openlog)
    {
      have_called_openlog = TRUE;
      openlog (G_LOG_DOMAIN, LOG_CONS | LOG_NDELAY | LOG_PID, LOG_DAEMON);
    }

  /*
   * Note: we should not call GLib fucntions here.
   *
   * Mapping glib log levels to syslog priorities
   * is not at all obvious.
   */

  switch (log_level & G_LOG_LEVEL_MASK)
    {
    /*
     * In GLib this is always fatal, caller of this
     * function aborts()
     */
    case G_LOG_LEVEL_ERROR:
      priority = LOG_CRIT;
      break;

    /*
     * By convention in GLib applications, critical warnings
     * are usually internal programmer error (ie: precondition
     * failures). This maps well to LOG_CRIT.
     */
    case G_LOG_LEVEL_CRITICAL:
      priority = LOG_CRIT;
      break;

    /*
     * By convention in GLib apps, g_warning() is used for
     * non-fatal problems, but ones that should be corrected
     * or not be encountered in normal system behavior.
     */
    case G_LOG_LEVEL_WARNING:
      priority = LOG_ERR;
      break;

    /*
     * These are related to bad input, or other hosts behaving
     * badly. Map well to syslog warnings.
     */
    case G_LOG_LEVEL_MESSAGE:
    default:
      priority = LOG_WARNING;
      break;

    /* Informational messages, startup, shutdown etc. */
    case G_LOG_LEVEL_INFO:
      priority = LOG_INFO;
      break;

    /* Debug messages. */
    case G_LOG_LEVEL_DEBUG:
      domains = g_getenv ("G_MESSAGES_DEBUG");
      if (domains == NULL ||
          (strcmp (domains, "all") != 0 && (!log_domain || !strstr (domains, log_domain))))
        return;

      priority = LOG_INFO;
      break;
    }

  syslog (priority, "%s", message);
}

int
main (int argc,
      char **argv)
{
  GError *error;
  GOptionContext *opt_context;
  gint ret;
  guint name_owner_id;

  ret = 1;
  loop = NULL;
  opt_context = NULL;
  name_owner_id = 0;

#if !GLIB_CHECK_VERSION(2,36,0)
  g_type_init ();
#endif

  /* avoid gvfs (http://bugzilla.gnome.org/show_bug.cgi?id=526454) */
  if (!g_setenv ("GIO_USE_VFS", "local", TRUE))
    {
      g_printerr ("Error setting GIO_USE_GVFS\n");
      goto out;
    }

  opt_context = g_option_context_new ("udisks-extra storage daemon");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  error = NULL;
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  if (!opt_debug)
    g_log_set_default_handler (log_handler, NULL);

  if (g_getenv ("PATH") == NULL)
    g_setenv ("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", TRUE);

  g_info ("udisks-lvm version %s starting", PACKAGE_VERSION);

  loop = g_main_loop_new (NULL, FALSE);

  g_unix_signal_add (SIGINT, on_sigint, NULL);
  g_unix_signal_add (SIGTERM, on_sigint, NULL);

  name_owner_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
                                  "com.redhat.lvm2",
                                  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                    (opt_replace ? G_BUS_NAME_OWNER_FLAGS_REPLACE : 0),
                                  on_bus_acquired,
                                  on_name_acquired,
                                  on_name_lost,
                                  NULL,
                                  NULL);


  g_debug ("Entering main event loop");

  g_main_loop_run (loop);

  ret = 0;

 out:
  if (the_daemon != NULL)
    g_object_unref (the_daemon);
  if (name_owner_id != 0)
    g_bus_unown_name (name_owner_id);
  if (loop != NULL)
    g_main_loop_unref (loop);
  if (opt_context != NULL)
    g_option_context_free (opt_context);

  g_info ("udisks-lvm version %s exiting", PACKAGE_VERSION);

  return ret;
}
