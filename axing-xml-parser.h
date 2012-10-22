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

#ifndef __AXI_XML_PARSER_H__
#define __AXI_XML_PARSER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "axing-resolver.h"
#include "axing-resource.h"
#include "axing-stream.h"

G_BEGIN_DECLS

#define AXING_TYPE_XML_PARSER            (axing_xml_parser_get_type ())
#define AXING_XML_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), AXING_TYPE_XML_PARSER, AxingXmlParser))
#define AXING_XML_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), AXING_TYPE_XML_PARSER, AxingXmlParserClass))
#define AXING_IS_XML_PARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AXING_TYPE_XML_PARSER))
#define AXING_IS_XML_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AXING_TYPE_XML_PARSER))
#define AXING_XML_PARSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), AXING_TYPE_XML_PARSER, AxingXmlParserClass))

#define AXING_XML_PARSER_ERROR axing_xml_parser_error_quark()

typedef struct _AxingXmlParser        AxingXmlParser;
typedef struct _AxingXmlParserPrivate AxingXmlParserPrivate;
typedef struct _AxingXmlParserClass   AxingXmlParserClass;

struct _AxingXmlParser {
    AxingStream parent;

    /*< private >*/
    AxingXmlParserPrivate *priv;
};

struct _AxingXmlParserClass {
    AxingStreamClass parent_class;

    /*< private >*/
    void (*_axing_reserved1) (void);
    void (*_axing_reserved2) (void);
    void (*_axing_reserved3) (void);
    void (*_axing_reserved4) (void);
    void (*_axing_reserved5) (void);
};

typedef enum {
    AXING_XML_PARSER_ERROR_SYNTAX,
    AXING_XML_PARSER_ERROR_BUFFER,
    AXING_XML_PARSER_ERROR_CHARSET,
    AXING_XML_PARSER_ERROR_DUPATTR,
    AXING_XML_PARSER_ERROR_WRONGEND,
    AXING_XML_PARSER_ERROR_MISSINGEND,
    AXING_XML_PARSER_ERROR_NS_QNAME,
    AXING_XML_PARSER_ERROR_NS_NOTFOUND,
    AXING_XML_PARSER_ERROR_OTHER
} AxingXmlParserError;

GType             axing_xml_parser_get_type        (void);
GQuark            axing_xml_parser_error_quark     (void);

AxingXmlParser *  axing_xml_parser_new             (AxingResource        *resource,
                                                    AxingResolver        *resolver);

void              axing_xml_parser_parse           (AxingXmlParser       *parser,
                                                    GCancellable         *cancellable,
                                                    GError              **error);
void              axing_xml_parser_parse_async     (AxingXmlParser       *parser,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
void              axing_xml_parser_parse_finish    (AxingXmlParser       *parser,
                                                    GAsyncResult         *res,
                                                    GError              **error);

G_END_DECLS

#endif /* __AXING_XML_PARSER_H__ */
