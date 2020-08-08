/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2013-2020 Shaun McCance  <shaunm@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __AXING_SIMPLE_RESOLVER_H__
#define __AXING_SIMPLE_RESOLVER_H__

#include <glib-object.h>
#include "axing-resolver.h"

G_BEGIN_DECLS

#define AXING_TYPE_SIMPLE_RESOLVER axing_simple_resolver_get_type ()
G_DECLARE_FINAL_TYPE (AxingSimpleResolver, axing_simple_resolver, AXING, SIMPLE_RESOLVER, AxingResolver)

AxingResolver *  axing_simple_resolver_new              (void);

G_END_DECLS

#endif /* __AXING_SIMPLE_RESOLVER_H__ */
