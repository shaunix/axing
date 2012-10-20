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

#include "axing-resolver.h"

struct _AxingResolverPrivate {
    gpointer reserved;
};

static void      axing_resolver_init          (AxingResolver       *resolver);
static void      axing_resolver_class_init    (AxingResolverClass  *klass);

G_DEFINE_TYPE (AxingResolver, axing_resolver, G_TYPE_OBJECT);

static void
axing_resolver_init (AxingResolver *resolver)
{
    resolver->priv = G_TYPE_INSTANCE_GET_PRIVATE (resolver, AXING_TYPE_RESOLVER,
                                                AxingResolverPrivate);
}

static void
axing_resolver_class_init (AxingResolverClass *klass)
{
    g_type_class_add_private (klass, sizeof (AxingResolverPrivate));
}

AxingResource *
axing_resolver_resolve (AxingResolver *resolver,
                        AxingResource *base,
                        const char    *uri,
                        const char    *pubid,
                        const char    *hint,
                        GCancellable  *cancellable,
                        GError       **error)
{
    AxingResolverClass *klass = AXING_RESOLVER_GET_CLASS (resolver);
    g_assert (klass->resolve != NULL);
    return klass->resolve (resolver, base, uri, pubid, hint, cancellable, error);
}

void
axing_resolver_resolve_async (AxingResolver       *resolver,
                              AxingResource       *base,
                              const char          *uri,
                              const char          *pubid,
                              const char          *hint,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
    AxingResolverClass *klass = AXING_RESOLVER_GET_CLASS (resolver);
    g_assert (klass->resolve_async != NULL);
    return klass->resolve_async (resolver, base, uri, pubid, hint, cancellable, callback, user_data);
}

AxingResource *
axing_resolver_resolve_finish (AxingResolver *resolver,
                               GAsyncResult  *result,
                               GError       **error)
{
    AxingResolverClass *klass = AXING_RESOLVER_GET_CLASS (resolver);
    g_assert (klass->resolve_finish != NULL);
    return klass->resolve_finish (resolver, result, error);
}
