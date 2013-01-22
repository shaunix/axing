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

#include "axing-stream.h"

struct _AxingStreamPrivate {
    gpointer reserved;
};

static void      axing_stream_init          (AxingStream       *stream);
static void      axing_stream_class_init    (AxingStreamClass  *klass);
static void      axing_stream_dispose       (GObject           *object);
static void      axing_stream_finalize      (GObject           *object);
static void      axing_stream_get_property  (GObject           *object,
                                             guint              prop_id,
                                             GValue            *value,
                                             GParamSpec        *pspec);
static void      axing_stream_set_property  (GObject           *object,
                                             guint              prop_id,
                                             const GValue      *value,
                                             GParamSpec        *pspec);

static void      stream_event_default       (AxingStream       *stream);


enum {
    STREAM_EVENT,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (AxingStream, axing_stream, G_TYPE_OBJECT);

static void
axing_stream_init (AxingStream *stream)
{
    stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream, AXING_TYPE_STREAM,
                                                AxingStreamPrivate);
}

static void
axing_stream_class_init (AxingStreamClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (AxingStreamPrivate));

    klass->stream_event = stream_event_default;

    object_class->get_property = axing_stream_get_property;
    object_class->set_property = axing_stream_set_property;
    object_class->dispose = axing_stream_dispose;
    object_class->finalize = axing_stream_finalize;

    signals[STREAM_EVENT] = g_signal_new ("stream-event",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_FIRST,
                                          G_STRUCT_OFFSET (AxingStreamClass, stream_event),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE, 0);
}

static void
axing_stream_dispose (GObject *object)
{
    G_OBJECT_CLASS (axing_stream_parent_class)->dispose (object);
}

static void
axing_stream_finalize (GObject *object)
{
    G_OBJECT_CLASS (axing_stream_parent_class)->finalize (object);
}

static void
axing_stream_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
}

static void
axing_stream_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
}

AxingStreamEventType
axing_stream_get_event_type (AxingStream *stream)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_event_type != NULL);
    return klass->get_event_type (stream);
}

const char *
axing_stream_get_event_localname (AxingStream *stream)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_event_localname != NULL);
    return klass->get_event_localname (stream);
}

const char *
axing_stream_get_event_qname (AxingStream *stream)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_event_qname != NULL);
    return klass->get_event_qname (stream);
}

const char *
axing_stream_get_event_namespace (AxingStream *stream)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_event_namespace != NULL);
    return klass->get_event_namespace (stream);
}

const char *
axing_stream_get_event_prefix (AxingStream *stream)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_event_prefix != NULL);
    return klass->get_event_prefix (stream);
}

const char *
axing_stream_get_event_content (AxingStream *stream)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_event_content != NULL);
    return klass->get_event_content (stream);
}

const char * const *
axing_stream_get_attributes (AxingStream *stream)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_attributes != NULL);
    return klass->get_attributes (stream);
}

const char *
axing_stream_get_attribute_localname (AxingStream *stream,
                                      const char  *qname)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_attribute_localname != NULL);
    return klass->get_attribute_localname (stream, qname);
}

const char *
axing_stream_get_attribute_prefix (AxingStream *stream,
                                   const char  *qname)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_attribute_prefix != NULL);
    return klass->get_attribute_prefix (stream, qname);
}

const char *
axing_stream_get_attribute_namespace (AxingStream *stream,
                                      const char  *qname)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_attribute_namespace != NULL);
    return klass->get_attribute_namespace (stream, qname);
}

const char *
axing_stream_get_attribute_value (AxingStream *stream,
                                  const char  *name,
                                  const char  *ns)
{
    AxingStreamClass *klass = AXING_STREAM_GET_CLASS (stream);
    g_assert (klass->get_attribute_value != NULL);
    return klass->get_attribute_value (stream, name, ns);
}

static void
stream_event_default (AxingStream *stream)
{
    /* FIXME */
}

void
axing_stream_emit_event (AxingStream *stream)
{
    g_signal_emit (stream, signals[STREAM_EVENT], 0);
}
