/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2017-2020 Shaun McCance <shaunm@gnome.org>
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

#ifndef __AXING_READER_H__
#define __AXING_READER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "axing-node-type.h"

G_BEGIN_DECLS

#define AXING_TYPE_READER axing_reader_get_type ()
G_DECLARE_INTERFACE (AxingReader, axing_reader, AXING, READER, GObject)

struct _AxingReaderInterface {
    GTypeInterface g_iface;

    /*< public >*/
    gboolean       (* read)          (AxingReader        *reader,
                                      GError            **error);
    void           (* read_async)    (AxingReader        *reader,
                                      GCancellable       *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer            user_data);
    gboolean       (* read_finish)   (AxingReader        *reader,
                                      GAsyncResult       *result,
                                      GError            **error);

    AxingNodeType  (* get_node_type) (AxingReader *reader);

    const char *  (* get_qname)     (AxingReader *reader);
    const char *  (* get_localname) (AxingReader *reader);
    const char *  (* get_prefix)    (AxingReader *reader);
    const char *  (* get_namespace) (AxingReader *reader);
    const char *  (* get_nsname)    (AxingReader *reader);

    const char *  (* get_content)   (AxingReader *reader);

    int           (* get_linenum)   (AxingReader *reader);
    int           (* get_colnum)    (AxingReader *reader);

    const char * const *  (* get_attrs)            (AxingReader *reader);
    const char *          (* get_attr_localname)   (AxingReader *reader,
                                                    const char  *qname);
    const char *          (* get_attr_prefix)      (AxingReader *reader,
                                                    const char  *qname);
    const char *          (* get_attr_namespace)   (AxingReader *reader,
                                                    const char  *qname);
    const char *          (* get_attr_nsname)      (AxingReader *reader,
                                                    const char  *qname);
    const char *          (* get_attr_value)       (AxingReader *reader,
                                                    const char  *qname);
    int                   (* get_attr_linenum)     (AxingReader *reader,
                                                    const char  *qname);
    int                   (* get_attr_colnum)      (AxingReader *reader,
                                                    const char  *qname);

    /*< private >*/
    gpointer padding[12];
};

gboolean axing_reader_read        (AxingReader        *reader,
                                   GError            **error);
void     axing_reader_read_async  (AxingReader        *reader,
                                   GCancellable       *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer            user_data);
gboolean axing_reader_read_finish (AxingReader        *reader,
                                   GAsyncResult       *result,
                                   GError            **error);

AxingNodeType  axing_reader_get_node_type        (AxingReader *reader);

const char *   axing_reader_get_qname            (AxingReader *reader);
const char *   axing_reader_get_localname        (AxingReader *reader);
const char *   axing_reader_get_prefix           (AxingReader *reader);
const char *   axing_reader_get_namespace        (AxingReader *reader);
const char *   axing_reader_get_nsname           (AxingReader *reader);

const char *   axing_reader_get_content          (AxingReader *reader);

int            axing_reader_get_linenum          (AxingReader *reader);
int            axing_reader_get_colnum           (AxingReader *reader);

const char * const *  axing_reader_get_attrs            (AxingReader *reader);
const char *          axing_reader_get_attr_localname   (AxingReader *reader,
                                                         const char  *qname);
const char *          axing_reader_get_attr_prefix      (AxingReader *reader,
                                                         const char  *qname);
const char *          axing_reader_get_attr_namespace   (AxingReader *reader,
                                                         const char  *qname);
const char *          axing_reader_get_attr_nsname      (AxingReader *reader,
                                                         const char  *qname);
const char *          axing_reader_get_attr_value       (AxingReader *reader,
                                                         const char  *qname);
int                   axing_reader_get_attr_linenum     (AxingReader *reader,
                                                         const char  *qname);
int                   axing_reader_get_attr_colnum      (AxingReader *reader,
                                                         const char  *qname);

G_END_DECLS

#endif /* __AXING_READER_H__ */
