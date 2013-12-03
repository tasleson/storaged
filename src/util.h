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

#ifndef __UDISKS_DAEMON_UTIL_H__
#define __UDISKS_DAEMON_UTIL_H__

#include "daemon.h"

G_BEGIN_DECLS

void                ul_util_safe_append_to_object_path   (GString *str,
                                                          const gchar *s);

gchar *             ul_util_escape                  (const gchar *str);

gchar *             ul_util_escape_and_quote        (const gchar *str);

GPid                ul_util_spawn_for_variant       (const gchar **argv,
                                                     const GVariantType *type,
                                                     void (*callback) (GPid, GVariant *, GError *, gpointer),
                                                     gpointer user_data);

gchar *             ul_util_encode_lvm_name         (const gchar *name,
                                                     gboolean for_logical_volume);

gchar *             ul_util_decode_lvm_name         (const gchar *encoded);

/*
 * GLib doesn't have g_info() yet:
 * https://bugzilla.gnome.org/show_bug.cgi?id=711103
 */
#ifndef g_info

#if defined(G_HAVE_ISO_VARARGS)

#define g_info(...)     g_log (G_LOG_DOMAIN,         \
                               G_LOG_LEVEL_INFO,     \
                               __VA_ARGS__)

#elif defined (G_HAVE_GNUC_VARARGS)

#define g_info(format...)       g_log (G_LOG_DOMAIN,         \
                                       G_LOG_LEVEL_INFO,     \
                                       format)

#else

static void
g_info (const gchar *format,
        ...)
{
  va_list args;
  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, format, args);
  va_end (args);
}

#endif /* no varargs */

#endif /* g_info */

G_END_DECLS

#endif /* __UDISKS_DAEMON_UTIL_H__ */
