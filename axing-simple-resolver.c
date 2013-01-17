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

#include "axing-simple-resolver.h"
#include "axing-utils.h"

struct _AxingSimpleResolverPrivate {
    gpointer reserved;
};

static void      axing_simple_resolver_init           (AxingSimpleResolver       *resolver);
static void      axing_simple_resolver_class_init     (AxingSimpleResolverClass  *klass);
static void      axing_simple_resolver_dispose        (GObject                   *object);
static void      axing_simple_resolver_finalize       (GObject                   *object);

static AxingResource * simple_resolver_resolve        (AxingResolver        *resolver,
                                                       AxingResource        *base,
                                                       const char           *xml_base,
                                                       const char           *uri,
                                                       const char           *pubid,
                                                       const char           *hint,
                                                       GCancellable         *cancellable,
                                                       GError              **error);
static void            simple_resolver_resolve_async  (AxingResolver        *resolver,
                                                       AxingResource        *base,
                                                       const char           *xml_base,
                                                       const char           *uri,
                                                       const char           *pubid,
                                                       const char           *hint,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
static AxingResource * simple_resolver_resolve_finish (AxingResolver        *resolver,
                                                       GAsyncResult         *result,
                                                       GError              **error);

G_DEFINE_TYPE (AxingSimpleResolver, axing_simple_resolver, AXING_TYPE_RESOLVER);

static void
axing_simple_resolver_init (AxingSimpleResolver *resolver)
{
    resolver->priv = G_TYPE_INSTANCE_GET_PRIVATE (resolver, AXING_TYPE_SIMPLE_RESOLVER,
                                                  AxingSimpleResolverPrivate);
}

static void
axing_simple_resolver_class_init (AxingSimpleResolverClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    AxingResolverClass *resolver_class = AXING_RESOLVER_CLASS (klass);

    g_type_class_add_private (klass, sizeof (AxingSimpleResolverPrivate));

    resolver_class->resolve = simple_resolver_resolve;
    resolver_class->resolve_async = simple_resolver_resolve_async;
    resolver_class->resolve_finish = simple_resolver_resolve_finish;

    object_class->dispose = axing_simple_resolver_dispose;
    object_class->finalize = axing_simple_resolver_finalize;
}

static void
axing_simple_resolver_dispose (GObject *object)
{
    G_OBJECT_CLASS (axing_simple_resolver_parent_class)->dispose (object);
}

static void
axing_simple_resolver_finalize (GObject *object)
{
    G_OBJECT_CLASS (axing_simple_resolver_parent_class)->finalize (object);
}

AxingResolver *
axing_simple_resolver_new (void)
{
    return g_object_new (AXING_TYPE_SIMPLE_RESOLVER, NULL);
}

static AxingResource *
simple_resolver_resolve (AxingResolver *resolver,
                         AxingResource *base,
                         const char    *xml_base,
                         const char    *link,
                         const char    *pubid,
                         const char    *hint,
                         GCancellable  *cancellable,
                         GError       **error)
{
    GFile *file = NULL;
    char *ret;

    if (xml_base && xml_base[0]) {
        ret = axing_uri_resolve_relative (xml_base, link);
    }

    file = axing_resource_get_file (base);
    if (file) {
        char *tmp = g_file_get_uri (file);
        ret = axing_uri_resolve_relative (tmp, link);
        g_free (tmp);
        file = NULL;
    }

    if (ret) {
        AxingResource *resource;
        file = g_file_new_for_uri (ret);
        resource = axing_resource_new (file, NULL);
        g_free (ret);
        g_object_unref (file);
        return resource;
    }
    else {
        /* FIXME: set error */
        return NULL;
    }
}

static void
simple_resolver_resolve_async (AxingResolver       *resolver,
                               AxingResource       *base,
                               const char          *xml_base,
                               const char          *link,
                               const char          *pubid,
                               const char          *hint,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    GSimpleAsyncResult *result;
    AxingResource *resource;
    GError *error = NULL;

    resource = simple_resolver_resolve (resolver, base, xml_base,
                                        link, pubid, hint,
                                        cancellable, &error);
    if (error) {
        result = g_simple_async_result_new_from_error (G_OBJECT (resolver),
                                                       callback, user_data, error);
        g_error_free (error);
    }
    else {
        result = g_simple_async_result_new (G_OBJECT (resolver),
                                            callback, user_data,
                                            simple_resolver_resolve_async);
        g_object_set_data (G_OBJECT (result), "resource", resource);
    }

    g_simple_async_result_complete_in_idle (result);
}

static AxingResource *
simple_resolver_resolve_finish (AxingResolver *resolver,
                                GAsyncResult  *result,
                                GError       **error)
{
    AxingResource *ret = g_object_steal_data (G_OBJECT (result), "resource");

    g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);

    return ret;
}
