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

#include "testing.h"

typedef struct {
  GDBusConnection *bus;
  gpointer daemon;
  GDBusObjectManager *objman;

  struct {
    gchar *device;
    gchar *object_path;
  } blocks[3];

  GDBusProxy *volume_group;
} Test;

static void
setup_target (Test *test,
              gconstpointer data)
{
  gchar *base;
  gchar *arg;
  guint i;

  testing_target_setup (&test->bus, &test->objman, &test->daemon);

  /* Create three raw disk files which we'll use */
  for (i = 0; i < G_N_ELEMENTS (test->blocks); i++)
    {
      base = g_strdup_printf ("test-udisk-lvm-%d", i);
      arg = g_strdup_printf ("of=%s", base);
      testing_target_execute (NULL, "dd", "if=/dev/zero", arg, "bs=1M", "count=50", "status=none", NULL);
      testing_target_execute (&test->blocks[i].device, "losetup", "-f", "--show", base, NULL);
      g_free (base);
      g_free (arg);

      g_strstrip (test->blocks[i].device);
      base = g_path_get_basename (test->blocks[i].device);

      /* Intelligent guess */
      test->blocks[i].object_path = g_strdup_printf ("/org/freedesktop/UDisks2/block_devices/%s", base);
    }
}

static void
teardown_target (Test *test,
                 gconstpointer data)
{
  guint i;

  testing_target_teardown (&test->bus, &test->objman, &test->daemon);

  for (i = 0; i < G_N_ELEMENTS (test->blocks); i++)
    {
      g_free (test->blocks[i].device);
      g_free (test->blocks[i].object_path);
    }
}

static void
ret_proxy_if_interface_matches (GDBusProxy *proxy,
                                const gchar *want_interface,
                                GDBusProxy **ret)
{
  if (g_str_equal (g_dbus_proxy_get_interface_name (proxy), want_interface))
    {
      g_assert (*ret == NULL);
      *ret = g_object_ref (proxy);
    }
}

static void
on_volume_group_added (GDBusObjectManager *objman,
                       GDBusObject *make_it_stop,
                       GDBusInterface *interface,
                       gpointer user_data)
{
  ret_proxy_if_interface_matches (G_DBUS_PROXY (interface),
                                  "com.redhat.lvm2.VolumeGroup", user_data);
}

static void
on_proxy_removed (GDBusObjectManager *objman,
                  GDBusObject *make_it_stop,
                  GDBusInterface *interface,
                  gpointer user_data)
{
  GDBusProxy **proxy = user_data;
  if (*proxy == G_DBUS_PROXY (interface))
    g_clear_object (proxy);
}

static GDBusProxy *
lookup_interface (Test *test,
                  const gchar *path,
                  const gchar *interface)
{
  gpointer proxy;

  proxy = g_dbus_object_manager_get_interface (test->objman, path, interface);

  return proxy;
}

static void
setup_vgcreate (Test *test,
                gconstpointer data)
{
  gulong sig;

  setup_target (test, data);

  sig = g_signal_connect (test->objman, "interface-added",
                          G_CALLBACK (on_volume_group_added), &test->volume_group);

  testing_target_execute (NULL, "vgcreate", "test-udisk-lvm",
                          test->blocks[0].device, test->blocks[1].device, NULL);

  testing_wait_until (test->volume_group != NULL);
  g_signal_handler_disconnect (test->objman, sig);
}

static void
teardown_vgremove (Test *test,
                   gconstpointer data)
{
  g_clear_object (&test->volume_group);
  testing_target_execute (NULL, "vgremove", "-f", "test-udisk-lvm", NULL);
  teardown_target (test, data);
}

static void
test_volume_group_create (Test *test,
                          gconstpointer data)
{
  GDBusProxy *volume_group = NULL;
  const gchar *volume_group_path;
  GDBusProxy *manager;
  GVariant *blocks[2];
  GVariant *retval;
  GError *error = NULL;

  manager = lookup_interface (test, "/org/freedesktop/UDisks2/Manager", "com.redhat.lvm2.Manager");
  g_assert (manager != NULL);

  blocks[0] = g_variant_new_object_path (test->blocks[0].object_path);
  blocks[1] = g_variant_new_object_path (test->blocks[1].object_path);
  retval = g_dbus_proxy_call_sync (manager, "VolumeGroupCreate",
                                   g_variant_new ("(@aost@a{sv})",
                                                  g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH, blocks, 2),
                                                  "test-udisk-lvm", (guint64)0,
                                                  g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0)),
                                   G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                   -1, NULL, &error);
  g_assert_no_error (error);

  while (g_main_context_iteration (NULL, FALSE));

  g_variant_get (retval, "(&o)", &volume_group_path);
  volume_group = lookup_interface (test, volume_group_path, "com.redhat.lvm2.VolumeGroup");
  g_assert (volume_group != NULL);

  g_assert_cmpstr (testing_proxy_string (volume_group, "Name"), ==, "test-udisk-lvm");

  g_variant_unref (retval);
}

static void
test_volume_group_delete (Test *test,
                          gconstpointer data)
{
  GVariant *retval;
  GError *error = NULL;

  /* Now delete it, and it should dissappear */
  g_signal_connect (test->objman, "interface-removed",
                    G_CALLBACK (on_proxy_removed), &test->volume_group);

  retval = g_dbus_proxy_call_sync (test->volume_group, "Delete",
                                   g_variant_new ("(@a{sv})",
                                                  g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0)),
                                   G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                   -1, NULL, &error);

  g_assert_no_error (error);

  /* The object should disappear */
  testing_wait_until (test->volume_group == NULL);

  g_variant_unref (retval);
}

int
main (int argc,
      char **argv)
{
#if !GLIB_CHECK_VERSION(2,36,0)
  g_type_init ();
#endif

  g_test_init (&argc, &argv, NULL);

  if (testing_target_init ())
    {
      g_test_add ("/udisks/lvm/volume-group/create", Test, NULL,
                  setup_target, test_volume_group_create, teardown_vgremove);
      g_test_add ("/udisks/lvm/volume-group/delete", Test, NULL,
                  setup_vgcreate, test_volume_group_delete, teardown_target);
    }

  return g_test_run ();
}
