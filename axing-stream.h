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

#ifndef __AXING_STREAM_H__
#define __AXING_STREAM_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define AXING_TYPE_STREAM            (axing_stream_get_type ())
#define AXING_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), AXING_TYPE_STREAM, AxingStream))
#define AXING_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), AXING_TYPE_STREAM, AxingStreamClass))
#define AXING_IS_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AXING_TYPE_STREAM))
#define AXING_IS_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AXING_TYPE_STREAM))
#define AXING_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), AXING_TYPE_STREAM, AxingStreamClass))

typedef enum {
    AXING_STREAM_EVENT_NONE,
    /*
    AXING_STREAM_EVENT_START,
    AXING_STREAM_EVENT_END,
    */
    AXING_STREAM_EVENT_START_ELEMENT,
    AXING_STREAM_EVENT_END_ELEMENT,
    /*
    AXING_STREAM_EVENT_ATTRIBUTE,
    */
    AXING_STREAM_EVENT_CONTENT,
    AXING_STREAM_EVENT_COMMENT,
    /*
    AXING_STREAM_EVENT_CDATA,
    AXING_STREAM_EVENT_ENTITY,
    */
    /*
    AXING_STREAM_EVENT_START_PREFIX,
    AXING_STREAM_EVENT_END_PREFIX,
    */
    AXING_STREAM_EVENT_INSTRUCTION
} AxingStreamEventType;

/*
  NONE
  XML_DECLARATION
  PREFIX
  ELEMENT
  ATTRIBUTE
  ELEMENT_CONTENT
    ...
  END_ELEMENT
  END_PREFIX

  get_xml_version
  get_xml_encoding
  get_xml_standalone
  get_xml_base
  get_xml_id
  get_xml_lang
 */

typedef struct _AxingStream         AxingStream;
typedef struct _AxingStreamPrivate  AxingStreamPrivate;
typedef struct _AxingStreamClass    AxingStreamClass;

struct _AxingStream {
    GObject parent;

    /*< private >*/
    AxingStreamPrivate *priv;
};

struct _AxingStreamClass {
    GObjectClass parent_class;

    /*< public >*/
    AxingStreamEventType    (* get_event_type)         (AxingStream      *stream);
    const char *            (* get_event_localname)    (AxingStream      *stream);
    const char *            (* get_event_qname)        (AxingStream      *stream);
    const char *            (* get_event_namespace)    (AxingStream      *stream);
    const char *            (* get_event_prefix)       (AxingStream      *stream);
    const char *            (* get_event_content)      (AxingStream      *stream);

    /*< private >*/
    void                    (* stream_event)           (AxingStream *stream);

    void (*_axing_reserved1) (void);
    void (*_axing_reserved2) (void);
    void (*_axing_reserved3) (void);
    void (*_axing_reserved4) (void);
    void (*_axing_reserved5) (void);
};


GType                axing_stream_get_type                (void);

AxingStreamEventType   axing_stream_get_event_type          (AxingStream *stream);
const char *           axing_stream_get_event_localname     (AxingStream *stream);
const char *           axing_stream_get_event_qname         (AxingStream *stream);
const char *           axing_stream_get_event_namespace     (AxingStream *stream);
const char *           axing_stream_get_event_prefix        (AxingStream *stream);
const char *           axing_stream_get_event_content       (AxingStream *stream);

#ifdef FIXME_utility_functions
void         axing_stream_skip_current_element (AxingStream  *stream);
gboolean    axing_stream_event_is_empty_element (AxingStreamEvent *event);
#endif

G_END_DECLS

#endif /* __AXING_STREAM_H__ */
