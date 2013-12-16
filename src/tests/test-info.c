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

static GHashTable *
testing_list_objects (GDBusConnection *connection,
                      const gchar *bus_name,
                      const gchar *path_manager,
                      const gchar *path_prefix)
{
  GError *error = NULL;
  GVariant *arg0;
  const gchar *object_path;
  GVariant *ifaces_and_properties;
  GVariantIter iter;
  GHashTable *objects;
  GVariant *retval;

  retval = g_dbus_connection_call_sync (connection,
                                        bus_name,
                                        path_manager,
                                        "org.freedesktop.DBus.ObjectManager",
                                        "GetManagedObjects",
                                        g_variant_new ("()"),
                                        G_VARIANT_TYPE ("(a{oa{sa{sv}}})"),
                                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                        -1, NULL, &error);
  g_assert_no_error (error);

  objects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                   (GDestroyNotify)g_variant_unref);

  arg0 = g_variant_get_child_value (retval, 0);
  g_variant_iter_init (&iter, arg0);
  while (g_variant_iter_next (&iter,
                              "{&o@a{sa{sv}}}",
                              &object_path,
                              &ifaces_and_properties))
    {
      if (!path_prefix || g_str_has_prefix (object_path, path_prefix))
        g_hash_table_insert (objects, g_strdup (object_path), ifaces_and_properties);
      else
        g_variant_unref (ifaces_and_properties);
    }
  g_variant_unref (arg0);
  g_variant_unref (retval);

  return objects;
}

typedef struct {
  GDBusConnection *bus;
  gpointer daemon;
} Test;

static void
setup_target (Test *test,
              gconstpointer data)
{
  test->bus = testing_target_connect ();
  test->daemon = testing_target_launch ("*Acquired*on the system message bus*",
                                        BUILDDIR "/src/udisks-lvm",
                                        "--resource-dir=" BUILDDIR "/src",
                                        "--replace", "--debug",
                                        NULL);
}

static void
teardown_target (Test *test,
                 gconstpointer data)
{
  gint status = testing_target_wait (test->daemon);
  g_assert_cmpint (status, ==, 0);
  g_clear_object (&test->bus);
}

static void
test_block_objects (Test *test,
                    gconstpointer data)
{
  GHashTable *objects;
  GHashTableIter iter;
  gpointer key;

  objects = testing_list_objects (test->bus, "com.redhat.lvm2",
                                  "/org/freedesktop/UDisks2",
                                  "/org/freedesktop/UDisks2/block_devices/");

  if (g_test_verbose ())
    {
      g_hash_table_iter_init (&iter, objects);
      while (g_hash_table_iter_next (&iter, &key, NULL))
        g_printerr ("%s\n", (gchar *)key);
    }

  g_assert_cmpint (g_hash_table_size (objects), >, 0);

  g_hash_table_unref (objects);
}

GError *error = NULL;
GVariant *retval;
gchar *s;

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
      g_test_add ("/udisks/lvm/block-objects", Test, NULL, setup_target, test_block_objects, teardown_target);
    }

  return g_test_run ();
}
