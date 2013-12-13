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

extern const gchar * testing_target_name;

gboolean             testing_target_init            (void);

GDBusConnection *    testing_target_connect         (void);

void                 testing_target_execute         (const gchar *prog,
                                                     ...) G_GNUC_NULL_TERMINATED;

GPid                 testing_target_launch          (const gchar *wait_until,
                                                     const gchar *prog,
                                                     ...) G_GNUC_NULL_TERMINATED;

void                 testing_target_upload          (const gchar *dest_path,
                                                     const gchar *file,
                                                     ...) G_GNUC_NULL_TERMINATED;

#define TESTING_TYPE_IO_STREAM    (testing_io_stream_get_type ())
#define TESTING_IO_STREAM(o)      (G_TYPE_CHECK_INSTANCE_CAST ((o), TESTING_TYPE_IO_STREAM, TestingIOStream))
#define TESTING_IS_IO_STREAM(o)   (G_TYPE_CHECK_INSTANCE_TYPE ((o), TESTING_TYPE_IO_STREAM))

typedef struct _TestingIOStream TestingIOStream;

GType            testing_io_stream_get_type       (void);

GIOStream *      testing_io_stream_new            (GInputStream *input,
                                                   GOutputStream *output);

#endif /* __TESTING_IO_STREAM_H__ */
