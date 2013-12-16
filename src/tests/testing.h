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

#ifndef __TESTING_IO_STREAM_H__
#define __TESTING_IO_STREAM_H__

#include <gio/gio.h>

#include <string.h>

extern const gchar * testing_target_name;

gboolean             testing_target_init            (void);

GDBusConnection *    testing_target_connect         (void);

void                 testing_target_execute         (gchar **output,
                                                     const gchar *prog,
                                                     ...) G_GNUC_NULL_TERMINATED;

gpointer             testing_target_launch          (const gchar *wait_until,
                                                     const gchar *prog,
                                                     ...) G_GNUC_NULL_TERMINATED;

gint                 testing_target_wait            (gpointer launched);

#define TESTING_TYPE_IO_STREAM    (testing_io_stream_get_type ())
#define TESTING_IO_STREAM(o)      (G_TYPE_CHECK_INSTANCE_CAST ((o), TESTING_TYPE_IO_STREAM, TestingIOStream))
#define TESTING_IS_IO_STREAM(o)   (G_TYPE_CHECK_INSTANCE_TYPE ((o), TESTING_TYPE_IO_STREAM))

typedef struct _TestingIOStream TestingIOStream;

GType            testing_io_stream_get_type       (void);

GIOStream *      testing_io_stream_new            (GInputStream *input,
                                                   GOutputStream *output);

#ifndef g_assert_str_contains
#define g_assert_str_contains(s1, s2) G_STMT_START { \
  const char *__s1 = (s1), *__s2 = (s2); \
  if (strstr (__s1, __s2) != NULL) ; else \
    testing_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                               "assertion failed (%s): (\"%s\", \"%s\")", \
                               #s1 " does not contain " #s2, __s1, __s2); \
} G_STMT_END
#endif

#ifndef g_assert_str_matches
#define g_assert_str_matches(s1, s2) G_STMT_START { \
  const char *__s1 = (s1), *__s2 = (s2); \
  if (g_pattern_match_simple (__s2, __s1)) ; else \
    testing_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                               "assertion failed (%s): (\"%s\", \"%s\")", \
                               #s1 " does not match " #s2, __s1, __s2); \
  } G_STMT_END
#endif

void             testing_assertion_message        (const gchar *log_domain,
                                                   const gchar *file,
                                                   gint line,
                                                   const gchar *func,
                                                   const gchar *format,
                                                   ...) G_GNUC_PRINTF (5, 6);

#endif /* __TESTING_IO_STREAM_H__ */
