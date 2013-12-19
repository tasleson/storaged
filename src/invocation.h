/*
 * Copyright 2012 Red Hat Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Stef Walter <stefw@gnome.org>
 */

#include "config.h"

#ifndef __INVOCATION_H__
#define __INVOCATION_H__

#include <gio/gio.h>

G_BEGIN_DECLS

typedef void (* UlClientFunc) (const gchar *bus_name,
                               gpointer user_data);

void                 ul_invocation_initialize             (GDBusConnection *connection,
                                                           UlClientFunc client_appeared,
                                                           UlClientFunc client_disappeared,
                                                           gpointer user_data);

uid_t                ul_invocation_get_caller_uid         (GDBusMethodInvocation *invocation);

void                 ul_invocation_cleanup                (void);

G_END_DECLS

#endif /* __UL_INVOCATION_H__ */
