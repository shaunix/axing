/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2012 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __AXING_RESOURCE_H__
#define __AXING_RESOURCE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AXING_TYPE_RESOURCE            (axing_resource_get_type ())
#define AXING_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), AXING_TYPE_RESOURCE, AxingResource))
#define AXING_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), AXING_TYPE_RESOURCE, AxingResourceClass))
#define AXING_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AXING_TYPE_RESOURCE))
#define AXING_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AXING_TYPE_RESOURCE))
#define AXING_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), AXING_TYPE_RESOURCE, AxingResourceClass))

typedef struct _AxingResource        AxingResource;
typedef struct _AxingResourcePrivate AxingResourcePrivate;
typedef struct _AxingResourceClass   AxingResourceClass;

struct _AxingResource {
    GObject parent;

    /*< private >*/
    AxingResourcePrivate *priv;
};

struct _AxingResourceClass {
    GObjectClass parent_class;

    /*< private >*/
    void (*_axing_reserved1) (void);
    void (*_axing_reserved2) (void);
    void (*_axing_reserved3) (void);
    void (*_axing_reserved4) (void);
    void (*_axing_reserved5) (void);
};


GType                axing_resource_get_type           (void);

AxingResource *      axing_resource_new                (GFile         *file,
                                                        GInputStream  *stream);

GFile *              axing_resource_get_file           (AxingResource *resource);
GInputStream *       axing_resource_get_input_stream   (AxingResource *resource);

G_END_DECLS

#endif /* __AXING_RESOURCE_H__ */
