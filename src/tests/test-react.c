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

#define wait_until_and_idle(cond) G_STMT_START { \
  while (!(cond)) \
    g_main_context_iteration (NULL, TRUE); \
  while (g_main_context_iteration (NULL, FALSE)); \
} G_STMT_END

static void
unbreak_object_manager_added (GDBusObjectManager *udisks_object_manager,
                              GDBusObject *object,
                              gpointer user_data)
{
  GList *interfaces, *l;

  /* Yes, GDBusObjectManager really is this awkward */
  interfaces = g_dbus_object_get_interfaces (object);
  for (l = interfaces; l != NULL; l = g_list_next (l))
    g_signal_emit_by_name (udisks_object_manager, "interface-added", object, l->data);
  g_list_free_full (interfaces, g_object_unref);
}

static void
unbreak_object_manager_removed (GDBusObjectManager *udisks_object_manager,
                                GDBusObject *object,
                                gpointer user_data)
{
  GList *interfaces, *l;

  /* Yes, GDBusObjectManager really is this awkward */
  interfaces = g_dbus_object_get_interfaces (object);
  for (l = interfaces; l != NULL; l = g_list_next (l))
    g_signal_emit_by_name (udisks_object_manager, "interface-removed", object, l->data);
  g_list_free_full (interfaces, g_object_unref);
}

static void
setup_target (Test *test,
              gconstpointer data)
{
  GError *error = NULL;
  gchar *base;
  gchar *arg;
  guint i;

  test->bus = testing_target_connect ();

  test->daemon = testing_target_launch ("*Acquired*on the system message bus*",
                                        BUILDDIR "/src/udisks-lvm",
                                        "--resource-dir=" BUILDDIR "/src",
                                        "--replace", "--debug",
                                        NULL);

  test->objman = g_dbus_object_manager_client_new_sync (test->bus,
                                                        G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
                                                        "com.redhat.lvm2",
                                                        "/org/freedesktop/UDisks2",
                                                        NULL, NULL, NULL, NULL, &error);
  g_assert_no_error (error);

  /* Groan */
  g_signal_connect (test->objman, "object-added", G_CALLBACK (unbreak_object_manager_added), NULL);
  g_signal_connect (test->objman, "object-removed", G_CALLBACK (unbreak_object_manager_removed), NULL);

  /* Free up any unused devices */
  testing_target_execute (NULL, "losetup", "-D", NULL);
  testing_target_execute (NULL, "pvscan", "--cache", NULL);

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
  GError *error = NULL;
  gint status;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (test->blocks); i++)
    {
      g_free (test->blocks[i].device);
      g_free (test->blocks[i].object_path);
    }

  g_clear_object (&test->objman);

  g_dbus_connection_flush_sync (test->bus, NULL, &error);
  g_assert_no_error (error);
  g_clear_object (&test->bus);

  status = testing_target_wait (test->daemon);
  g_assert_cmpint (status, ==, 0);
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
on_logical_volume_added (GDBusObjectManager *objman,
                         GDBusObject *make_it_stop,
                         GDBusInterface *interface,
                         gpointer user_data)
{
  ret_proxy_if_interface_matches (G_DBUS_PROXY (interface),
                                  "com.redhat.lvm2.LogicalVolume", user_data);
}

static void
on_logical_volume_block_added (GDBusObjectManager *objman,
                               GDBusObject *make_it_stop,
                               GDBusInterface *interface,
                               gpointer user_data)
{
  ret_proxy_if_interface_matches (G_DBUS_PROXY (interface),
                                  "com.redhat.lvm2.LogicalVolumeBlock", user_data);
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

static const gchar *
string_property (GDBusProxy *proxy,
                 const gchar *property)
{
  GVariant *value;
  const gchar *ret = NULL;

  value = g_dbus_proxy_get_cached_property (proxy, property);
  if (value)
    {
      ret = g_variant_get_string (value, NULL);
      g_variant_unref (value);
    }

  return ret;
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

  wait_until_and_idle (test->volume_group != NULL);
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
test_vgcreate_remove (Test *test,
                      gconstpointer data)
{
  GDBusProxy *volume_group = NULL;
  GDBusProxy *block;

  g_signal_connect (test->objman, "interface-added",
                    G_CALLBACK (on_volume_group_added), &volume_group);

  testing_target_execute (NULL, "vgcreate", "test-udisk-lvm",
                          test->blocks[0].device, test->blocks[1].device, NULL);

  wait_until_and_idle (volume_group != NULL);

  /* Found a new VolumeGroup exposed */
  g_assert_str_matches (g_dbus_proxy_get_object_path (volume_group), "/org/freedesktop/UDisks2/lvm/*");
  g_assert_cmpstr (string_property (volume_group, "Name"), ==, "test-udisk-lvm");

  /* At this point these two guys should each be a PhysicalVolumeBlock */
  block = lookup_interface (test, test->blocks[0].object_path, "com.redhat.lvm2.PhysicalVolumeBlock");
  g_assert_cmpstr (string_property (block, "VolumeGroup"), ==, g_dbus_proxy_get_object_path (volume_group));
  g_object_unref (block);

  block = lookup_interface (test, test->blocks[1].object_path, "com.redhat.lvm2.PhysicalVolumeBlock");
  g_assert_cmpstr (string_property (block, "VolumeGroup"), ==, g_dbus_proxy_get_object_path (volume_group));
  g_object_unref (block);

  g_signal_connect (test->objman, "interface-removed",
                    G_CALLBACK (on_proxy_removed), &volume_group);

  testing_target_execute (NULL, "vgremove", "-f", "test-udisk-lvm", NULL);

  /* The object should disappear */
  wait_until_and_idle (volume_group == NULL);
}

static void
test_lvcreate_change_remove (Test *test,
                             gconstpointer data)
{
  GDBusProxy *proxy = NULL;
  GDBusProxy *logical_volume1;
  GDBusProxy *logical_volume2;
  const gchar *volume_group_path;
  GDBusProxy *block = NULL;

  g_signal_connect (test->objman, "interface-added",
                    G_CALLBACK (on_logical_volume_added), &proxy);

  testing_target_execute (NULL, "lvcreate", "test-udisk-lvm", "--name", "one",
                          "--size", "20m", "--activate", "n", "--zero", "n", NULL);

  wait_until_and_idle (proxy != NULL);
  logical_volume1 = proxy;
  proxy = NULL;

  /*
   * TODO: We get the following here. Does LVM not support enumerating (by udisks-lvm)
   * simultaneous to creation of a new logical volume?
   *
   * lvcreate test-udisk-lvm -n two -L 20m
   * device-mapper: create ioctl on test--udisk--lvm-two failed: Device or resource busy
   * Failed to activate new LV.
   *
   * TODO: Fix this for real, because it'll come up IRL.
   */
  g_usleep (G_USEC_PER_SEC / 2);

  testing_target_execute (NULL, "lvcreate", "test-udisk-lvm", "--name", "two",
                          "--size", "20m", "--activate", "n", "--zero", "n", NULL);

  wait_until_and_idle (proxy != NULL);
  logical_volume2 = proxy;
  proxy = NULL;

  /* Check that they're in the volume group, ... both by path */
  volume_group_path = g_dbus_proxy_get_object_path (test->volume_group);
  g_assert_str_prefix (g_dbus_proxy_get_object_path (logical_volume1), volume_group_path);
  g_assert_str_prefix (g_dbus_proxy_get_object_path (logical_volume2), volume_group_path);

  /* ... and explicitly */
  g_assert_cmpstr (string_property (logical_volume1, "VolumeGroup"), ==, volume_group_path);
  g_assert_cmpstr (string_property (logical_volume2, "VolumeGroup"), ==, volume_group_path);

  /* Both have the right names */
  g_assert_cmpstr (string_property (logical_volume1, "Name"), ==, "one");
  g_assert_cmpstr (string_property (logical_volume2, "Name"), ==, "two");

  /* Activate one of them, and a new block should appear */
  g_signal_connect (test->objman, "interface-added",
                    G_CALLBACK (on_logical_volume_block_added), &block);
  testing_target_execute (NULL, "lvchange", "test-udisk-lvm/one", "--activate", "y", NULL);
  wait_until_and_idle (block != NULL);

  /* The new block should have the right property pointing back to lv */
  g_assert_cmpstr (string_property (block, "LogicalVolume"), ==, g_dbus_proxy_get_object_path (logical_volume1));

  /* Remove the other logical volume, and it should disappear */
  g_signal_connect (test->objman, "interface-removed",
                    G_CALLBACK (on_proxy_removed), &logical_volume2);

  testing_target_execute (NULL, "lvremove", "test-udisk-lvm/two", NULL);
  wait_until_and_idle (logical_volume2 == NULL);

  g_object_unref (logical_volume1);
  g_object_unref (block);
}

static void
test_vgreduce (Test *test,
               gconstpointer data)
{
  const gchar *volume_group_path;
  GDBusProxy *block = NULL;

  block = lookup_interface (test, test->blocks[0].object_path, "com.redhat.lvm2.PhysicalVolumeBlock");
  volume_group_path = g_dbus_proxy_get_object_path (test->volume_group);
  g_assert_cmpstr (string_property (block, "VolumeGroup"), ==, volume_group_path);

  g_signal_connect (test->objman, "interface-removed",
                    G_CALLBACK (on_proxy_removed), &block);

  testing_target_execute (NULL, "vgreduce", "test-udisk-lvm", test->blocks[0].device, NULL);

  wait_until_and_idle (block == NULL);
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
      g_test_add ("/udisks/lvm/vgcreate-remove", Test, NULL,
                  setup_target, test_vgcreate_remove, teardown_target);
      g_test_add ("/udisks/lvm/lvcreate-change-remove", Test, NULL,
                  setup_vgcreate, test_lvcreate_change_remove, teardown_vgremove);
      g_test_add ("/udisks/lvm/vgreduce", Test, NULL,
                  setup_vgcreate, test_vgreduce, teardown_vgremove);
    }

  return g_test_run ();
}
