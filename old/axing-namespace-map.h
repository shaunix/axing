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

#ifndef __AXING_NAMESPACE_MAP_H__
#define __AXING_NAMESPACE_MAP_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define AXING_TYPE_NAMESPACE_MAP               (axing_namespace_map_get_type ())
#define AXING_NAMESPACE_MAP(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), AXING_TYPE_NAMESPACE_MAP, AxingNamespaceMap))
#define AXING_IS_NAMESPACE_MAP(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AXING_TYPE_NAMESPACE_MAP))
#define AXING_NAMESPACE_MAP_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), AXING_TYPE_NAMESPACE_MAP, AxingNamespaceMapInterface))

typedef struct _AxingNamespaceMap           AxingNamespaceMap;
typedef struct _AxingNamespaceMapInterface  AxingNamespaceMapInterface;

struct _AxingNamespaceMapInterface {
    GObjectClass parent_class;
    const char * (* get_namespace) (AxingNamespaceMap *map,
                                    const char        *prefix);
};

GType             axing_namespace_map_get_type       (void);

const char *      axing_namespace_map_get_namespace  (AxingNamespaceMap *map,
                                                      const char        *prefix);

G_END_DECLS

#endif /* __AXING_NAMESPACE_MAP_H__ */
