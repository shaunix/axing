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

#include "axing-reader.h"

G_DEFINE_INTERFACE (AxingReader, axing_reader, G_TYPE_OBJECT)

static void
axing_reader_default_init (AxingReaderInterface *iface)
{
}

gboolean
axing_reader_read (AxingReader  *reader,
                   GError      **error)
{
    g_return_val_if_fail (AXING_IS_READER (reader), FALSE);
    return AXING_READER_GET_IFACE (reader)->read (reader, error);
}

void
axing_reader_read_async (AxingReader        *reader,
                         GCancellable       *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer            user_data)
{
    g_return_if_fail (AXING_IS_READER (reader));
    AXING_READER_GET_IFACE (reader)->read_async (reader, cancellable, callback, user_data);
}

gboolean
axing_reader_read_finish (AxingReader   *reader,
                          GAsyncResult  *result,
                          GError       **error)
{
    g_return_val_if_fail (AXING_IS_READER (reader), FALSE);
    return AXING_READER_GET_IFACE (reader)->read_finish (reader, result, error);
}

AxingNodeType
axing_reader_get_node_type (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), AXING_NODE_TYPE_NONE);
    return AXING_READER_GET_IFACE (reader)->get_node_type (reader);
}

const char *
axing_reader_get_qname (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_qname (reader);
}

const char *
axing_reader_get_localname (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_localname (reader);
}

const char *
axing_reader_get_prefix (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_prefix (reader);
}

const char *
axing_reader_get_namespace (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_namespace (reader);
}

const char *
axing_reader_get_nsname (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_nsname (reader);
}

const char *
axing_reader_get_content (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_content (reader);
}

int
axing_reader_get_linenum (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), 0);
    return AXING_READER_GET_IFACE (reader)->get_linenum (reader);
}

int
axing_reader_get_colnum (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), 0);
    return AXING_READER_GET_IFACE (reader)->get_colnum (reader);
}

const char * const *
axing_reader_get_attrs (AxingReader *reader)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_attrs (reader);
}

const char *
axing_reader_get_attr_localname (AxingReader *reader,
                                 const char  *qname)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_attr_localname (reader, qname);
}

const char *
axing_reader_get_attr_prefix (AxingReader *reader,
                              const char  *qname)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_attr_prefix (reader, qname);
}

const char *
axing_reader_get_attr_namespace (AxingReader *reader,
                                 const char  *qname)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_attr_namespace (reader, qname);
}

const char *
axing_reader_get_attr_nsname (AxingReader *reader,
                              const char  *qname)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_attr_nsname (reader, qname);
}

const char *
axing_reader_get_attr_value (AxingReader *reader,
                             const char  *qname)
{
    g_return_val_if_fail (AXING_IS_READER (reader), NULL);
    return AXING_READER_GET_IFACE (reader)->get_attr_value (reader, qname);
}

int
axing_reader_get_attr_linenum (AxingReader *reader,
                               const char  *qname)
{
    g_return_val_if_fail (AXING_IS_READER (reader), 0);
    return AXING_READER_GET_IFACE (reader)->get_attr_linenum (reader, qname);
}

int
axing_reader_get_attr_colnum (AxingReader *reader,
                              const char  *qname)
{
    g_return_val_if_fail (AXING_IS_READER (reader), 0);
    return AXING_READER_GET_IFACE (reader)->get_attr_colnum (reader, qname);
}


