/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2012-2017 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __AXING_RESOLVER_H__
#define __AXING_RESOLVER_H__

#include <glib-object.h>
#include "axing-resource.h"

G_BEGIN_DECLS

#define AXING_TYPE_RESOLVER axing_resolver_get_type ()
G_DECLARE_DERIVABLE_TYPE (AxingResolver, axing_resolver, AXING, RESOLVER, GObject)

typedef enum {
    AXING_RESOLVER_HINT_OTHER,
    AXING_RESOLVER_HINT_ENTITY,
    AXING_RESOLVER_HINT_XINCLUDE
} AxingResolverHint;

struct _AxingResolverClass {
    GObjectClass parent_class;

    /*< public >*/
    AxingResource *  (* resolve)          (AxingResolver       *resolver,
                                           AxingResource       *base,
                                           const char          *xml_base,
                                           const char          *link,
                                           const char          *pubid,
                                           AxingResolverHint    hint,
                                           GCancellable        *cancellable,
                                           GError             **error);
    void             (* resolve_async)    (AxingResolver       *resolver,
                                           AxingResource       *base,
                                           const char          *xml_base,
                                           const char          *link,
                                           const char          *pubid,
                                           AxingResolverHint    hint,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data);
    AxingResource *  (* resolve_finish)   (AxingResolver       *resolver,
                                           GAsyncResult        *result,
                                           GError             **error);

    /*< private >*/
    gpointer padding[12];
};


AxingResolver *  axing_resolver_get_default      (void);

AxingResource *  axing_resolver_resolve          (AxingResolver       *resolver,
                                                  AxingResource       *base,
                                                  const char          *xml_base,
                                                  const char          *link,
                                                  const char          *pubid,
                                                  AxingResolverHint    hint,
                                                  GCancellable        *cancellable,
                                                  GError             **error);
void             axing_resolver_resolve_async    (AxingResolver       *resolver,
                                                  AxingResource       *base,
                                                  const char          *xml_base,
                                                  const char          *link,
                                                  const char          *pubid,
                                                  AxingResolverHint    hint,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
AxingResource *  axing_resolver_resolve_finish   (AxingResolver       *resolver,
                                                  GAsyncResult        *result,
                                                  GError             **error);

G_END_DECLS

#endif /* __AXING_RESOLVER_H__ */
