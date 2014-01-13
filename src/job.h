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

#ifndef __UL_JOB_H__
#define __UL_JOB_H__

#include <udisks/udisks.h>

#include "types.h"

G_BEGIN_DECLS

#define UL_TYPE_JOB         (ul_job_get_type ())
#define UL_JOB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UL_TYPE_JOB, UlJob))
#define UL_JOB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), UL_TYPE_JOB, UlJobClass))
#define UL_JOB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), UL_TYPE_JOB, UlJobClass))
#define UL_IS_JOB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UL_TYPE_JOB))
#define UL_IS_JOB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), UL_TYPE_JOB))

typedef struct _UlJobClass     UlJobClass;
typedef struct _UlJobPrivate   UlJobPrivate;

/**
 * UlJob:
 *
 * The #UlJob structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _UlJob
{
  /*< private >*/
  UDisksJobSkeleton parent_instance;
  UlJobPrivate *priv;
};

/**
 * UlJobClass:
 * @parent_class: Parent class.
 *
 * Class structure for #UlJob.
 */
struct _UlJobClass
{
  UDisksJobSkeletonClass parent_class;
  /*< private >*/
  gpointer padding[8];
};

typedef gboolean   (* UlJobFunc)                 (GCancellable *cancellable,
                                                  gpointer user_data,
                                                  GError **error);

GType              ul_job_get_type               (void) G_GNUC_CONST;

GCancellable *     ul_job_get_cancellable        (UlJob *self);

gboolean           ul_job_get_auto_estimate      (UlJob *self);

void               ul_job_set_auto_estimate      (UlJob *self,
                                                  gboolean value);

void               ul_job_add_thing              (UlJob *self,
                                                  gpointer object_or_interface);

G_END_DECLS

#endif /* __UL_JOB_H__ */
