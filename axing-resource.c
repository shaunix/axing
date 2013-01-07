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

#include "axing-resource.h"
#include "axing-private.h"

struct _AxingResourcePrivate {
    GFile *file;
    GInputStream *input;
    GSimpleAsyncResult *result;
};

static void      axing_resource_init          (AxingResource       *resource);
static void      axing_resource_class_init    (AxingResourceClass  *klass);
static void      axing_resource_dispose       (GObject           *object);
static void      axing_resource_finalize      (GObject           *object);
static void      axing_resource_get_property  (GObject           *object,
                                               guint              prop_id,
                                               GValue            *value,
                                               GParamSpec        *pspec);
static void      axing_resource_set_property  (GObject           *object,
                                               guint              prop_id,
                                               const GValue      *value,
                                               GParamSpec        *pspec);

enum {
    PROP_0,
    PROP_FILE,
    PROP_STREAM,
    N_PROPS
};

G_DEFINE_TYPE (AxingResource, axing_resource, G_TYPE_OBJECT);

static void
axing_resource_init (AxingResource *resource)
{
    resource->priv = G_TYPE_INSTANCE_GET_PRIVATE (resource, AXING_TYPE_RESOURCE,
                                                  AxingResourcePrivate);
}

static void
axing_resource_class_init (AxingResourceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (AxingResourcePrivate));

    object_class->get_property = axing_resource_get_property;
    object_class->set_property = axing_resource_set_property;
    object_class->dispose = axing_resource_dispose;

    g_object_class_install_property (object_class, PROP_FILE,
                                     g_param_spec_object ("file",
                                                          N_("file"),
                                                          N_("The GFile object identifying the resource"),
                                                          G_TYPE_FILE,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class, PROP_STREAM,
                                     g_param_spec_object ("input-stream",
                                                          N_("input stream"),
                                                          N_("The GInputStream to read the resource"),
                                                          G_TYPE_INPUT_STREAM,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_STRINGS));
}

static void
axing_resource_dispose (GObject *object)
{
    AxingResource *resource = AXING_RESOURCE (object);
    if (resource->priv->file) {
        g_object_unref (resource->priv->file);
        resource->priv->file = NULL;
    }
    if (resource->priv->input) {
        g_object_unref (resource->priv->input);
        resource->priv->input = NULL;
    }
    if (resource->priv->result) {
        g_object_unref (resource->priv->result);
        resource->priv->result = NULL;
    }
    G_OBJECT_CLASS (axing_resource_parent_class)->dispose (object);
}

static void
axing_resource_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    AxingResource *resource = AXING_RESOURCE (object);
    switch (prop_id) {
    case PROP_FILE:
        g_value_set_object (value, resource->priv->file);
        break;
    case PROP_STREAM:
        g_value_set_object (value, resource->priv->input);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
axing_resource_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    AxingResource *resource = AXING_RESOURCE (object);
    switch (prop_id) {
    case PROP_FILE:
        if (resource->priv->file)
            g_object_unref (resource->priv->file);
        resource->priv->file = G_FILE (g_value_dup_object (value));
        break;
    case PROP_STREAM:
        if (resource->priv->input)
            g_object_unref (resource->priv->input);
        resource->priv->input = G_INPUT_STREAM (g_value_dup_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

AxingResource *
axing_resource_new (GFile        *file,
                    GInputStream *stream)
{
    g_return_val_if_fail (file != NULL || stream != NULL, NULL);
    return g_object_new (AXING_TYPE_RESOURCE,
                         "file", file,
                         "input-stream", stream,
                         NULL);
}

GFile *
axing_resource_get_file (AxingResource *resource)
{
    return resource->priv->file;
}

GInputStream *
axing_resource_get_input_stream (AxingResource *resource)
{
    return resource->priv->input;
}

GInputStream *
axing_resource_read (AxingResource  *resource,
                     GCancellable   *cancellable,
                     GError        **error)
{
    if (resource->priv->input) {
        return resource->priv->input;
    }
    else {
        return (GInputStream *) g_file_read (resource->priv->file,
                                             cancellable,
                                             error);
    }
}

static void
resource_file_read_cb (GFile         *file,
                       GAsyncResult  *result,
                       AxingResource *resource)
{
    GError *error = NULL;

    resource->priv->input = G_INPUT_STREAM (g_file_read_finish (file, result, &error));

    if (error) {
        g_simple_async_result_set_from_error (resource->priv->result, error);
        g_error_free (error);
    }

    g_simple_async_result_complete (resource->priv->result);
}

void
axing_resource_read_async (AxingResource       *resource,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
    resource->priv->result = g_simple_async_result_new (G_OBJECT (resource),
                                                        callback, user_data,
                                                        axing_resource_read_async);
    if (resource->priv->input) {
        g_simple_async_result_complete_in_idle (resource->priv->result);
    }
    else {
        g_file_read_async (resource->priv->file,
                           G_PRIORITY_DEFAULT,
                           cancellable,
                           (GAsyncReadyCallback) resource_file_read_cb,
                           resource);
    }
}

GInputStream *
axing_resource_read_finish (AxingResource *resource,
                            GAsyncResult  *res,
                            GError       **error)
{
    g_simple_async_result_propagate_error (resource->priv->result, error);

    if (resource->priv->input)
        return g_object_ref (resource->priv->input);
    else
        return NULL;
}
