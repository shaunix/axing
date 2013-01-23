/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2012-2013 Shaun McCance  <shaunm@gnome.org>
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

#include "axing-dtd-schema.h"
#include "axing-namespace-map.h"
#include "axing-private.h"
#include "axing-resource.h"
#include "axing-stream.h"
#include "axing-xml-parser.h"

#define NS_XML "http://www.w3.org/XML/1998/namespace"

typedef enum {
    PARSER_STATE_NONE,

    PARSER_STATE_START,
    PARSER_STATE_PROLOG,
    PARSER_STATE_EPILOG,

    PARSER_STATE_STELM_BASE,
    PARSER_STATE_STELM_ATTNAME,
    PARSER_STATE_STELM_ATTEQ,
    PARSER_STATE_STELM_ATTVAL,
    PARSER_STATE_ENDELM,

    PARSER_STATE_TEXT,
    PARSER_STATE_COMMENT,
    PARSER_STATE_CDATA,
    PARSER_STATE_INSTRUCTION,

    PARSER_STATE_DOCTYPE,
    PARSER_STATE_NULL
} ParserState;

typedef enum {
    DOCTYPE_STATE_START,
    DOCTYPE_STATE_NAME,
    DOCTYPE_STATE_SYSTEM,
    DOCTYPE_STATE_SYSTEM_VAL,
    DOCTYPE_STATE_PUBLIC,
    DOCTYPE_STATE_PUBLIC_VAL,
    DOCTYPE_STATE_EXTID,
    DOCTYPE_STATE_INT,
    DOCTYPE_STATE_INT_ELEMENT_START,
    DOCTYPE_STATE_INT_ELEMENT_NAME,
    DOCTYPE_STATE_INT_ELEMENT_VALUE,
    DOCTYPE_STATE_INT_ELEMENT_AFTER,
    DOCTYPE_STATE_INT_ATTLIST_START,
    DOCTYPE_STATE_INT_ATTLIST_NAME,
    DOCTYPE_STATE_INT_ATTLIST_VALUE,
    DOCTYPE_STATE_INT_ATTLIST_QUOTE,
    DOCTYPE_STATE_INT_ATTLIST_AFTER,
    DOCTYPE_STATE_INT_NOTATION_START,
    DOCTYPE_STATE_INT_NOTATION_NAME,
    DOCTYPE_STATE_INT_NOTATION_SYSTEM,
    DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL,
    DOCTYPE_STATE_INT_NOTATION_PUBLIC,
    DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL,
    DOCTYPE_STATE_INT_NOTATION_PUBLIC_AFTER,
    DOCTYPE_STATE_INT_NOTATION_AFTER,
    DOCTYPE_STATE_INT_ENTITY_START,
    DOCTYPE_STATE_INT_ENTITY_NAME,
    DOCTYPE_STATE_INT_ENTITY_VALUE,
    DOCTYPE_STATE_INT_ENTITY_PUBLIC,
    DOCTYPE_STATE_INT_ENTITY_PUBLIC_VAL,
    DOCTYPE_STATE_INT_ENTITY_SYSTEM,
    DOCTYPE_STATE_INT_ENTITY_SYSTEM_VAL,
    DOCTYPE_STATE_INT_ENTITY_NDATA,
    DOCTYPE_STATE_INT_ENTITY_AFTER,
    DOCTYPE_STATE_AFTER_INT,
    DOCTYPE_STATE_NULL
} DoctypeState;

typedef enum { XML_1_0, XML_1_1 } XmlVersion;

typedef struct {
    char       *qname;
    GHashTable *nshash;
} ParserStackFrame;

typedef struct {
    char  *qname;
    char  *value;

    /* These three may be NULL to avoid extra strdups, but
       the Stream API defines them never to be NULL when the
       attribute exists. Value must be computed if NULL. */
    char  *prefix;
    char  *localname;
    char  *namespace;

    int    linenum;
    int    colnum;
} AttributeData;

static void
attribute_data_free (AttributeData *data) {
    g_free (data->qname); g_free (data->prefix); g_free (data->localname);
    g_free (data->namespace); g_free (data->value); g_free (data);
}

typedef struct _ParserContext ParserContext;
struct _ParserContext {
    AxingXmlParser    *parser;
    AxingResource     *resource;
    GInputStream      *srcstream;
    GDataInputStream  *datastream;
    char              *basename;
    char              *entname;
    char              *showname;

    ParserState    state;
    ParserState    init_state;
    ParserState    prev_state;
    DoctypeState   doctype_state;
    int            linenum;
    int            colnum;
    int            event_stack_root;

    int            node_linenum;
    int            node_colnum;
    int            attr_linenum;
    int            attr_colnum;

    char          *pause_line;
    char          *cur_qname;
    char          *cur_attrname;
    char           quotechar;
    gboolean       empty;
    GHashTable    *cur_nshash;
    GHashTable    *cur_attrs;
    GString       *cur_text;
    char          *decl_system;
    char          *decl_public;
    gboolean       decl_pedef;
    char          *decl_ndata;

    ParserContext *parent;
};

struct _AxingXmlParserPrivate {
    gboolean    async;

    AxingResource      *resource;
    AxingResolver      *resolver;
    ParserContext      *context;
    GCancellable       *cancellable;
    GSimpleAsyncResult *result;
    GError             *error;

    XmlVersion          xml_version;

    AxingStreamEventType event_type;
    char                *event_qname;

    /* These three may be NULL to avoid extra strdups, but
       the Stream API defines them never to be NULL. Value
       must be computed if NULL. */
    char                *event_prefix;
    char                *event_localname;
    char                *event_namespace;

    GArray              *event_stack;
    char                *event_content;

    char               **event_attrkeys;
    AttributeData      **event_attrvals;

    AxingDtdSchema      *doctype;
};

static void      axing_xml_parser_init          (AxingXmlParser       *parser);
static void      axing_xml_parser_class_init    (AxingXmlParserClass  *klass);
static void      axing_xml_parser_dispose       (GObject              *object);
static void      axing_xml_parser_finalize      (GObject              *object);
static void      axing_xml_parser_get_property  (GObject              *object,
                                                 guint                 prop_id,
                                                 GValue               *value,
                                                 GParamSpec           *pspec);
static void      axing_xml_parser_set_property  (GObject              *object,
                                                 guint                 prop_id,
                                                 const GValue         *value,
                                                 GParamSpec           *pspec);

static void      parser_clean_event_data        (AxingXmlParser       *parser);

static AxingStreamEventType  stream_get_event_type          (AxingStream    *stream);
static const char *          stream_get_event_qname         (AxingStream    *stream);
static const char *          stream_get_event_prefix        (AxingStream    *stream);
static const char *          stream_get_event_localname     (AxingStream    *stream);
static const char *          stream_get_event_namespace     (AxingStream    *stream);
static const char *          stream_get_event_content       (AxingStream    *stream);
const char * const *         stream_get_attributes          (AxingStream    *stream);
const char *                 stream_get_attribute_localname (AxingStream    *stream,
                                                             const char     *qname);
const char *                 stream_get_attribute_prefix    (AxingStream    *stream,
                                                             const char     *qname);
const char *                 stream_get_attribute_namespace (AxingStream    *stream,
                                                             const char     *qname);
const char *                 stream_get_attribute_value     (AxingStream    *stream,
                                                             const char     *name,
                                                             const char     *ns);


static void         namespace_map_interface_init (AxingNamespaceMapInterface *iface);
static const char * namespace_map_get_namespace  (AxingNamespaceMap   *map,
                                                  const char          *prefix);

static void      context_resource_read_cb       (AxingResource        *resource,
                                                 GAsyncResult         *result,
                                                 ParserContext        *context);
static void      context_parse_sync             (ParserContext        *context);
static void      context_start_async            (ParserContext        *context);
static void      context_start_cb               (GBufferedInputStream *stream,
                                                 GAsyncResult         *res,
                                                 ParserContext        *context);
static void      context_read_data_cb           (GDataInputStream     *stream,
                                                 GAsyncResult         *res,
                                                 ParserContext        *context);
static void      context_check_end              (ParserContext        *context);
static void      context_parse_xml_decl         (ParserContext        *context);
static void      context_parse_data             (ParserContext        *context,
                                                 char                 *line);
static void      context_parse_doctype          (ParserContext        *context,
                                                 char                **line);
static void      context_parse_doctype_element  (ParserContext        *context,
                                                 char                **line);
static void      context_parse_doctype_attlist  (ParserContext        *context,
                                                 char                **line);
static void      context_parse_doctype_notation (ParserContext        *context,
                                                 char                **line);
static void      context_parse_doctype_entity   (ParserContext        *context,
                                                 char                **line);
static void      context_parse_parameter        (ParserContext        *context,
                                                 char                **line);
static void      context_parse_cdata            (ParserContext        *context,
                                                 char                **line);
static void      context_parse_comment          (ParserContext        *context,
                                                 char                **line);
static void      context_parse_instruction      (ParserContext        *context,
                                                 char                **line);
static void      context_parse_end_element      (ParserContext        *context,
                                                 char                **line);
static void      context_parse_start_element    (ParserContext        *context,
                                                 char                **line);
static void      context_parse_attrs            (ParserContext        *context,
                                                 char                **line);
static void      context_parse_entity           (ParserContext        *context,
                                                 char                **line);
static void      context_parse_text             (ParserContext        *context,
                                                 char                **line);
static void      context_trigger_start_element  (ParserContext        *context);
static void      context_complete               (ParserContext        *context);
static void      context_process_entity         (ParserContext        *context,
                                                 const char           *entname,
                                                 char                **line);
static void      context_process_entity_resolved(AxingResolver        *resolver,
                                                 GAsyncResult         *result,
                                                 ParserContext        *context);
static void      context_process_entity_finish  (ParserContext        *contetext);


static char *    resource_get_basename          (AxingResource        *resource);

static ParserContext * context_new              (AxingXmlParser       *parser);
static void            context_free             (ParserContext        *context);


enum {
    PROP_0,
    PROP_RESOURCE,
    PROP_RESOLVER,
    N_PROPS
};

G_DEFINE_TYPE_WITH_CODE (AxingXmlParser, axing_xml_parser, AXING_TYPE_STREAM,
                         G_IMPLEMENT_INTERFACE (AXING_TYPE_NAMESPACE_MAP,
                                                namespace_map_interface_init));

static void
axing_xml_parser_init (AxingXmlParser *parser)
{
    parser->priv = G_TYPE_INSTANCE_GET_PRIVATE (parser, AXING_TYPE_XML_PARSER,
                                                AxingXmlParserPrivate);

    parser->priv->event_stack = g_array_new (FALSE, FALSE, sizeof(ParserStackFrame));
}

static void
axing_xml_parser_class_init (AxingXmlParserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    AxingStreamClass *stream_class = AXING_STREAM_CLASS (klass);

    g_type_class_add_private (klass, sizeof (AxingXmlParserPrivate));

    stream_class->get_event_type = stream_get_event_type;
    stream_class->get_event_qname = stream_get_event_qname;
    stream_class->get_event_prefix = stream_get_event_prefix;
    stream_class->get_event_localname = stream_get_event_localname;
    stream_class->get_event_namespace = stream_get_event_namespace;
    stream_class->get_event_content = stream_get_event_content;

    stream_class->get_attributes = stream_get_attributes;
    stream_class->get_attribute_localname = stream_get_attribute_localname;
    stream_class->get_attribute_prefix = stream_get_attribute_prefix;
    stream_class->get_attribute_namespace = stream_get_attribute_namespace;
    stream_class->get_attribute_value = stream_get_attribute_value;

    object_class->get_property = axing_xml_parser_get_property;
    object_class->set_property = axing_xml_parser_set_property;
    object_class->dispose = axing_xml_parser_dispose;
    object_class->finalize = axing_xml_parser_finalize;

    g_object_class_install_property (object_class, PROP_RESOURCE,
                                     g_param_spec_object ("resource",
                                                          N_("resource"),
                                                          N_("The AxingResource to parse data from"),
                                                          AXING_TYPE_RESOURCE,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_RESOURCE,
                                     g_param_spec_object ("resolver",
                                                          N_("resolver"),
                                                          N_("The AxingResolver to use to resolve references"),
                                                          AXING_TYPE_RESOLVER,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#ifdef FIXME
transport-encoding
declared-encoding
#endif
}

static void
axing_xml_parser_dispose (GObject *object)
{
    AxingXmlParser *parser = AXING_XML_PARSER (object);

    if (parser->priv->resource) {
        g_object_unref (parser->priv->resource);
        parser->priv->resource = NULL;
    }

    if (parser->priv->resolver) {
        g_object_unref (parser->priv->resolver);
        parser->priv->resolver = NULL;
    }

    if (parser->priv->context) {
        context_free (parser->priv->context);
        parser->priv->context = NULL;
    }

    if (parser->priv->cancellable) {
        g_object_unref (parser->priv->cancellable);
        parser->priv->cancellable = NULL;
    }

    if (parser->priv->result) {
        g_object_unref (parser->priv->result);
        parser->priv->result = NULL;
    }

    if (parser->priv->doctype) {
        g_object_unref (parser->priv->doctype);
        parser->priv->doctype = NULL;
    }

    G_OBJECT_CLASS (axing_xml_parser_parent_class)->dispose (object);
}

static void
axing_xml_parser_finalize (GObject *object)
{
    AxingXmlParser *parser = AXING_XML_PARSER (object);

    g_clear_error (&(parser->priv->error));

    parser_clean_event_data (parser);

    if (parser->priv->event_stack)
        g_array_free (parser->priv->event_stack, TRUE);

    G_OBJECT_CLASS (axing_xml_parser_parent_class)->finalize (object);
}

static void
axing_xml_parser_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    AxingXmlParser *parser = AXING_XML_PARSER (object);
    switch (prop_id) {
    case PROP_RESOURCE:
        g_value_set_object (value, parser->priv->resource);
        break;
    case PROP_RESOLVER:
        g_value_set_object (value, parser->priv->resolver);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
axing_xml_parser_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    AxingXmlParser *parser = AXING_XML_PARSER (object);
    switch (prop_id) {
    case PROP_RESOURCE:
        if (parser->priv->resource)
            g_object_unref (parser->priv->resource);
        parser->priv->resource = AXING_RESOURCE (g_value_dup_object (value));
        break;
    case PROP_RESOLVER:
        if (parser->priv->resolver)
            g_object_unref (parser->priv->resolver);
        parser->priv->resolver = AXING_RESOLVER (g_value_dup_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

GQuark
axing_xml_parser_error_quark (void)
{
    return g_quark_from_static_string ("axing-parser-error-quark");
}

AxingXmlParser *
axing_xml_parser_new (AxingResource *resource,
                      AxingResolver *resolver)
{
    return g_object_new (AXING_TYPE_XML_PARSER,
                         "resource", resource,
                         NULL);
}

static void
namespace_map_interface_init (AxingNamespaceMapInterface *iface)
{
    iface->get_namespace = namespace_map_get_namespace;
}

static const char *
namespace_map_get_namespace (AxingNamespaceMap *map,
                             const char        *prefix)
{
    AxingXmlParser *parser = AXING_XML_PARSER (map);
    ParserStackFrame frame;
    int i;
    if (g_str_equal (prefix, "xml")) {
        return NS_XML;
    }
    for (i = parser->priv->event_stack->len - 1; i >= 0; i--) {
        frame = g_array_index (parser->priv->event_stack, ParserStackFrame, i);
        if (frame.nshash != NULL) {
            const char *ns = g_hash_table_lookup (frame.nshash, prefix);
            if (ns != NULL) {
                return ns;
            }
        }
    }
    return NULL;
}

static void
parser_clean_event_data (AxingXmlParser *parser)
{
    g_free (parser->priv->event_qname);
    parser->priv->event_qname = NULL;
    g_free (parser->priv->event_prefix);
    parser->priv->event_prefix = NULL;
    g_free (parser->priv->event_localname);
    parser->priv->event_localname = NULL;
    g_free (parser->priv->event_namespace);
    parser->priv->event_namespace = NULL;
    g_free (parser->priv->event_content);
    parser->priv->event_content = NULL;

    if (parser->priv->event_attrvals != NULL) {
        gint i;
        for (i = 0; parser->priv->event_attrvals[i] != NULL; i++)
            attribute_data_free (parser->priv->event_attrvals[i]);
        g_free (parser->priv->event_attrvals);
        parser->priv->event_attrvals = NULL;
    }
    /* strings owned by AttributeData structs */
    g_free (parser->priv->event_attrkeys);
    parser->priv->event_attrkeys = NULL;

    parser->priv->event_type = AXING_STREAM_EVENT_NONE;
}

static AxingStreamEventType
stream_get_event_type (AxingStream *stream)
{
    return AXING_XML_PARSER (stream)->priv->event_type;
}

static const char *
stream_get_event_qname (AxingStream *stream)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_INSTRUCTION,
                          NULL);
    return parser->priv->event_qname;
}

static const char *
stream_get_event_content (AxingStream *stream)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_CONTENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_COMMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_CDATA   ||
                          parser->priv->event_type == AXING_STREAM_EVENT_INSTRUCTION,
                          NULL);
    g_return_val_if_fail (parser->priv->event_type != AXING_STREAM_EVENT_NONE, NULL);
    return parser->priv->event_content;
}

static const char *
stream_get_event_prefix (AxingStream *stream)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_INSTRUCTION,
                          NULL);
    return parser->priv->event_prefix ? parser->priv->event_prefix : "";
}

static const char *
stream_get_event_localname (AxingStream *stream)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_INSTRUCTION,
                          NULL);
    return parser->priv->event_localname ? parser->priv->event_localname : parser->priv->event_qname;
}

static const char *
stream_get_event_namespace (AxingStream *stream)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT ||
                          parser->priv->event_type == AXING_STREAM_EVENT_INSTRUCTION,
                          NULL);
    return parser->priv->event_namespace ? parser->priv->event_namespace : "";
}

static char *nokeys[1] = {NULL};

const char * const *
stream_get_attributes (AxingStream *stream)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT, NULL);
    if (parser->priv->event_attrkeys == NULL) {
        return (const char * const *) nokeys;
    }
    return (const char * const *) parser->priv->event_attrkeys;
}

const char *
stream_get_attribute_localname (AxingStream *stream,
                                const char  *qname)
{
    AxingXmlParser *parser;
    int i;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT, NULL);
    for (i = 0; parser->priv->event_attrvals[i] != NULL; i++) {
        AttributeData *data = parser->priv->event_attrvals[i];
        if (g_str_equal (qname, data->qname)) {
            return data->localname ? data->localname : data->qname;
        }
    }
    return NULL;
}

const char *
stream_get_attribute_prefix (AxingStream *stream,
                             const char  *qname)
{
    AxingXmlParser *parser;
    int i;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT, NULL);
    for (i = 0; parser->priv->event_attrvals[i] != NULL; i++) {
        AttributeData *data = parser->priv->event_attrvals[i];
        if (g_str_equal (qname, data->qname)) {
            return data->prefix ? data->prefix : "";
        }
    }
    return NULL;
}

const char *
stream_get_attribute_namespace (AxingStream *stream,
                                const char  *qname)
{
    AxingXmlParser *parser;
    int i;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT, NULL);
    for (i = 0; parser->priv->event_attrvals[i] != NULL; i++) {
        AttributeData *data = parser->priv->event_attrvals[i];
        if (g_str_equal (qname, data->qname)) {
            return data->namespace ? data->namespace : "";
        }
    }
    return NULL;
}

const char *
stream_get_attribute_value (AxingStream *stream,
                            const char  *name,
                            const char  *ns)
{
    AxingXmlParser *parser;
    int i;
    g_return_val_if_fail (AXING_IS_XML_PARSER (stream), NULL);
    parser = (AxingXmlParser *) stream;
    g_return_val_if_fail (parser->priv->event_type == AXING_STREAM_EVENT_START_ELEMENT, NULL);
    for (i = 0; parser->priv->event_attrvals[i] != NULL; i++) {
        AttributeData *data = parser->priv->event_attrvals[i];
        if (ns == NULL) {
            if (g_str_equal (name, data->qname))
                return data->value;
        }
        else {
            if (g_str_equal (name, data->localname) &&
                ((ns[0] == '\0') ? data->namespace == NULL : g_str_equal (ns, data->namespace)))
                return data->value;
        }
    }
    return NULL;
}

static void
axing_xml_parser_parse_init (AxingXmlParser *parser,
                             GCancellable   *cancellable)
{
    GFile *file;

    parser->priv->context = context_new (parser);
    parser->priv->context->state = PARSER_STATE_START;

    parser->priv->context->resource = g_object_ref (parser->priv->resource);

    if (cancellable)
        parser->priv->cancellable = g_object_ref (cancellable);

    parser->priv->context->basename = resource_get_basename (parser->priv->resource);
}

void
axing_xml_parser_parse (AxingXmlParser  *parser,
                        GCancellable    *cancellable,
                        GError         **error)
{
    g_return_if_fail (parser->priv->context == NULL);

    axing_xml_parser_parse_init (parser, cancellable);

    parser->priv->async = FALSE;

    context_parse_sync (parser->priv->context);

    context_free (parser->priv->context);
    parser->priv->context = NULL;
    if (parser->priv->error) {
        if (error != NULL)
            *error = parser->priv->error;
        else
            g_error_free (parser->priv->error);
        parser->priv->error = NULL;
    }
}

static void
context_parse_sync (ParserContext *context)
{
    char *line;

    context->srcstream = axing_resource_read (context->resource,
                                              context->parser->priv->cancellable,
                                              &(context->parser->priv->error));
    if (context->parser->priv->error)
        goto error;

    context->datastream = g_data_input_stream_new (context->srcstream);
    if (context->state == PARSER_STATE_START) {
        g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (context->datastream),
                                      1024,
                                      context->parser->priv->cancellable,
                                      &(context->parser->priv->error));
        if (context->parser->priv->error)
            goto error;
        context_parse_xml_decl (context);
        if (context->parser->priv->error)
            goto error;
    }

    g_data_input_stream_set_newline_type (context->datastream,
                                          G_DATA_STREAM_NEWLINE_TYPE_ANY);
    while ((line = g_data_input_stream_read_upto (context->datastream, " ", -1, NULL,
                                                  context->parser->priv->cancellable,
                                                  &(context->parser->priv->error)) )) {
        if (context->parser->priv->error)
            goto error;
        context_parse_data (context, line);
        g_free (line);
        if (context->parser->priv->error)
            goto error;
        if (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (context->datastream)) > 0) {
            char eol[2] = {0, 0};
            eol[0] = g_data_input_stream_read_byte (context->datastream,
                                                    context->parser->priv->cancellable,
                                                    &(context->parser->priv->error));
            if (context->parser->priv->error)
                goto error;
            context_parse_data (context, eol);
            if (context->parser->priv->error)
                goto error;
        }
    }
    if (line == NULL && context->parser->priv->error == NULL)
        context_check_end (context);

 error:
    return;
}

void
axing_xml_parser_parse_async (AxingXmlParser      *parser,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
    ParserContext *context;
    GFile *file;
    GInputStream *stream;

    g_return_if_fail (parser->priv->context == NULL);

    axing_xml_parser_parse_init (parser, cancellable);

    parser->priv->async = TRUE;
    parser->priv->result = g_simple_async_result_new (G_OBJECT (parser),
                                                      callback, user_data,
                                                      axing_xml_parser_parse_async);

    axing_resource_read_async (parser->priv->resource,
                               parser->priv->cancellable,
                               (GAsyncReadyCallback) context_resource_read_cb,
                               parser->priv->context);
}

void
axing_xml_parser_parse_finish (AxingXmlParser *parser,
                               GAsyncResult   *res,
                               GError        **error)
{
    g_warn_if_fail (g_simple_async_result_get_source_tag (G_SIMPLE_ASYNC_RESULT (res)) == axing_xml_parser_parse_async);

    if (parser->priv->context) {
        context_free (parser->priv->context);
        parser->priv->context = NULL;
    }

    if (parser->priv->error) {
        if (error != NULL)
            *error = parser->priv->error;
        else
            g_error_free (parser->priv->error);
        parser->priv->error = NULL;
    }
}

#define XML_IS_CHAR(cp, context) ((context->parser->priv->xml_version == XML_1_1) ? (cp == 0x09 || cp == 0x0A || cp == 0x0D || (cp >= 0x20 && cp <= 0x7E) || cp == 0x85 || (cp  >= 0xA0 && cp <= 0xD7FF) || (cp >= 0xE000 && cp <= 0xFFFD) || (cp >= 0x10000 && cp <= 0x10FFFF)) : (cp == 0x9 || cp == 0x0A || cp == 0x0D || (cp >= 0x20 && cp <= 0xD7FF) || (cp >= 0xE000 && cp <= 0xFFFD) || (cp >= 0x10000 && cp <= 0x10FFFF)))

#define XML_IS_CHAR_RESTRICTED(cp, context) ((context->parser->priv->xml_version == XML_1_1) && ((cp >= 0x1 && cp <= 0x8) || (cp >= 0xB && cp <= 0xC) || (cp >= 0xE && cp <= 0x1F) || (cp >= 0x7F && cp <= 0x84) || (cp >= 0x86 && cp <= 0x9F)))

#define IS_1_1(context) (context->state != PARSER_STATE_START && context->parser->priv->xml_version == XML_1_1)

#define XML_IS_SPACE(line, context)                                     \
    ((line)[0] == 0x20 || (line)[0] == 0x09 ||                          \
     (line)[0] == 0x0D || (line)[0] == 0x0A ||                          \
     (IS_1_1(context) &&                                                \
      ( ((guchar)(line)[0] == 0xC2 && (guchar)(line)[1] == 0x85) ||     \
        ((guchar)(line)[0] == 0xE2 && (guchar)(line)[1] == 0x80 &&      \
         (guchar)(line)[2] == 0xA8)                                     \
        )))

#define XML_IS_NAME_START_BYTE(c) (c == ':' || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z'))

#define XML_IS_NAME_START_CHAR(cp) (XML_IS_NAME_START_BYTE(cp) || (cp >= 0xC0 && cp <= 0xD6) || (cp >= 0xD8 && cp <= 0xF6) || (cp >= 0xF8 && cp <= 0x2FF) || (cp >= 0x370 && cp <= 0x37D) || (cp >= 0x37F && cp <= 0x1FFF) || (cp >= 0x200C && cp <= 0x200D) || (cp >= 0x2070 && cp <= 0x218F) || (cp >= 0x2C00 && cp <= 0x2FEF) || (cp >= 0x3001 && cp <= 0xD7FF) || (cp >= 0xF900 && cp <= 0xFDCF) || (cp >= 0xFDF0 && cp <= 0xFFFD) || (cp >= 0x10000 && cp <= 0xEFFFF))

#define XML_IS_NAME_BYTE(c) (XML_IS_NAME_START_BYTE(c) || c == '-' || c == '.' || (c >= '0' && c <= '9'))

#define XML_IS_NAME_CHAR(cp) (XML_IS_NAME_START_CHAR(cp) || cp == '-' || cp == '.' || (cp >= '0' && cp <= '9') || cp == 0xB7 || (cp >= 0x300 && cp <= 0x36F) || (cp >= 0x203F && cp <= 0x2040))

#define XML_GET_NAME(line, namevar, context) {                          \
        GString *name;                                                  \
        if ((guchar)(*line)[0] < 0x80) {                                \
            if (!XML_IS_NAME_START_BYTE((*line)[0]))                    \
                ERROR_SYNTAX(context);                                  \
        }                                                               \
        else {                                                          \
            gunichar cp = g_utf8_get_char (*line);                      \
            if (!XML_IS_NAME_START_CHAR (cp))                           \
                ERROR_SYNTAX(context);                                  \
        }                                                               \
        name = g_string_new (NULL);                                     \
        while ((*line)[0]) {                                            \
            if ((guchar)(*line)[0] < 0x80) {                            \
                if (!XML_IS_NAME_BYTE ((*line)[0]))                     \
                    break;                                              \
                g_string_append_c (name, (*line)[0]);                   \
                (*line)++; context->colnum++;                           \
            }                                                           \
            else {                                                      \
                gunichar cp;                                            \
                char *next;                                             \
                gsize bytes;                                            \
                cp = g_utf8_get_char (*line);                           \
                if (!XML_IS_NAME_CHAR (cp))                             \
                    break;                                              \
                next = g_utf8_next_char (*line);                        \
                bytes = next - *line;                                   \
                g_string_append_len (name, *line, bytes);               \
                *line = next; context->colnum += 1;                     \
            }                                                           \
        }                                                               \
        namevar = g_string_free (name, FALSE);                          \
    }

#define ERROR_SYNTAX(context) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_SYNTAX, "%s:%i:%i:Syntax error.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }

#define ERROR_DUPATTR(context) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_DUPATTR, "%s:%i:%i:Duplicate attribute \"%s\".", context->showname ? context->showname : context->basename, context->attr_linenum, context->attr_colnum, context->cur_attrname); goto error; }

#define ERROR_MISSINGEND(context, qname) { context->parser->priv->error = g_error_new (AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_MISSINGEND, "%s:%i:%i:Missing end tag for \"%s\".", context->showname ? context->showname : context->basename, context->linenum, context->colnum, qname); goto error; }

#define ERROR_EXTRACONTENT(context) { context->parser->priv->error = g_error_new (AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_EXTRACONTENT, "%s:%i:%i:Extra content at end of resource.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }

#define ERROR_WRONGEND(context) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_WRONGEND, "%s:%i:%i:Incorrect end tag \"%s\".", context->showname ? context->showname : context->basename, context->node_linenum, context->node_colnum, context->parser->priv->event_qname); goto error; }

#define ERROR_ENTITY(context) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_ENTITY, "%s:%i:%i:Incorrect entity reference.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }

#define ERROR_NS_QNAME(context) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_QNAME, "%s:%i:%i:Could not parse QName \"%s\".", context->showname ? context->showname : context->basename, context->node_linenum, context->node_colnum, context->parser->priv->event_qname); goto error; }

#define ERROR_NS_QNAME_ATTR(context, data) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_QNAME, "%s:%i:%i:Could not parse QName \"%s\".", context->showname ? context->showname : context->basename, data->linenum, data->colnum, data->qname); goto error; }

#define ERROR_NS_NOTFOUND(context) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_NOTFOUND, "%s:%i:%i:Could not find namespace for prefix \"%s\".", context->showname ? context->showname : context->basename, context->node_linenum, context->node_colnum, context->parser->priv->event_prefix); goto error; }

#define ERROR_NS_NOTFOUND_ATTR(context, data) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_NOTFOUND, "%s:%i:%i:Could not find namespace for prefix \"%s\".", context->showname ? context->showname : context->basename, data->linenum, data->colnum, data->prefix); goto error; }

#define ERROR_NS_DUPATTR(context, data) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_DUPATTR, "%s:%i:%i:Duplicate expanded name for attribute \"%s\".", context->showname ? context->showname : context->basename, data->linenum, data->colnum, data->qname); goto error; }

#define ERROR_NS_INVALID(context, prefix) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_INVALID, "%s:%i:%i:Invalid namespace for prefix \"%s\".", context->showname ? context->showname : context->basename, context->attr_linenum, context->attr_colnum, prefix); goto error; }

#define ERROR_FIXME(context) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_OTHER, "%s:%i:%i:Unsupported feature.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }

#define EAT_SPACES(line, buf, bufsize, context)                         \
    while((bufsize < 0 || (line) - buf < bufsize)) {                    \
        if ((line)[0] == 0x20 || (line)[0] == 0x09)                     \
            { (line)++; context->colnum++; }                            \
        else if ((line)[0] == 0x0A)                                     \
            { (line)++; context->colnum = 1; context->linenum++; }      \
        else if ((line)[0] == 0x0D) {                                   \
            (line)++; context->colnum = 1; context->linenum++;          \
            if ((line)[0] == 0x0A)                                      \
                (line)++;                                               \
            else if (IS_1_1(context) &&                                 \
                     (guchar)(line)[0] == 0xC2 &&                       \
                     (guchar)(line)[1] == 0x85)                         \
                line = line + 2;                                        \
        }                                                               \
        else if (IS_1_1(context) &&                                     \
                 (guchar)(line)[0] == 0xC2 &&                           \
                 (guchar)(line)[1] == 0x85) {                           \
            line = line + 2; context->colnum = 1; context->linenum++;   \
        }                                                               \
        else if (IS_1_1(context) &&                                     \
                 (guchar)(line)[0] == 0xE2 &&                           \
                 (guchar)(line)[1] == 0x80 &&                           \
                 (guchar)(line)[2] == 0xA8) {                           \
            line = line + 3; context->colnum = 1; context->linenum++;   \
        }                                                               \
        else                                                            \
            break;                                                      \
    }

#define APPEND_CHAR(line, context, check)                               \
    if (check) {                                                        \
        if ((guchar)(*line)[0] < 0x80) {                                \
            if (!((*line)[0] == 0x09 || (*line)[0] == 0x0A ||           \
                  (*line)[0] == 0x0D || (*line)[0] >= 0x20 ))           \
                ERROR_SYNTAX(context);                                  \
        }                                                               \
        else {                                                          \
            gunichar cp = g_utf8_get_char (*line);                      \
            if (!XML_IS_CHAR (cp, context))                             \
                ERROR_SYNTAX(context);                                  \
        }                                                               \
    }                                                                   \
    if ((*line)[0] == 0x0A) {                                           \
        g_string_append_c (context->cur_text, 0x0A);                    \
        (*line)++; context->linenum++; context->colnum = 1;             \
    }                                                                   \
    else if ((*line)[0] == 0x0D) {                                      \
        g_string_append_c (context->cur_text, 0x0A);                    \
        (*line)++; context->linenum++; context->colnum = 1;             \
        if ((*line)[0] == 0x0A)                                         \
            (*line)++;                                                  \
        else if (IS_1_1(context) &&                                     \
                 (guchar)(*line)[0] == 0xC2 &&                          \
                 (guchar)(*line)[1] == 0x85)                            \
            *line = *line + 2;                                          \
    }                                                                   \
    else if (IS_1_1(context) &&                                         \
             (guchar)(*line)[0] == 0xC2 &&                              \
             (guchar)(*line)[1] == 0x85) {                              \
        g_string_append_c (context->cur_text, 0x0A);                    \
        *line = *line + 2; context->colnum = 1; context->linenum++;     \
    }                                                                   \
    else if (IS_1_1(context) &&                                         \
             (guchar)(*line)[0] == 0xE2 &&                              \
             (guchar)(*line)[1] == 0x80 &&                              \
             (guchar)(*line)[2] == 0xA8) {                              \
        g_string_append_c (context->cur_text, 0x0A);                    \
        *line = *line + 3; context->colnum = 1; context->linenum++;     \
    }                                                                   \
    else if ((guchar)(*line)[0] < 0x80) {                               \
        g_string_append_c (context->cur_text, (*line)[0]);              \
        (*line)++; context->colnum++;                                   \
    }                                                                   \
    else {                                                              \
        char *next;                                                     \
        gsize bytes;                                                    \
        next = g_utf8_next_char (*line);                                \
        bytes = next - *line;                                           \
        g_string_append_len (context->cur_text, *line, bytes);          \
        *line = next; context->colnum++;                                \
    }

#define CHECK_BUFFER(c, num, buf, bufsize, context) if (c - buf + num > bufsize) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_BUFFER, "Insufficient buffer for XML declaration\n"); goto error; }

#define READ_TO_QUOTE(c, buf, bufsize, context, quot) EAT_SPACES (c, buf, bufsize, context); CHECK_BUFFER (c, 1, buf, bufsize, context); if (c[0] != '=') { ERROR_SYNTAX(context); } c += 1; context->colnum += 1; EAT_SPACES (c, buf, bufsize, context); CHECK_BUFFER (c, 1, buf, bufsize, context); if (c[0] != '"' && c[0] != '\'') { ERROR_SYNTAX(context); } quot = c[0]; c += 1; context->colnum += 1;


static void
context_resource_read_cb (AxingResource *resource,
                          GAsyncResult  *result,
                          ParserContext *context)
{
    context->srcstream = G_INPUT_STREAM (axing_resource_read_finish (resource, result,
                                                                     &(context->parser->priv->error)));
    if (context->parser->priv->error) {
        context_complete (context);
        return;
    }
    context_start_async (context);
}

static void
context_start_async (ParserContext *context)
{
    context->datastream = g_data_input_stream_new (context->srcstream);

    g_buffered_input_stream_fill_async (G_BUFFERED_INPUT_STREAM (context->datastream),
                                        1024,
                                        G_PRIORITY_DEFAULT,
                                        context->parser->priv->cancellable,
                                        (GAsyncReadyCallback) context_start_cb,
                                        context);
}

static void
context_start_cb (GBufferedInputStream *stream,
                  GAsyncResult         *res,
                  ParserContext        *context)
{
    g_buffered_input_stream_fill_finish (stream, res, NULL);
    if (context->state == PARSER_STATE_START) {
        context_parse_xml_decl (context);
        if (context->parser->priv->error) {
            context_complete (context);
            return;
        }
    }
    g_data_input_stream_set_newline_type (context->datastream,
                                          G_DATA_STREAM_NEWLINE_TYPE_ANY);
    g_data_input_stream_read_upto_async (context->datastream,
                                         " ", -1,
                                         G_PRIORITY_DEFAULT,
                                         context->parser->priv->cancellable,
                                         (GAsyncReadyCallback) context_read_data_cb,
                                         context);
}

static void
context_read_data_cb (GDataInputStream *stream,
                      GAsyncResult     *res,
                      ParserContext    *context)
{
    gchar *line;
    if (context->parser->priv->error) {
        context_complete (context);
        return;
    }
    if (context->pause_line) {
        line = context->pause_line;
        context->pause_line = NULL;
        context_parse_data (context, line);
        g_free (line);
    }
    else {
        line = g_data_input_stream_read_upto_finish (stream, res, NULL, &(context->parser->priv->error));
        if (line == NULL && context->parser->priv->error == NULL) {
            /* https://bugzilla.gnome.org/show_bug.cgi?id=692101
               g_data_input_stream_read_upto_finish returns NULL when it should return ""
               when at one of the stop chars. This happens, e.g., when there are two stop
               chars in a row. Checking the buffer is a workaround. The buffer will always
               be non-empty for these false NULLs, because at least the one stop char must
               be in the buffer for this to happen.
             */
            if (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (context->datastream)) > 0)
                goto bug692101;
            context_check_end (context);
        }
        if (line == NULL || context->parser->priv->error) {
            context_complete (context);
            return;
        }
        context_parse_data (context, line);
        g_free (line);
        if (context->parser->priv->error) {
            context_complete (context);
            return;
        }
    bug692101:
        if (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (context->datastream)) > 0) {
            char eol[2] = {0, 0};
            eol[0] = g_data_input_stream_read_byte (context->datastream,
                                                    context->parser->priv->cancellable,
                                                    &(context->parser->priv->error));
            if (context->parser->priv->error) {
                context_complete (context);
                return;
            }
            context_parse_data (context, eol);
            if (context->parser->priv->error) {
                context_complete (context);
                return;
            }
        }
    }
    if (context->pause_line == NULL)
        g_data_input_stream_read_upto_async (context->datastream,
                                             " ", -1,
                                             G_PRIORITY_DEFAULT,
                                             context->parser->priv->cancellable,
                                             (GAsyncReadyCallback) context_read_data_cb,
                                             context);
}

static void
context_check_end (ParserContext *context)
{
    if (context->parser->priv->event_stack->len != context->event_stack_root) {
        ParserStackFrame frame = g_array_index (context->parser->priv->event_stack,
                                                ParserStackFrame,
                                                context->parser->priv->event_stack->len - 1);
        ERROR_MISSINGEND(context, frame.qname);
    }
    if (context->state != context->init_state &&
        !(context->state == PARSER_STATE_EPILOG && context->init_state == PARSER_STATE_PROLOG)) {
        ERROR_SYNTAX(context);
    }
 error:
    return;
}

static void
context_parse_xml_decl (ParserContext *context)
{
    int i;
    gsize bufsize;
    char *buf, *c;
    char quot;
    char *encoding = NULL;
    buf = (char *) g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (context->datastream), &bufsize);

    g_return_if_fail (context->state == PARSER_STATE_START);

    if (!(bufsize >= 6 && !strncmp(buf, "<?xml", 5) && XML_IS_SPACE(buf + 5, context) )) {
        return;
    }
    
    c = buf + 6;
    context->colnum += 6;
    EAT_SPACES (c, buf, bufsize, context);

    CHECK_BUFFER (c, 8, buf, bufsize, context);
    if (!(!strncmp(c, "version", 7) && (c[7] == '=' || XML_IS_SPACE(c + 7, context)) )) {
        ERROR_SYNTAX(context);
    }
    c += 7; context->colnum += 7;

    READ_TO_QUOTE (c, buf, bufsize, context, quot);

    CHECK_BUFFER (c, 4, buf, bufsize, context);
    if (c[0] == '1' && c[1] == '.' && (c[2] == '0' || c[2] == '1') && c[3] == quot) {
        context->parser->priv->xml_version = (c[2] == '0') ? XML_1_0 : XML_1_1;
    }
    else {
        ERROR_SYNTAX(context);
    }
    c += 4; context->colnum += 4;

    EAT_SPACES (c, buf, bufsize, context);
    CHECK_BUFFER (c, 1, buf, bufsize, context);
    if (c[0] == 'e') {
        GString *enc;
        CHECK_BUFFER (c, 9, buf, bufsize, context);
        if (!(!strncmp(c, "encoding", 8) && (c[8] == '=' || XML_IS_SPACE(c + 8, context)) )) {
            ERROR_SYNTAX(context);
        }
        c += 8; context->colnum += 8;

        READ_TO_QUOTE (c, buf, bufsize, context, quot);

        CHECK_BUFFER (c, 1, buf, bufsize, context);
        if (!((c[0] >= 'A' && c[0] <= 'Z') || (c[0] >= 'a' && c[0] <= 'z'))) {
            ERROR_SYNTAX(context);
        }

        enc = g_string_sized_new (16);
        while ((c[0] >= 'A' && c[0] <= 'Z') || (c[0] >= 'a' && c[0] <= 'z') ||
               (c[0] >= '0' && c[0] <= '9') || c[0] == '-' || c[0] == '.' || c[0] == '_') {
            CHECK_BUFFER (c, 2, buf, bufsize, context);
            g_string_append_c (enc, c[0]);
            c += 1; context->colnum += 1;
        }
        if (c[0] != quot) {
            ERROR_SYNTAX(context);
        }
        c += 1; context->colnum += 1;

        encoding = g_string_free (enc, FALSE);
    }

    EAT_SPACES (c, buf, bufsize, context);
    CHECK_BUFFER (c, 1, buf, bufsize, context);
    if (c[0] == 's') {
        CHECK_BUFFER (c, 11, buf, bufsize, context);
        if (!(!strncmp(c, "standalone", 10) && (c[10] == '=' || XML_IS_SPACE(c + 10, context)) )) {
            ERROR_SYNTAX(context);
        }
        c += 10; context->colnum += 10;

        READ_TO_QUOTE (c, buf, bufsize, context, quot);

        CHECK_BUFFER (c, 3, buf, bufsize, context);
        if (c[0] == 'y' && c[1] == 'e' && c[2] == 's') {
            c += 3; context->colnum += 3;
        }
        else if (c[0] == 'n' && c[1] == 'o') {
            c += 2; context->colnum += 2;
        }
        else {
            ERROR_SYNTAX(context);
        }
        if (c[0] != quot) {
            ERROR_SYNTAX(context);
        }
        c += 1; context->colnum += 1;
    }

    EAT_SPACES (c, buf, bufsize, context);
    CHECK_BUFFER (c, 2, buf, bufsize, context);
    if (!(c[0] == '?' && c[1] == '>')) {
        ERROR_SYNTAX(context);
    }
    c += 2; context->colnum += 2;

    g_input_stream_skip (G_INPUT_STREAM (context->datastream), c - buf, NULL, NULL);

    if (encoding != NULL) {
        GConverter *converter;
        GInputStream *cstream;
        converter = (GConverter *) g_charset_converter_new ("utf-8", encoding, NULL);
        if (converter == NULL) {
            context->parser->priv->error = g_error_new (AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_CHARSET,
                                                        "Unsupported character set %s\n", encoding);
            goto error;
        }
        cstream = g_converter_input_stream_new (G_INPUT_STREAM (context->datastream), converter);
        g_object_unref (converter);
        g_object_unref (context->datastream);
        context->datastream = g_data_input_stream_new (cstream);
        g_object_unref (cstream);
        g_free (encoding);
    }

    context->state = PARSER_STATE_PROLOG;

 error:
    return;
}

static void
context_parse_data (ParserContext *context, char *line)
{
    /* The parsing functions make these assumptions about the data that gets passed in:
       1) It always has complete UTF-8 characters.
       2) It always terminates somewhere where a space is permissable, e.g. never in
          the middle of an element name.
       3) Mutli-character newline sequences are not split into separate chunks of data.
     */
    char *c = line;
    while (c[0] != '\0') {
        switch (context->state) {
        case PARSER_STATE_START:
        case PARSER_STATE_PROLOG:
        case PARSER_STATE_EPILOG:
        case PARSER_STATE_TEXT:
            if (context->state != PARSER_STATE_TEXT) {
                EAT_SPACES (c, line, -1, context);
            }
            if (c[0] == '<') {
                if (context->state == PARSER_STATE_TEXT && context->cur_text != NULL) {
                    g_free (context->parser->priv->event_content);
                    context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
                    context->cur_text = NULL;
                    context->parser->priv->event_type = AXING_STREAM_EVENT_CONTENT;

                    axing_stream_emit_event (AXING_STREAM (context->parser));
                    parser_clean_event_data (context->parser);
                }
                switch (c[1]) {
                case '!':
                    switch (c[2]) {
                    case '[':
                        context_parse_cdata (context, &c);
                        break;
                    case 'D':
                        context_parse_doctype (context, &c);
                        break;
                    default:
                        context_parse_comment (context, &c);
                    }
                    break;
                case '?':
                    context_parse_instruction (context, &c);
                    break;
                case '/':
                    context_parse_end_element (context, &c);
                    break;
                default:
                    if (context->state == PARSER_STATE_EPILOG)
                        ERROR_EXTRACONTENT(context);
                    context_parse_start_element (context, &c);
                }
            }
            else if (context->state == PARSER_STATE_EPILOG && c[0] != '\0') {
                ERROR_EXTRACONTENT(context);
            }
            else if (context->state == PARSER_STATE_TEXT) {
                context_parse_text (context, &c);
            }
            else if (c[0] != '\0') {
                ERROR_SYNTAX(context);
            }
            break;
        case PARSER_STATE_STELM_BASE:
        case PARSER_STATE_STELM_ATTNAME:
        case PARSER_STATE_STELM_ATTEQ:
        case PARSER_STATE_STELM_ATTVAL:
            context_parse_attrs (context, &c);
            break;
        case PARSER_STATE_ENDELM:
            context_parse_end_element (context, &c);
            break;
        case PARSER_STATE_CDATA:
            context_parse_cdata (context, &c);
            break;
        case PARSER_STATE_COMMENT:
            context_parse_comment (context, &c);
            break;
        case PARSER_STATE_INSTRUCTION:
            context_parse_instruction (context, &c);
            break;
        case PARSER_STATE_DOCTYPE:
            context_parse_doctype (context, &c);
            break;
        default:
            g_assert_not_reached ();
        }
        if (context->parser->priv->error != NULL) {
            return;
        }
    }
 error:
    return;
}

static void
context_parse_doctype (ParserContext  *context,
                       char          **line)
{
    switch (context->state) {
    case PARSER_STATE_START:
    case PARSER_STATE_PROLOG:
        if (context->parser->priv->doctype != NULL) {
            ERROR_SYNTAX(context);
        }
        if (!g_str_has_prefix (*line, "<!DOCTYPE")) {
            ERROR_SYNTAX(context);
        }
        (*line) += 9; context->colnum += 9;
        EAT_SPACES (*line, *line, -1, context);
        context->state = PARSER_STATE_DOCTYPE;
        context->doctype_state = DOCTYPE_STATE_START;
        if ((*line)[0] == '\0')
            return;
        break;
    case PARSER_STATE_DOCTYPE:
        break;
    default:
        ERROR_SYNTAX(context);
    }

    if (context->doctype_state == DOCTYPE_STATE_START) {
        char *doctype;
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, doctype, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->parser->priv->doctype = axing_dtd_schema_new ();
        axing_dtd_schema_set_doctype (context->parser->priv->doctype, doctype);
        g_free (doctype);
        context->doctype_state = DOCTYPE_STATE_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_NAME) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '>') {
            context->doctype_state = DOCTYPE_STATE_NULL;
            context->state = PARSER_STATE_PROLOG;
            (*line)++; context->colnum++;
            return;
        }
        else if ((*line)[0] == '[') {
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT;
        }
        else if (g_str_has_prefix (*line, "SYSTEM")) {
            (*line) += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_SYSTEM;
        }
        else if (g_str_has_prefix (*line, "PUBLIC")) {
            (*line) += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_PUBLIC;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_PUBLIC) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_PUBLIC_VAL;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_PUBLIC_VAL) {
        while ((*line)[0] != '\0') {
            char c = (*line)[0];
            if (c == context->quotechar) {
                char *public = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                axing_dtd_schema_set_public_id (context->parser->priv->doctype, public);
                g_free (public);
                context->doctype_state = DOCTYPE_STATE_SYSTEM;
                (*line)++; context->colnum++;
                if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
                    ERROR_SYNTAX(context);
                break;
            }
            if (!(c == 0x20 || c == 0x0D || c == 0x0A ||
                  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '!' || c == '#' || c == '$' || c == '%' || (c >= '\'' && c <= '/') ||
                  c == ':' || c == ';' || c == '=' || c == '?' || c == '@' || c == '_')) {
                ERROR_SYNTAX(context);
            }
            APPEND_CHAR (line, context, FALSE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_SYSTEM) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_SYSTEM_VAL;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_SYSTEM_VAL) {
        while ((*line)[0] != '\0') {
            if ((*line)[0] == context->quotechar) {
                char *system = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                axing_dtd_schema_set_system_id (context->parser->priv->doctype, system);
                g_free (system);
                context->doctype_state = DOCTYPE_STATE_EXTID;
                (*line)++; context->colnum++;
                break;
            }
            APPEND_CHAR (line, context, TRUE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_EXTID) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        switch ((*line)[0]) {
        case '\0':
            return;
        case '[':
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT;
            break;
        case '>':
            context->doctype_state = DOCTYPE_STATE_NULL;
            context->state = PARSER_STATE_PROLOG;
            (*line)++; context->colnum++;
            return;
        default:
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0') {
            return;
        }
        else if ((*line)[0] == ']') {
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_AFTER_INT;
        }
        else if ((*line)[0] == '%') {
            context_parse_parameter (context, line);
            if (context->parser->priv->error)
                goto error;
        }
        else if (g_str_has_prefix (*line, "<!ELEMENT")) {
            context_parse_doctype_element (context, line);
        }
        else if (g_str_has_prefix (*line, "<!ATTLIST")) {
            context_parse_doctype_attlist (context, line);
        }
        else if (g_str_has_prefix (*line, "<!ENTITY")) {
            context_parse_doctype_entity (context, line);
        }
        else if (g_str_has_prefix (*line, "<!NOTATION")) {
            context_parse_doctype_notation (context, line);
        }
        else if (g_str_has_prefix (*line, "<!--")) {
            context_parse_comment (context, line);
        }
        else if (g_str_has_prefix (*line, "<?")) {
            context_parse_instruction (context, line);
        }
        else {
            ERROR_SYNTAX(context);
        }
        return;
    }

    switch (context->doctype_state) {
    case DOCTYPE_STATE_INT_ELEMENT_START:
    case DOCTYPE_STATE_INT_ELEMENT_NAME:
    case DOCTYPE_STATE_INT_ELEMENT_VALUE:
    case DOCTYPE_STATE_INT_ELEMENT_AFTER:
        context_parse_doctype_element (context, line);
        break;
    case DOCTYPE_STATE_INT_ATTLIST_START:
    case DOCTYPE_STATE_INT_ATTLIST_NAME:
    case DOCTYPE_STATE_INT_ATTLIST_VALUE:
    case DOCTYPE_STATE_INT_ATTLIST_QUOTE:
    case DOCTYPE_STATE_INT_ATTLIST_AFTER:
        context_parse_doctype_attlist (context, line);
        break;
    case DOCTYPE_STATE_INT_NOTATION_START:
    case DOCTYPE_STATE_INT_NOTATION_NAME:
    case DOCTYPE_STATE_INT_NOTATION_SYSTEM:
    case DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL:
    case DOCTYPE_STATE_INT_NOTATION_PUBLIC:
    case DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL:
    case DOCTYPE_STATE_INT_NOTATION_PUBLIC_AFTER:
    case DOCTYPE_STATE_INT_NOTATION_AFTER:
        context_parse_doctype_notation (context, line);
        break;
    case DOCTYPE_STATE_INT_ENTITY_START:
    case DOCTYPE_STATE_INT_ENTITY_NAME:
    case DOCTYPE_STATE_INT_ENTITY_VALUE:
    case DOCTYPE_STATE_INT_ENTITY_PUBLIC:
    case DOCTYPE_STATE_INT_ENTITY_PUBLIC_VAL:
    case DOCTYPE_STATE_INT_ENTITY_SYSTEM:
    case DOCTYPE_STATE_INT_ENTITY_SYSTEM_VAL:
    case DOCTYPE_STATE_INT_ENTITY_NDATA:
    case DOCTYPE_STATE_INT_ENTITY_AFTER:
        context_parse_doctype_entity (context, line);
        break;
    default:
        break;
    }

    if (context->doctype_state == DOCTYPE_STATE_AFTER_INT) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] != '>')
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_NULL;
        context->state = PARSER_STATE_PROLOG;
        (*line)++; context->colnum++;
        return;
    }

 error:
    return;
}

static void
context_parse_doctype_element (ParserContext *context, char **line) {
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (g_str_has_prefix(*line, "<!ELEMENT"));
        (*line) += 9; context->colnum += 9;
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_START;
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_START) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->cur_qname, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_NAME) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if (g_str_has_prefix (*line, "EMPTY")) {
            context->cur_text = g_string_new ("EMPTY");
            (*line) += 5; context->colnum += 5;
            context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_AFTER;
        }
        else if (g_str_has_prefix (*line, "ANY")) {
            context->cur_text = g_string_new ("ANY");
            (*line) += 3; context->colnum += 3;
            context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_AFTER;
        }
        else if ((*line)[0] == '(') {
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            g_string_append_c (context->cur_text, '(');
            context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_VALUE;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_VALUE) {
        /* not checking the internal syntax here; valid chars only */
        while ((*line)[0] != '\0' && (*line)[0] != '>') {
            gunichar cp = g_utf8_get_char (*line);
            if (!(XML_IS_NAME_CHAR (cp) || XML_IS_SPACE (*line, context) ||
                  (*line)[0] == '(' || (*line)[0] == ')' || (*line)[0] == '|' ||
                  (*line)[0] == ',' || (*line)[0] == '+' || (*line)[0] == '*' ||
                  (*line)[0] == '?' || (*line)[0] == '#' )) {
                ERROR_SYNTAX(context);
            }
            APPEND_CHAR (line, context, FALSE);
        }
        if ((*line)[0] == '>')
            context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_AFTER;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_AFTER) {
        char *value;
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] != '>')
            ERROR_SYNTAX(context);
        value = g_string_free (context->cur_text, FALSE);
        context->cur_text = NULL;
        axing_dtd_schema_add_element (context->parser->priv->doctype,
                                      context->cur_qname,
                                      value,
                                      &(context->parser->priv->error));
        g_free (value);
        g_free (context->cur_qname);
        context->cur_qname = NULL;
        (*line)++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }
 error:
    return;
}

static void
context_parse_doctype_attlist (ParserContext *context, char **line) {
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (g_str_has_prefix(*line, "<!ATTLIST"));
        (*line) += 9; context->colnum += 9;
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_START;
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_START) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->cur_qname, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_NAME) {
        gunichar cp;
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        cp = g_utf8_get_char (*line);
        if (XML_IS_NAME_START_CHAR (cp)) {
            context->cur_text = g_string_new (NULL);
            context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_VALUE;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_VALUE) {
        /* not checking the internal syntax here; valid chars only */
        while ((*line)[0] != '\0' && (*line)[0] != '>') {
            gunichar cp;
            if ((*line)[0] == '"') {
                g_string_append_c (context->cur_text, '"');
                (*line)++; context->colnum++;
                context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_QUOTE;
                break;
            }
            cp = g_utf8_get_char (*line);
            if (!(XML_IS_NAME_CHAR (cp) || XML_IS_SPACE (*line, context) ||
                  (*line)[0] == '#' || (*line)[0] == '|' ||
                  (*line)[0] == '(' || (*line)[0] == ')' )) {
                ERROR_SYNTAX(context);
            }
            APPEND_CHAR (line, context, FALSE);
        }
        if ((*line)[0] == '>')
            context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_AFTER;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_QUOTE) {
        while ((*line)[0] != '\0') {
            if ((*line)[0] == '"') {
                g_string_append_c (context->cur_text, '"');
                (*line)++; context->colnum++;
                context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_VALUE;
                break;
            }
            APPEND_CHAR (line, context, TRUE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_AFTER) {
        char *value;
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] != '>')
            ERROR_SYNTAX(context);
        value = g_string_free (context->cur_text, FALSE);
        context->cur_text = NULL;
        axing_dtd_schema_add_attlist (context->parser->priv->doctype,
                                      context->cur_qname,
                                      value,
                                      &(context->parser->priv->error));
        g_free (value);
        g_free (context->cur_qname);
        context->cur_qname = NULL;
        (*line)++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }

 error:
    return;
}

static void
context_parse_doctype_notation (ParserContext *context, char **line) {
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (g_str_has_prefix(*line, "<!NOTATION"));
        (*line) += 10; context->colnum += 10;
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_NOTATION_START;
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_START) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->cur_qname, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_NOTATION_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_NAME) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if (g_str_has_prefix (*line, "SYSTEM")) {
            (*line) += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_SYSTEM;
        }
        else if (g_str_has_prefix (*line, "PUBLIC")) {
            (*line) += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_PUBLIC;
        }
        else {
            ERROR_SYNTAX(context);
        }
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_SYSTEM) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '"' || (*line)[0] == '\'') {
            context->quotechar = (*line)[0];
            (*line)++; context->colnum++;
            context->cur_text = g_string_new (NULL);
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL) {
        while ((*line)[0] != '\0') {
            if ((*line)[0] == context->quotechar) {
                context->decl_system = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->doctype_state = DOCTYPE_STATE_INT_NOTATION_AFTER;
                (*line)++; context->colnum++;
                break;
            }
            APPEND_CHAR (line, context, TRUE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_PUBLIC) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL) {
        while ((*line)[0] != '\0') {
            char c = (*line)[0];
            if (c == context->quotechar) {
                context->decl_public = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->doctype_state = DOCTYPE_STATE_INT_NOTATION_PUBLIC_AFTER;
                (*line)++; context->colnum++;
                if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0' || (*line)[0] == '>'))
                    ERROR_SYNTAX(context);
                break;
            }
            if (!(c == 0x20 || c == 0x0D || c == 0x0A ||
                  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '!' || c == '#' || c == '$' || c == '%' || (c >= '\'' && c <= '/') ||
                  c == ':' || c == ';' || c == '=' || c == '?' || c == '@' || c == '_')) {
                ERROR_SYNTAX(context);
            }
            APPEND_CHAR (line, context, FALSE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_PUBLIC_AFTER) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL;
        }
        else if ((*line)[0] == '>') {
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_AFTER;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_AFTER) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] != '>')
            ERROR_SYNTAX(context);
        axing_dtd_schema_add_notation (context->parser->priv->doctype,
                                       context->cur_qname,
                                       context->decl_public,
                                       context->decl_system);
        g_free (context->cur_qname);
        context->cur_qname = NULL;
        if (context->decl_system) {
            g_free (context->decl_system);
            context->decl_system = NULL;
        }
        if (context->decl_public) {
            g_free (context->decl_public);
            context->decl_public = NULL;
        }
        (*line)++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }

 error:
    return;
}

static void
context_parse_doctype_entity (ParserContext *context, char **line) {
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (g_str_has_prefix(*line, "<!ENTITY"));
        (*line) += 8; context->colnum += 8;
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ENTITY_START;
        context->decl_pedef = FALSE;
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_START) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '%') {
            /* if already true, we've seen a % in this decl */
            if (context->decl_pedef)
                ERROR_SYNTAX(context);
            context->decl_pedef = TRUE;
            (*line)++; context->colnum++;
            if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
                ERROR_SYNTAX(context);
            EAT_SPACES (*line, *line, -1, context);
            if ((*line)[0] == '\0')
                return;
        }
        XML_GET_NAME(line, context->cur_qname, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ENTITY_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_NAME) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if (g_str_has_prefix (*line, "SYSTEM")) {
            (*line) += 6; context->colnum += 6;
            if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
                ERROR_SYNTAX(context);
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_SYSTEM;
            EAT_SPACES (*line, *line, -1, context);
        }
        else if (g_str_has_prefix (*line, "PUBLIC")) {
            (*line) += 6; context->colnum += 6;
            if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
                ERROR_SYNTAX(context);
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_PUBLIC;
            EAT_SPACES (*line, *line, -1, context);
        }
        else if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_VALUE;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_VALUE) {
        while ((*line)[0] != '\0') {
            if ((*line)[0] == context->quotechar) {
                /* let the AFTER handler handle cur_text */
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
                (*line)++; context->colnum++;
                break;
            }
            APPEND_CHAR (line, context, TRUE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_PUBLIC) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_PUBLIC_VAL;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_PUBLIC_VAL) {
        while ((*line)[0] != '\0') {
            char c = (*line)[0];
            if (c == context->quotechar) {
                context->decl_public = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_SYSTEM;
                (*line)++; context->colnum++;
                if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
                    ERROR_SYNTAX(context);
                break;
            }
            if (!(c == 0x20 || c == 0x0D || c == 0x0A ||
                  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '!' || c == '#' || c == '$' || c == '%' || (c >= '\'' && c <= '/') ||
                  c == ':' || c == ';' || c == '=' || c == '?' || c == '@' || c == '_')) {
                ERROR_SYNTAX(context);
            }
            APPEND_CHAR (line, context, FALSE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_SYSTEM) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            context->cur_text = g_string_new (NULL);
            (*line)++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_SYSTEM_VAL;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_SYSTEM_VAL) {
        while ((*line)[0] != '\0') {
            if ((*line)[0] == context->quotechar) {
                context->decl_system = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
                (*line)++; context->colnum++;
                break;
            }
            APPEND_CHAR (line, context, TRUE);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_NDATA) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->decl_ndata, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0' || (*line)[0] == '>'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_AFTER) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if (g_str_has_prefix (*line, "NDATA")) {
            if (context->cur_text != NULL    || /* not after an EntityValue */
                context->decl_system == NULL || /* only after PUBLIC or SYSTEM */
                context->decl_pedef == TRUE  || /* no NDATA on param entities */
                context->decl_ndata != NULL  )  /* only one NDATA */
                ERROR_SYNTAX(context);
            (*line) += 5; context->colnum += 5;
            if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0'))
                ERROR_SYNTAX(context);
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_NDATA;
            return;
        }
        if ((*line)[0] != '>')
            ERROR_SYNTAX(context);
        if (context->decl_pedef) {
            if (context->cur_text) {
                char *value = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                axing_dtd_schema_add_parameter (context->parser->priv->doctype,
                                                context->cur_qname,
                                                value);
                g_free (value);
            }
            else {
                axing_dtd_schema_add_external_parameter (context->parser->priv->doctype,
                                                         context->cur_qname,
                                                         context->decl_public,
                                                         context->decl_system);
            }
        }
        else {
            if (context->cur_text) {
                char *value = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                axing_dtd_schema_add_entity (context->parser->priv->doctype,
                                             context->cur_qname,
                                             value);
                g_free (value);
            }
            else if (context->decl_ndata) {
                axing_dtd_schema_add_unparsed_entity (context->parser->priv->doctype,
                                                      context->cur_qname,
                                                      context->decl_public,
                                                      context->decl_system,
                                                      context->decl_ndata);
            }
            else {
                axing_dtd_schema_add_external_entity (context->parser->priv->doctype,
                                                      context->cur_qname,
                                                      context->decl_public,
                                                      context->decl_system);
            }
        }
        g_free (context->cur_qname);
        context->cur_qname = NULL;
        if (context->cur_text) {
            g_string_free (context->cur_text, TRUE);
            context->cur_text = NULL;
        }
        if (context->decl_system) {
            g_free (context->decl_system);
            context->decl_system = NULL;
        }
        if (context->decl_public) {
            g_free (context->decl_public);
            context->decl_public = NULL;
        }
        if (context->decl_ndata) {
            g_free (context->decl_ndata);
            context->decl_ndata = NULL;
        }
        (*line)++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }

 error:
    return;
}

static void
context_parse_parameter (ParserContext  *context,
                         char          **line)
{
    const char *beg = *line + 1;
    char *entname = NULL;
    char *value = NULL;
    int colnum = context->colnum;
    g_assert ((*line)[0] == '%');

    (*line)++; colnum++;

    while ((*line)[0] != '\0') {
        gunichar cp;
        if ((*line)[0] == ';') {
            break;
        }
        /* FIXME */
        cp = g_utf8_get_char (*line);
        if ((*line) == beg) {
            if (!XML_IS_NAME_START_CHAR(cp)) {
                ERROR_ENTITY(context);
            }
        }
        else {
            if (!XML_IS_NAME_CHAR(cp)) {
                ERROR_ENTITY(context);
            }
        }
        *line = g_utf8_next_char (*line);
        colnum += 1;
    }
    if ((*line)[0] != ';') {
        ERROR_ENTITY(context);
    }
    entname = g_strndup (beg, *line - beg);
    (*line)++; colnum++;

    value = axing_dtd_schema_get_parameter (context->parser->priv->doctype, entname);
    if (value) {
        ParserContext *entctxt = context_new (context->parser);
        /* not duping these two, NULL them before free below */
        entctxt->basename = context->basename;
        entctxt->entname = entname;
        entctxt->showname = g_strdup_printf ("%s(%%%s;)", entctxt->basename, entname);
        entctxt->state = context->state;
        entctxt->init_state = context->state;
        entctxt->doctype_state = context->doctype_state;
        entctxt->event_stack_root = entctxt->parser->priv->event_stack->len;
        entctxt->cur_text = context->cur_text;
        context->cur_text = NULL;

        context_parse_data (entctxt, value);
        if (entctxt->parser->priv->error == NULL) {
            if (entctxt->state != context->state
                || entctxt->doctype_state != context->doctype_state)
                ERROR_SYNTAX(entctxt);
        }

        context->state = entctxt->state;
        context->cur_text = entctxt->cur_text;
        entctxt->cur_text = NULL;
        entctxt->basename = NULL;
        entctxt->entname = NULL;
        context_free (entctxt);
        g_free (value);
    }
    else {
        ERROR_FIXME(context);
    }

 error:
    context->colnum = colnum;
    g_free (entname);
}

static void
context_parse_cdata (ParserContext *context, char **line)
{
    if (context->state != PARSER_STATE_CDATA) {
        if (!g_str_has_prefix (*line, "<![CDATA[")) {
            ERROR_SYNTAX(context);
        }
        (*line) += 9; context->colnum += 9;
        context->cur_text = g_string_new (NULL);
        context->state = PARSER_STATE_CDATA;
    }

    while ((*line)[0] != '\0') {
        if ((*line)[0] == ']' && (*line)[1] == ']' && (*line)[2] == '>') {
            (*line) += 3; context->colnum += 3;

            context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
            context->cur_text = NULL;
            context->parser->priv->event_type = AXING_STREAM_EVENT_CDATA;

            axing_stream_emit_event (AXING_STREAM (context->parser));
            parser_clean_event_data (context->parser);
            context->state = PARSER_STATE_TEXT;
            return;
        }
        APPEND_CHAR (line, context, TRUE);
    }

 error:
    return;
}

static void
context_parse_comment (ParserContext *context, char **line)
{
    if (context->state != PARSER_STATE_COMMENT) {
        if (!g_str_has_prefix (*line, "<!--")) {
            ERROR_SYNTAX(context);
        }
        (*line) += 4; context->colnum += 4;
        context->cur_text = g_string_new (NULL);
        context->prev_state = context->state;
        context->state = PARSER_STATE_COMMENT;
    }

    while ((*line)[0] != '\0') {
        if ((*line)[0] == '-' && (*line)[1] == '-') {
            if ((*line)[2] != '>') {
                ERROR_SYNTAX(context);
            }
            (*line) += 3; context->colnum += 3;

            if (context->prev_state == PARSER_STATE_DOCTYPE) {
                /* currently not doing anything with comments in the internal subset */
                g_string_free (context->cur_text, TRUE);
                context->cur_text = NULL;
            }
            else {
                context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->parser->priv->event_type = AXING_STREAM_EVENT_COMMENT;

                axing_stream_emit_event (AXING_STREAM (context->parser));
                parser_clean_event_data (context->parser);
            }
            context->state = context->prev_state;
            return;
        }
        APPEND_CHAR (line, context, TRUE);
    }

 error:
    return;
}

static void
context_parse_instruction (ParserContext *context, char **line)
{
    if (context->state != PARSER_STATE_INSTRUCTION) {
        if (!g_str_has_prefix (*line, "<?")) {
            ERROR_SYNTAX(context);
        }
        (*line) += 2; context->colnum += 2;
        XML_GET_NAME(line, context->parser->priv->event_qname, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0' || (*line)[0] == '?'))
            ERROR_SYNTAX(context);

        context->prev_state = context->state;
        context->state = PARSER_STATE_INSTRUCTION;
    }

    if (context->cur_text == NULL) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        context->cur_text = g_string_new (NULL);
    }

    while ((*line)[0] != '\0') {
        if ((*line)[0] == '?' && (*line)[1] == '>') {
            (*line) += 2; context->colnum += 2;

            if (context->prev_state == PARSER_STATE_DOCTYPE) {
                /* currently not doing anything with PIs in the internal subset */
                g_string_free (context->cur_text, TRUE);
                context->cur_text = NULL;
            }
            else {
                context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->parser->priv->event_type = AXING_STREAM_EVENT_INSTRUCTION;

                axing_stream_emit_event (AXING_STREAM (context->parser));
                parser_clean_event_data (context->parser);
            }
            context->state = context->prev_state;
            return;
        }
        APPEND_CHAR (line, context, TRUE);
    }

 error:
    return;
}

static void
context_parse_end_element (ParserContext *context, char **line)
{
    ParserStackFrame frame;
    const char *colon;
    if (context->state != PARSER_STATE_ENDELM) {
        g_assert ((*line)[0] == '<' && (*line)[1] == '/');
        context->node_linenum = context->linenum;
        context->node_colnum = context->colnum;
        (*line) += 2; context->colnum += 2;
        XML_GET_NAME(line, context->parser->priv->event_qname, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0' || (*line)[0] == '>'))
            ERROR_SYNTAX(context);
    }
    EAT_SPACES (*line, *line, -1, context);
    if ((*line)[0] == '\0') {
        context->state = PARSER_STATE_ENDELM;
        return;
    }
    if ((*line)[0] != '>') {
        ERROR_SYNTAX(context);
    }
    (*line)++; context->colnum++;

    colon = strchr (context->parser->priv->event_qname, ':');
    if (colon != NULL) {
        gunichar cp;
        const char *localname;
        const char *namespace;
        if (colon == context->parser->priv->event_qname) {
            ERROR_NS_QNAME(context);
        }
        localname = colon + 1;
        if (localname[0] == '\0' || strchr (localname, ':')) {
            ERROR_NS_QNAME(context);
        }
        cp = g_utf8_get_char (localname);
        if (!XML_IS_NAME_START_CHAR(cp)) {
            ERROR_NS_QNAME(context);
        }
        context->parser->priv->event_prefix = g_strndup (context->parser->priv->event_qname,
                                                         colon - context->parser->priv->event_qname);
        context->parser->priv->event_localname = g_strdup (localname);
        namespace = namespace_map_get_namespace (AXING_NAMESPACE_MAP (context->parser),
                                                 context->parser->priv->event_prefix);
        if (namespace == NULL) {
            ERROR_NS_NOTFOUND(context);
        }
        context->parser->priv->event_namespace = g_strdup (namespace);
    }
    else {
        const char *namespace = namespace_map_get_namespace (AXING_NAMESPACE_MAP (context->parser), "");
        if (namespace != NULL)
            context->parser->priv->event_namespace = g_strdup (namespace);
    }

    if (context->parser->priv->event_stack->len <= context->event_stack_root) {
        context->linenum = context->node_linenum;
        context->colnum = context->node_colnum;
        ERROR_EXTRACONTENT(context);
    }
    frame = g_array_index (context->parser->priv->event_stack,
                           ParserStackFrame,
                           context->parser->priv->event_stack->len - 1);
    g_array_remove_index (context->parser->priv->event_stack,
                          context->parser->priv->event_stack->len - 1);
    if (!g_str_equal (frame.qname, context->parser->priv->event_qname)) {
        ERROR_WRONGEND(context);
    }
    if (frame.nshash) {
        g_hash_table_destroy (frame.nshash);
    }
    g_free (frame.qname);

    context->parser->priv->event_type = AXING_STREAM_EVENT_END_ELEMENT;
    axing_stream_emit_event (AXING_STREAM (context->parser));

    if (context->parser->priv->event_stack->len == context->event_stack_root)
        context->state = context->init_state;
    else
        context->state = PARSER_STATE_TEXT;

    parser_clean_event_data (context->parser);

 error:
    return;
}

static void
context_parse_start_element (ParserContext *context, char **line)
{
    g_assert ((*line)[0] == '<');
    context->node_linenum = context->linenum;
    context->node_colnum = context->colnum;
    (*line)++; context->colnum++;

    g_free (context->cur_qname);
    context->cur_qname = NULL;
    XML_GET_NAME(line, context->cur_qname, context);

    if ((*line)[0] == '>') {
        (*line)++; context->colnum++;
        context_trigger_start_element (context);
    }
    else if ((*line)[0] == '/' && (*line)[1] == '>') {
        context->empty = TRUE;
        (*line) += 2; context->colnum += 2;
        context_trigger_start_element (context);
    }
    else if (XML_IS_SPACE(*line, context) || (*line)[0] == '\0') {
        context->state = PARSER_STATE_STELM_BASE;
    }
    else {
        ERROR_SYNTAX(context);
    }

 error:
    return;
}

static void
context_parse_attrs (ParserContext *context, char **line)
{
    if (context->state == PARSER_STATE_STELM_BASE) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '>') {
            (*line)++; context->colnum++;
            context_trigger_start_element (context);
            return;
        }
        else if ((*line)[0] == '/' && (*line)[1] == '>') {
            context->empty = TRUE;
            (*line) += 2; context->colnum += 2;
            context_trigger_start_element (context);
            return;
        }
        if ((*line)[0] == '\0') {
            return;
        }
        context->attr_linenum = context->linenum;
        context->attr_colnum = context->colnum;

        XML_GET_NAME(line, context->cur_attrname, context);
        if (!(XML_IS_SPACE(*line, context) || (*line)[0] == '\0' || (*line)[0] == '='))
            ERROR_SYNTAX(context);
        context->state = PARSER_STATE_STELM_ATTNAME;
    }
    if (context->state == PARSER_STATE_STELM_ATTNAME) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '=') {
            (*line)++; context->colnum++;
            context->state = PARSER_STATE_STELM_ATTEQ;
        }
        else if ((*line)[0] == '\0') {
            return;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }
    if (context->state == PARSER_STATE_STELM_ATTEQ) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\'' || (*line)[0] == '"') {
            context->quotechar = (*line)[0];
            (*line)++; context->colnum++;
            context->cur_text = g_string_new (NULL);
            context->state = PARSER_STATE_STELM_ATTVAL;
        }
        else if ((*line)[0] == '\0') {
            return;
        }
        else {
            ERROR_SYNTAX(context);
        }
    }
    if (context->state == PARSER_STATE_STELM_ATTVAL) {
        while ((*line)[0] != '\0') {
            if ((*line)[0] == context->quotechar) {
                char *xmlns = NULL;
                char *attrval;
                attrval = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;

                if (g_str_has_prefix (context->cur_attrname, "xmlns:")) {
                    /* FIXME: if cur_attrname == "xmlns:"? */
                    xmlns = context->cur_attrname + 6;
                }
                else if (g_str_equal (context->cur_attrname, "xmlns")) {
                    xmlns = "";
                }

                if (xmlns != NULL) {
                    if (g_str_equal (xmlns, "xml")) {
                        if (!g_str_equal(attrval, NS_XML)) {
                            ERROR_NS_INVALID(context, xmlns);
                        }
                    }
                    else {
                        if (g_str_equal(attrval, NS_XML)) {
                            ERROR_NS_INVALID(context, xmlns);
                        }
                    }
                    if (g_str_equal (xmlns, "xmlns")) {
                        ERROR_NS_INVALID(context, xmlns);
                    }
                    if (context->cur_nshash == NULL) {
                        context->cur_nshash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                     g_free, g_free);
                    }
                    if (g_hash_table_lookup (context->cur_nshash, xmlns) != NULL) {
                        ERROR_DUPATTR(context);
                    }
                    /* FIXME: ensure namespace is valid URI(1.0) or IRI(1.1) */
                    g_hash_table_insert (context->cur_nshash,
                                         g_strdup (xmlns),
                                         attrval);
                    g_free (context->cur_attrname);
                    context->cur_attrname = NULL;
                }
                else {
                    AttributeData *data;
                    if (context->cur_attrs == NULL) {
                        context->cur_attrs = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                                                    (GDestroyNotify) attribute_data_free);
                    }
                    if (g_hash_table_lookup (context->cur_attrs, context->cur_attrname) != NULL) {
                        ERROR_DUPATTR(context);
                    }
                    data = g_new0 (AttributeData, 1);
                    data->qname = context->cur_attrname;
                    data->value = attrval;
                    data->linenum = context->attr_linenum;
                    data->colnum = context->attr_colnum;
                    g_hash_table_insert (context->cur_attrs,
                                         context->cur_attrname,
                                         data);
                    context->cur_attrname = NULL;
                }

                context->state = PARSER_STATE_STELM_BASE;
                (*line)++; context->colnum++;
                if (!((*line)[0] == '>' || (*line)[0] == '/' ||
                      (*line)[0] == '\0' || XML_IS_SPACE (*line, context))) {
                    ERROR_SYNTAX(context);
                }
                return;
            }
            else if ((*line)[0] == '&') {
                context_parse_entity (context, line);
                if (context->parser->priv->error)
                    goto error;
                continue;
            }
            else if ((*line)[0] == '<') {
                ERROR_SYNTAX(context);
            }
            APPEND_CHAR (line, context, TRUE);
        }
    }
 error:
    return;
}

static void
context_parse_entity (ParserContext *context, char **line)
{
    const char *beg = *line + 1;
    char *entname = NULL;
    char builtin = '\0';
    int colnum = context->colnum;
    g_assert ((*line)[0] == '&');

    (*line)++; colnum++;

    if ((*line)[0] == '#') {
        gunichar cp = 0;
        (*line)++; colnum++;
        if ((*line)[0] == 'x') {
            (*line)++; colnum++;
            while ((*line)[0] != '\0') {
                if ((*line)[0] == ';')
                    break;
                else if ((*line)[0] >= '0' && (*line)[0] <= '9')
                    cp = 16 * cp + ((*line)[0] - '0');
                else if ((*line)[0] >= 'A' && (*line)[0] <= 'F')
                    cp = 16 * cp + 10 + ((*line)[0] - 'A');
                else if ((*line)[0] >= 'a' && (*line)[0] <= 'f')
                    cp = 16 * cp + 10 + ((*line)[0] - 'a');
                else
                    ERROR_ENTITY(context);
                (*line)++; colnum++;
            }
            if ((*line)[0] != ';')
                ERROR_ENTITY(context);
            (*line)++; colnum++;
        }
        else {
            while ((*line)[0] != '\0') {
                if ((*line)[0] == ';')
                    break;
                else if ((*line)[0] >= '0' && (*line)[0] <= '9')
                    cp = 10 * cp + ((*line)[0] - '0');
                else
                    ERROR_ENTITY(context);
                (*line)++; colnum++;
            }
            if ((*line)[0] != ';')
                ERROR_ENTITY(context);
            (*line)++; colnum++;
        }
        if (XML_IS_CHAR(cp, context) || XML_IS_CHAR_RESTRICTED(cp, context)) {
            if (context->cur_text == NULL)
                context->cur_text = g_string_new (NULL);
            g_string_append_unichar (context->cur_text, cp);
        }
        else {
            ERROR_ENTITY(context);
        }
    }
    else {
        while ((*line)[0] != '\0') {
            gunichar cp;
            if ((*line)[0] == ';') {
                break;
            }
            cp = g_utf8_get_char (*line);
            if ((*line) == beg) {
                if (!XML_IS_NAME_START_CHAR(cp)) {
                    ERROR_ENTITY(context);
                }
            }
            else {
                if (!XML_IS_NAME_CHAR(cp)) {
                    ERROR_ENTITY(context);
                }
            }
            *line = g_utf8_next_char (*line);
            colnum += 1;
        }
        if ((*line)[0] != ';') {
            ERROR_ENTITY(context);
        }
        entname = g_strndup (beg, *line - beg);
        (*line)++; colnum++;

        if (g_str_equal (entname, "lt"))
            builtin = '<';
        else if (g_str_equal (entname, "gt"))
            builtin = '>';
        else if (g_str_equal (entname, "amp"))
            builtin = '&';
        else if (g_str_equal (entname, "apos"))
            builtin = '\'';
        else if (g_str_equal (entname, "quot"))
            builtin = '"';

        if (builtin != '\0') {
            if (context->cur_text == NULL) 
                context->cur_text = g_string_new (NULL);
            g_string_append_c (context->cur_text, builtin);
        }
        else {
            context_process_entity (context, entname, line);
        }
    }
 error:
    context->colnum = colnum;
    g_free (entname);
}

static void
context_process_entity (ParserContext *context, const char *entname, char **line)
{
    ParserContext *parent;
    char *value=NULL, *system=NULL, *public=NULL, *ndata=NULL;

    for (parent = context->parent; parent != NULL; parent = parent->parent) {
        if (parent->entname && g_str_equal (entname, parent->entname)) {
            ERROR_ENTITY(context);
        }
    }

    if (axing_dtd_schema_get_entity_full (context->parser->priv->doctype, entname,
                                          &value, &public, &system, &ndata)) {
        if (value) {
            ParserContext *entctxt = context_new (context->parser);
            /* not duping these two, NULL them before free below */
            entctxt->parent = context;
            entctxt->basename = context->basename;
            entctxt->entname = (char *) entname;
            entctxt->showname = g_strdup_printf ("%s(&%s;)", entctxt->basename, entname);
            entctxt->state = context->state;
            entctxt->init_state = context->state;
            entctxt->event_stack_root = entctxt->parser->priv->event_stack->len;
            entctxt->cur_text = context->cur_text;
            context->cur_text = NULL;

            context_parse_data (entctxt, value);
            if (entctxt->parser->priv->error == NULL)
                context_check_end (entctxt);

            context->state = entctxt->state;
            context->cur_text = entctxt->cur_text;
            entctxt->cur_text = NULL;
            entctxt->basename = NULL;
            entctxt->entname = NULL;
            context_free (entctxt);
        }
        else if (ndata) {
            ERROR_FIXME(context);
        }
        else {
            AxingResolver *resolver;

            if (context->state == PARSER_STATE_STELM_ATTVAL) {
                ERROR_ENTITY(context);
            }

            if (context->parser->priv->resolver)
                resolver = g_object_ref (context->parser->priv->resolver);
            else
                resolver = axing_resolver_get_default ();

            if (context->parser->priv->async) {
                ParserContext *entctxt;
                entctxt = context_new (context->parser);
                entctxt->parent = context;
                entctxt->entname = g_strdup (entname);
                entctxt->state = context->state;
                entctxt->init_state = context->state;
                entctxt->event_stack_root = entctxt->parser->priv->event_stack->len;
                entctxt->cur_text = context->cur_text;
                context->cur_text = NULL;
                axing_resolver_resolve_async (resolver, context->resource,
                                              NULL, system, public, "xml:entity",
                                              context->parser->priv->cancellable,
                                              (GAsyncReadyCallback) context_process_entity_resolved,
                                              entctxt);
                context->pause_line = g_strdup (*line);
                while ((*line)[0])
                    (*line)++;
            }
            else {
                ParserContext *entctxt;
                AxingResource *resource;
                GFile *file;
                resource = axing_resolver_resolve (resolver, context->resource,
                                                   NULL, system, public, "xml:entity",
                                                   context->parser->priv->cancellable,
                                                   &(context->parser->priv->error));
                if (context->parser->priv->error) {
                    g_object_unref (resolver);
                    goto error;
                }

                entctxt = context_new (context->parser);
                entctxt->parent = context;
                entctxt->resource = resource;
                entctxt->basename = resource_get_basename (resource);
                entctxt->entname = g_strdup (entname);
                entctxt->state = context->state;
                entctxt->init_state = context->state;
                entctxt->event_stack_root = entctxt->parser->priv->event_stack->len;
                entctxt->cur_text = context->cur_text;
                context->cur_text = NULL;

                context_parse_sync (entctxt);

                context->state = entctxt->state;
                context->cur_text = entctxt->cur_text;
                entctxt->cur_text = NULL;
                context_free (entctxt);
                g_object_unref (resolver);
            }
        }
    }
 error:
    g_free (value);
    g_free (public);
    g_free (system);
    g_free (ndata);
}

static void
context_process_entity_resolved (AxingResolver *resolver,
                                 GAsyncResult  *result,
                                 ParserContext *context)
{
    AxingResource *resource;

    resource = axing_resolver_resolve_finish (resolver, result,
                                              &(context->parser->priv->error));
    if (context->parser->priv->error)
        goto error;

    context->resource = resource;
    context->basename = resource_get_basename (resource);

    axing_resource_read_async (context->resource,
                               context->parser->priv->cancellable,
                               (GAsyncReadyCallback) context_resource_read_cb,
                               context);
 error:
    g_object_unref (resolver);
}

static void
context_process_entity_finish (ParserContext *context)
{
    ParserContext *parent;
    parent = context->parent;
    context->parent = NULL;

    parent->state = context->state;
    parent->cur_text = context->cur_text;
    context->cur_text = NULL;
    context_free (context);

    context_read_data_cb (NULL, NULL, parent);
}


static void
context_parse_text (ParserContext *context, char **line)
{
    while ((*line)[0] != '\0') {
        if ((*line)[0] == '<') {
            if (context->cur_text) {
                g_free (context->parser->priv->event_content);
                context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->parser->priv->event_type = AXING_STREAM_EVENT_CONTENT;

                axing_stream_emit_event (AXING_STREAM (context->parser));
                parser_clean_event_data (context->parser);
            }
            return;
        }
        if ((*line)[0] == '&') {
            context_parse_entity (context, line);
            if (context->parser->priv->error)
                goto error;
            continue;
        }
        if (context->cur_text == NULL) {
            context->cur_text = g_string_new (NULL);
        }
        APPEND_CHAR (line, context, TRUE);
    }
 error:
    return;
}

static void
context_trigger_start_element (ParserContext *context)
{
    ParserStackFrame frame;
    const char *colon;

    g_free (context->parser->priv->event_qname);
    context->parser->priv->event_qname = context->cur_qname;
    context->cur_qname = NULL;

    frame.qname = g_strdup (context->parser->priv->event_qname);
    frame.nshash = context->cur_nshash;
    context->cur_nshash = NULL;
    g_array_append_val (context->parser->priv->event_stack, frame);

    colon = strchr (context->parser->priv->event_qname, ':');
    if (colon != NULL) {
        gunichar cp;
        const char *localname;
        const char *namespace;
        if (colon == context->parser->priv->event_qname) {
            ERROR_NS_QNAME(context);
        }
        localname = colon + 1;
        if (localname[0] == '\0' || strchr (localname, ':')) {
            ERROR_NS_QNAME(context);
        }
        cp = g_utf8_get_char (localname);
        if (!XML_IS_NAME_START_CHAR(cp)) {
            ERROR_NS_QNAME(context);
        }
        context->parser->priv->event_prefix = g_strndup (context->parser->priv->event_qname,
                                                         colon - context->parser->priv->event_qname);
        context->parser->priv->event_localname = g_strdup (localname);
        namespace = namespace_map_get_namespace (AXING_NAMESPACE_MAP (context->parser),
                                                 context->parser->priv->event_prefix);
        if (namespace == NULL) {
            ERROR_NS_NOTFOUND(context);
        }
        context->parser->priv->event_namespace = g_strdup (namespace);
    }
    else {
        const char *namespace = namespace_map_get_namespace (AXING_NAMESPACE_MAP (context->parser), "");
        if (namespace != NULL)
            context->parser->priv->event_namespace = g_strdup (namespace);
    }

    if (context->cur_attrs) {
        GHashTableIter attrs;
        gpointer key, val;
        guint num = g_hash_table_size (context->cur_attrs);
        guint cur;

        context->parser->priv->event_attrkeys = g_new0 (char *, num + 1);
        context->parser->priv->event_attrvals = g_new0 (AttributeData *, num + 1);

        cur = 0;
        /* We drop from the hash at each iteration, stealing the key and
           value. So we re-init the iterator each time. */
        while (g_hash_table_iter_init (&attrs, context->cur_attrs),
               g_hash_table_iter_next (&attrs, &key, &val)) {
            char *qname = (char *) key;
            AttributeData *data = (AttributeData *) val;
            const char *colon;

            /* FIXME: check for duplicate expanded name */

            colon = strchr (qname, ':');
            if (colon != NULL) {
                gunichar cp;
                const char *localname;
                const char *namespace;
                int pre;
                if (colon == qname) {
                    ERROR_NS_QNAME_ATTR(context, data);
                }
                localname = colon + 1;
                if (localname[0] == '\0' || strchr (localname, ':')) {
                    ERROR_NS_QNAME_ATTR(context, data);
                }
                cp = g_utf8_get_char (localname);
                if (!XML_IS_NAME_START_CHAR(cp)) {
                    ERROR_NS_QNAME_ATTR(context, data);
                }
                data->prefix = g_strndup (qname, colon - qname);
                data->localname = g_strdup (localname);
                namespace = namespace_map_get_namespace (AXING_NAMESPACE_MAP (context->parser),
                                                         data->prefix);
                if (namespace == NULL) {
                    ERROR_NS_NOTFOUND_ATTR(context, data);
                }
                data->namespace = g_strdup (namespace);
                for (pre = 0; pre < cur; pre++) {
                    if (context->parser->priv->event_attrvals[pre]->namespace &&
                        g_str_equal (context->parser->priv->event_attrvals[pre]->namespace,
                                     data->namespace) &&
                        g_str_equal (context->parser->priv->event_attrvals[pre]->localname,
                                     data->localname)) {
                        ERROR_NS_DUPATTR(context, data);
                    }
                }
            }

            context->parser->priv->event_attrkeys[cur] = qname;
            context->parser->priv->event_attrvals[cur] = data;
            cur++;
            g_hash_table_steal (context->cur_attrs, key);
        }
        g_hash_table_destroy (context->cur_attrs);
        context->cur_attrs = NULL;
    }

    context->parser->priv->event_type = AXING_STREAM_EVENT_START_ELEMENT;
    axing_stream_emit_event (AXING_STREAM (context->parser));

    if (context->empty) {
        if (frame.nshash) {
            g_hash_table_destroy (frame.nshash);
        }
        g_free (frame.qname);
        g_array_remove_index (context->parser->priv->event_stack,
                              context->parser->priv->event_stack->len - 1);
        context->parser->priv->event_type = AXING_STREAM_EVENT_END_ELEMENT;
        axing_stream_emit_event (AXING_STREAM (context->parser));
    }

 error:
    parser_clean_event_data (context->parser);
    if (context->empty &&
        (context->parser->priv->event_stack->len == context->event_stack_root)) {
        context->state = context->init_state;
        if (context->state == PARSER_STATE_PROLOG)
            context->state = PARSER_STATE_EPILOG;
    }
    else {
        context->state = PARSER_STATE_TEXT;
    }
    context->empty = FALSE;
}

static void
context_complete (ParserContext *context)
{
    if (context->parent)
        context_process_entity_finish (context);
    else
        g_simple_async_result_complete (context->parser->priv->result);
}

static char *
resource_get_basename (AxingResource *resource)
{
    GFile *file = axing_resource_get_file (resource);
    char *ret;
    if (file) {
        char *parsename;
        const char *slash;
        /* FIXME: slash in query/fragment */
        parsename = g_file_get_parse_name (file);
        slash = strrchr (parsename, '/');
        if (slash) {
            ret = g_strdup (slash + 1);
            g_free (parsename);
        }
        else {
            ret = parsename;
        }
    }
    else {
        ret = g_strdup ("-");
    }
    return ret;
}

static ParserContext *
context_new (AxingXmlParser *parser)
{
    ParserContext *context;

    context = g_new0 (ParserContext, 1);
    context->state = PARSER_STATE_NONE;
    context->init_state = PARSER_STATE_PROLOG;
    context->linenum = 1;
    context->colnum = 1;
    context->parser = g_object_ref (parser);
    context->event_stack_root = 0;

    return context;
}

static void
context_free (ParserContext *context)
{
    if (context->parser)
        g_object_unref (context->parser);
    if (context->resource)
        g_object_unref (context->resource);
    if (context->srcstream)
        g_object_unref (context->srcstream);
    if (context->datastream)
        g_object_unref (context->datastream);

    g_free (context->basename);
    g_free (context->entname);
    g_free (context->showname);
    g_free (context->pause_line);
    g_free (context->cur_qname);
    g_free (context->cur_attrname);

    if (context->cur_nshash)
        g_hash_table_destroy (context->cur_nshash);
    if (context->cur_attrs)
        g_hash_table_destroy (context->cur_attrs);
    if (context->cur_text)
        g_string_free (context->cur_text, TRUE);

    g_free (context->decl_system);
    g_free (context->decl_public);
    g_free (context->decl_ndata);

    g_free (context);
}
