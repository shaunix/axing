/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2013 Shaun McCance  <shaunm@gnome.org>
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

#define AXING_TYPE_SIMPLE_RESOLVER            (axing_simple_resolver_get_type ())
#define AXING_SIMPLE_RESOLVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), AXING_TYPE_SIMPLE_RESOLVER, AxingSimpleResolver))
#define AXING_SIMPLE_RESOLVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), AXING_TYPE_SIMPLE_RESOLVER, AxingSimpleResolverClass))
#define AXING_IS_SIMPLE_RESOLVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AXING_TYPE_SIMPLE_RESOLVER))
#define AXING_IS_SIMPLE_RESOLVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AXING_TYPE_SIMPLE_RESOLVER))
#define AXING_SIMPLE_RESOLVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), AXING_TYPE_SIMPLE_RESOLVER, AxingSimpleResolverClass))

typedef struct _AxingSimpleResolver         AxingSimpleResolver;
typedef struct _AxingSimpleResolverPrivate  AxingSimpleResolverPrivate;
typedef struct _AxingSimpleResolverClass    AxingSimpleResolverClass;

struct _AxingSimpleResolver {
    AxingResolver parent;

    /*< private >*/
    AxingSimpleResolverPrivate *priv;
};

struct _AxingSimpleResolverClass {
    AxingResolverClass parent_class;

    /*< private >*/
    void (*_axing_reserved1) (void);
    void (*_axing_reserved2) (void);
    void (*_axing_reserved3) (void);
    void (*_axing_reserved4) (void);
    void (*_axing_reserved5) (void);
};

GType            axing_simple_resolver_get_type         (void);

AxingResolver *  axing_simple_resolver_new              (void);

G_END_DECLS

#endif /* __AXING_SIMPLE_RESOLVER_H__ */
