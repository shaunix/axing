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

#ifndef __AXING_XML_PARSER_H__
#define __AXING_XML_PARSER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "axing-resolver.h"
#include "axing-resource.h"
#include "axing-reader.h"

G_BEGIN_DECLS

#define AXING_TYPE_XML_PARSER (axing_xml_parser_get_type ())
G_DECLARE_FINAL_TYPE (AxingXmlParser, axing_xml_parser, AXING, XML_PARSER, GObject)

#define AXING_XML_PARSER_ERROR axing_xml_parser_error_quark()

typedef enum {
    AXING_XML_PARSER_ERROR_SYNTAX,
    AXING_XML_PARSER_ERROR_ENTITY,
    AXING_XML_PARSER_ERROR_BUFFER,
    AXING_XML_PARSER_ERROR_CHARSET,
    AXING_XML_PARSER_ERROR_DUPATTR,
    AXING_XML_PARSER_ERROR_UNBALANCED,
    AXING_XML_PARSER_ERROR_NS_QNAME,
    AXING_XML_PARSER_ERROR_NS_NOTFOUND,
    AXING_XML_PARSER_ERROR_NS_DUPATTR,
    AXING_XML_PARSER_ERROR_NS_INVALID,
    AXING_XML_PARSER_ERROR_OTHER
} AxingXmlParserError;

GQuark            axing_xml_parser_error_quark     (void);

AxingXmlParser *  axing_xml_parser_new             (AxingResource        *resource,
                                                    AxingResolver        *resolver);

#ifdef REFACTOR
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
#endif /* REFACTOR */

G_END_DECLS

#endif /* __AXING_XML_PARSER_H__ */
