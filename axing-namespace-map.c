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

#include "axing-namespace-map.h"

G_DEFINE_INTERFACE (AxingNamespaceMap, axing_namespace_map, G_TYPE_OBJECT);

static void
axing_namespace_map_default_init (AxingNamespaceMapInterface *iface)
{
}

const char *
axing_namespace_map_get_namespace (AxingNamespaceMap *map,
                                   const char        *prefix)
{
    g_return_if_fail (AXING_IS_NAMESPACE_MAP (map));

    AXING_NAMESPACE_MAP_GET_INTERFACE (map)->get_namespace (map, prefix);
}
