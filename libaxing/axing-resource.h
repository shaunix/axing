/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2012-2020 Shaun McCance  <shaunm@gnome.org>
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

#define AXING_TYPE_RESOURCE axing_resource_get_type ()
G_DECLARE_FINAL_TYPE (AxingResource, axing_resource, AXING, RESOURCE, GObject)

struct _AxingResourceClass {
    GObjectClass parent_class;

    /*< private >*/
    gpointer padding[12];
};

AxingResource *      axing_resource_new                (GFile               *file,
                                                        GInputStream        *stream);

/*
FIXME
Currently none of the AxingResource accessors ref the return value. I think I did this
because they're usually just used a proxy to something, so reffing means the caller has
to do extra steps, e.g.

    reluri = g_file_get_uri (axing_resource_get_file (rel));

But honestly it's confusing, and I spent half an hour chasing down a bug caused by me
forgetting I didn't have an extra ref. So either reflect this oddity well in the API,
or just ref all return values.

OF COURSE, this raises the bigger question of whether AxingResource should just be a
GFile implementation. Then the thing above would have just been

    reluri = g_file_get_uri (rel);

We do need AxingResource so that we can use different input streams while keeping
the same GFile base URI for stuff like catalogs and automagic compressions, and also
so we can do propoerties like transport-encoding. But it was just so much easier to
do a simple composition class than to implement and proxy the hundred or so virtual
methods on GFile.
*/

GFile *              axing_resource_get_file           (AxingResource       *resource);
GInputStream *       axing_resource_get_input_stream   (AxingResource       *resource);

GInputStream *       axing_resource_read               (AxingResource       *resource,
                                                        GCancellable        *cancellable,
                                                        GError             **error);
void                 axing_resource_read_async         (AxingResource       *resource,
                                                        GCancellable        *cancellable,
                                                        GAsyncReadyCallback  callback,
                                                        gpointer             user_data);
GInputStream *       axing_resource_read_finish        (AxingResource       *resource,
                                                        GAsyncResult        *res,
                                                        GError             **error);

G_END_DECLS

#endif /* __AXING_RESOURCE_H__ */
