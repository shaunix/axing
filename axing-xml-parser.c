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
    PARSER_STATE_ROOT,

    PARSER_STATE_STELM_BASE,
    PARSER_STATE_STELM_ATTNAME,
    PARSER_STATE_STELM_ATTEQ,
    PARSER_STATE_STELM_ATTVAL,
    PARSER_STATE_ENDELM,

    PARSER_STATE_TEXT,
    PARSER_STATE_COMMENT,
    PARSER_STATE_CDATA,

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

typedef struct {
    AxingXmlParser    *parser;
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
} ParserContext;

struct _AxingXmlParserPrivate {
    gboolean    async;

    AxingResource      *resource;
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

static void      context_file_read_cb           (GFile                *file,
                                                 GAsyncResult         *res,
                                                 ParserContext        *context);
static void      context_start_async            (ParserContext        *context);
static void      context_start_cb               (GBufferedInputStream *stream,
                                                 GAsyncResult         *res,
                                                 ParserContext        *context);
static void      context_read_line_cb           (GDataInputStream     *stream,
                                                 GAsyncResult         *res,
                                                 ParserContext        *context);
static void      context_check_end              (ParserContext        *context);
static void      context_parse_xml_decl         (ParserContext        *context);
static void      context_parse_line             (ParserContext        *context,
                                                 char                 *line);
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
static void      context_free                   (ParserContext        *context);


enum {
    PROP_0,
    PROP_RESOURCE,
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

    parser->priv->context = g_new0 (ParserContext, 1);
    parser->priv->context->state = PARSER_STATE_NONE;
    parser->priv->context->init_state = PARSER_STATE_ROOT;
    parser->priv->context->linenum = 0;
    parser->priv->context->colnum = 1;
    parser->priv->context->parser = g_object_ref (parser);
    parser->priv->context->event_stack_root = 0;

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
#ifdef FIXME
transport-encoding
declared-encoding
#endif
}

static void
axing_xml_parser_dispose (GObject *object)
{
    /* FIXME */
    G_OBJECT_CLASS (axing_xml_parser_parent_class)->dispose (object);
}

static void
axing_xml_parser_finalize (GObject *object)
{
    /* FIXME */
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
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT,
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
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT,
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
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT,
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
                          parser->priv->event_type == AXING_STREAM_EVENT_END_ELEMENT,
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

void
axing_xml_parser_parse_async (AxingXmlParser      *parser,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
    GFile *file;
    GInputStream *stream;
    char *parsename;
    const char *slash;
    g_return_if_fail (parser->priv->context->state == PARSER_STATE_NONE);
    parser->priv->context->state = PARSER_STATE_START;

    parser->priv->async = TRUE;
    if (cancellable)
        parser->priv->cancellable = g_object_ref (cancellable);
    parser->priv->result = g_simple_async_result_new (G_OBJECT (parser),
                                                      callback, user_data,
                                                      axing_xml_parser_parse_async);

    file = axing_resource_get_file (parser->priv->resource);
    parsename = g_file_get_parse_name (file);
    slash = strrchr (parsename, '/');
    if (slash) {
        parser->priv->context->basename = g_strdup (slash + 1);
        g_free (parsename);
    }
    else {
        parser->priv->context->basename = parsename;
    }

    stream = axing_resource_get_input_stream (parser->priv->resource);
    if (stream == NULL) {
        g_file_read_async (file,
                           G_PRIORITY_DEFAULT,
                           parser->priv->cancellable,
                           (GAsyncReadyCallback) context_file_read_cb,
                           parser->priv->context);
    }
    else {
        parser->priv->context->srcstream = g_object_ref (stream);
        context_start_async (parser->priv->context);
    }
}

void
axing_xml_parser_parse_finish (AxingXmlParser     *parser,
                               GAsyncResult  *res,
                               GError       **error)
{
    g_warn_if_fail (g_simple_async_result_get_source_tag (G_SIMPLE_ASYNC_RESULT (res)) == axing_xml_parser_parse_async);

    /* FIXME: cleanup */

    if (parser->priv->error && error != NULL)
        *error = g_error_copy (parser->priv->error);
}

#define XML_IS_CHAR(c, context) ((context->parser->priv->xml_version == XML_1_1) ? (c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0x7E) || c == 0x85 || (c  >= 0xA0 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF)) : (c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF)))

#define XML_IS_CHAR_RESTRICTED(c, context) ((context->parser->priv->xml_version == XML_1_1) && ((c >= 0x1 && c <= 0x8) || (c >= 0xB && c <= 0xC) || (c >= 0xE && c <= 0x1F) || (c >= 0x7F && c <= 0x84) || (c >= 0x86 && c <= 0x9F)))

/* FIXME 1.1 newlines */
#define XML_IS_SPACE(c) (c == 0x20 || c == 0x09 || c == 0x0D || c == 0x0A)

#define XML_IS_NAME_START_CHAR(c) (c == ':' || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8 && c <= 0x2FF) || (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D) || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF))

#define XML_IS_NAME_CHAR(c) (XML_IS_NAME_START_CHAR(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0xB7 || (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040))

#define XML_GET_NAME(line, namevar, context) { if (XML_IS_NAME_START_CHAR (g_utf8_get_char (*line))) { GString *name = g_string_new (NULL); char *next; gsize bytes; while (XML_IS_NAME_CHAR (g_utf8_get_char (*line))) { next = g_utf8_next_char (*line); bytes = next - *line; g_string_append_len (name, *line, bytes); *line = next; context->colnum += 1; } namevar = g_string_free (name, FALSE); } else { ERROR_SYNTAX(context); } }

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

/* FIXME: XML 1.1 newlines */
#define EAT_SPACES(c, buf, bufsize, context) {gboolean aftercr = FALSE; while((bufsize < 0 || c - buf < bufsize) && XML_IS_SPACE(c[0])) {if (c[0] == 0x0D) { context->colnum = 1; context->linenum++; aftercr = TRUE; } else if (c[0] == 0x0A) { if (!aftercr) { context->colnum = 1; context->linenum++; } aftercr = FALSE; } else { context->colnum++; aftercr = FALSE; } (c)++; }}

#define CHECK_BUFFER(c, num, buf, bufsize, context) if (c - buf + num > bufsize) { context->parser->priv->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_BUFFER, "Insufficient buffer for XML declaration\n"); goto error; }

#define READ_TO_QUOTE(c, buf, bufsize, context, quot) EAT_SPACES (c, buf, bufsize, context); CHECK_BUFFER (c, 1, buf, bufsize, context); if (c[0] != '=') { ERROR_SYNTAX(context); } c += 1; context->colnum += 1; EAT_SPACES (c, buf, bufsize, context); CHECK_BUFFER (c, 1, buf, bufsize, context); if (c[0] != '"' && c[0] != '\'') { ERROR_SYNTAX(context); } quot = c[0]; c += 1; context->colnum += 1;


static void
context_file_read_cb (GFile         *file,
                      GAsyncResult  *res,
                      ParserContext *context)
{
    context->srcstream = G_INPUT_STREAM (g_file_read_finish (file, res, &context->parser->priv->error));

    if (context->parser->priv->error) {
        g_simple_async_result_complete (context->parser->priv->result);
        return;
    }
    context_start_async (context);
}

void poof (gpointer data, GObject *foo) { g_print ("poof\n"); }

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
    context_parse_xml_decl (context);
    if (context->parser->priv->error) {
        g_simple_async_result_complete (context->parser->priv->result);
        return;
    }
    g_data_input_stream_set_newline_type (context->datastream,
                                          G_DATA_STREAM_NEWLINE_TYPE_ANY);
    g_data_input_stream_read_line_async (context->datastream,
                                         G_PRIORITY_DEFAULT,
                                         context->parser->priv->cancellable,
                                         (GAsyncReadyCallback) context_read_line_cb,
                                         context);
}

static void
context_read_line_cb (GDataInputStream *stream,
                      GAsyncResult     *res,
                      ParserContext    *context)
{
    gchar *line;
    if (context->parser->priv->error) {
        g_simple_async_result_complete (context->parser->priv->result);
        return;
    }
    line = g_data_input_stream_read_line_finish (stream, res, NULL, &(context->parser->priv->error));
    if (line == NULL && context->parser->priv->error == NULL) {
        context_check_end (context);
    }
    if (line == NULL || context->parser->priv->error) {
        g_simple_async_result_complete (context->parser->priv->result);
        return;
    }
    context->linenum++;
    context->colnum = 1;
    context_parse_line (context, line);
    g_data_input_stream_read_line_async (context->datastream,
                                         G_PRIORITY_DEFAULT,
                                         context->parser->priv->cancellable,
                                         (GAsyncReadyCallback) context_read_line_cb,
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
    error:
        return;
    }
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

    if (!(bufsize >= 6 && !strncmp(buf, "<?xml", 5) && XML_IS_SPACE(buf[5]) )) {
        return;
    }
    
    c = buf + 6;
    context->colnum += 6;
    EAT_SPACES (c, buf, bufsize, context);

    CHECK_BUFFER (c, 8, buf, bufsize, context);
    if (!(!strncmp(c, "version", 7) && (c[7] == '=' || XML_IS_SPACE(c[7])) )) {
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
        if (!(!strncmp(c, "encoding", 8) && (c[8] == '=' || XML_IS_SPACE(c[8])) )) {
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
        if (!(!strncmp(c, "standalone", 10) && (c[10] == '=' || XML_IS_SPACE(c[10])) )) {
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

    context->state = PARSER_STATE_ROOT;

 error:
    return;
}

static void
context_parse_line (ParserContext *context, char *line)
{
    if (line[0] == '\0' && context->cur_text != NULL) {
        g_string_append_c (context->cur_text, 0xA);
        return;
    }
    context_parse_data (context, line);
    if (context->state == PARSER_STATE_TEXT) {
        if (context->cur_text == NULL)
            context->cur_text = g_string_new (NULL);
        g_string_append_c (context->cur_text, 0xA);
    }
}

static void
context_parse_data (ParserContext *context, char *line)
{
    char *c = line;
    while (c[0] != '\0') {
        switch (context->state) {
        case PARSER_STATE_START:
        case PARSER_STATE_ROOT:
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

                    g_signal_emit_by_name (context->parser, "stream-event");
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
                    /* FIXME: if STATE_TEXT and empty stack, error */
                    context_parse_start_element (context, &c);
                }
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
    case PARSER_STATE_ROOT:
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
            context->state = PARSER_STATE_ROOT;
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
                if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
                    ERROR_SYNTAX(context);
                break;
            }
            if (!(c == 0x20 || c == 0xD || c == 0xA ||
                  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '!' || c == '#' || c == '$' || c == '%' || (c >= '\'' && c <= '/') ||
                  c == ':' || c == ';' || c == '=' || c == '?' || c == '@' || c == '_')) {
                ERROR_SYNTAX(context);
            }
            g_string_append_c (context->cur_text, c);
            (*line)++; context->colnum++;
        }
        if (context->doctype_state == DOCTYPE_STATE_PUBLIC_VAL && (*line)[0] == '\0') {
            g_string_append_c (context->cur_text, 0xA);
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
            gunichar cp;
            char *next;
            gsize bytes;
            if ((*line)[0] == context->quotechar) {
                char *system = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                axing_dtd_schema_set_system_id (context->parser->priv->doctype, system);
                g_free (system);
                context->doctype_state = DOCTYPE_STATE_EXTID;
                (*line)++; context->colnum++;
                break;
            }
            cp = g_utf8_get_char (*line);
            if (!XML_IS_CHAR(cp, context)) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
        }
        if (context->doctype_state == DOCTYPE_STATE_SYSTEM_VAL && (*line)[0] == '\0') {
            g_string_append_c (context->cur_text, 0xA);
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
            context->state = PARSER_STATE_ROOT;
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
            ERROR_FIXME(context);
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
            ERROR_FIXME(context);
        }
        else if (g_str_has_prefix (*line, "<?")) {
            ERROR_FIXME(context);
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
        context->state = PARSER_STATE_ROOT;
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
        if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_START;
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_START) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->cur_qname, context);
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
            char *next;
            gsize bytes;
            if (!(XML_IS_NAME_CHAR (g_utf8_get_char (*line)) ||
                  XML_IS_SPACE ((*line)[0]) ||
                  (*line)[0] == '(' || (*line)[0] == ')' || (*line)[0] == '|' ||
                  (*line)[0] == ',' || (*line)[0] == '+' || (*line)[0] == '*' ||
                  (*line)[0] == '?' || (*line)[0] == '#' )) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
        }
        if ((*line)[0] == '\0')
            g_string_append_c (context->cur_text, 0xA);
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
        if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_START;
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_START) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->cur_qname, context);
        context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_NAME) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if (XML_IS_NAME_START_CHAR (g_utf8_get_char (*line))) {
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
            char *next;
            gsize bytes;
            if ((*line)[0] == '"') {
                g_string_append_c (context->cur_text, '"');
                (*line)++; context->colnum++;
                context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_QUOTE;
                break;
            }
            if (!(XML_IS_NAME_CHAR (g_utf8_get_char (*line)) ||
                  XML_IS_SPACE ((*line)[0]) ||
                  (*line)[0] == '#' || (*line)[0] == '|' ||
                  (*line)[0] == '(' || (*line)[0] == ')' )) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
        }
        if ((*line)[0] == '\0')
            g_string_append_c (context->cur_text, 0xA);
        if ((*line)[0] == '>')
            context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_AFTER;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_QUOTE) {
        while ((*line)[0] != '\0') {
            char *next;
            gsize bytes;
            if ((*line)[0] == '"') {
                g_string_append_c (context->cur_text, '"');
                (*line)++; context->colnum++;
                context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_VALUE;
                break;
            }
            if (!XML_IS_CHAR(g_utf8_get_char (*line), context)) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
        }
        if ((*line)[0] == '\0')
            g_string_append_c (context->cur_text, 0xA);
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
        if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
            ERROR_SYNTAX(context);
        context->doctype_state = DOCTYPE_STATE_INT_NOTATION_START;
        EAT_SPACES (*line, *line, -1, context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_START) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->cur_qname, context);
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
        if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
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
            gunichar cp;
            char *next;
            gsize bytes;
            if ((*line)[0] == context->quotechar) {
                context->decl_system = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->doctype_state = DOCTYPE_STATE_INT_NOTATION_AFTER;
                (*line)++; context->colnum++;
                break;
            }
            cp = g_utf8_get_char (*line);
            if (!XML_IS_CHAR(cp, context)) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
        }
        if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL && (*line)[0] == '\0') {
            g_string_append_c (context->cur_text, 0xA);
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
                if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0' || (*line)[0] == '>'))
                    ERROR_SYNTAX(context);
                break;
            }
            if (!(c == 0x20 || c == 0xD || c == 0xA ||
                  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '!' || c == '#' || c == '$' || c == '%' || (c >= '\'' && c <= '/') ||
                  c == ':' || c == ';' || c == '=' || c == '?' || c == '@' || c == '_')) {
                ERROR_SYNTAX(context);
            }
            g_string_append_c (context->cur_text, c);
            (*line)++; context->colnum++;
        }
        if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL && (*line)[0] == '\0') {
            g_string_append_c (context->cur_text, 0xA);
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
        if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
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
            if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
                ERROR_SYNTAX(context);
            EAT_SPACES (*line, *line, -1, context);
            if ((*line)[0] == '\0')
                return;
        }
        XML_GET_NAME(line, context->cur_qname, context);
        context->doctype_state = DOCTYPE_STATE_INT_ENTITY_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_NAME) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        if (g_str_has_prefix (*line, "SYSTEM")) {
            (*line) += 6; context->colnum += 6;
            if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
                ERROR_SYNTAX(context);
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_SYSTEM;
            EAT_SPACES (*line, *line, -1, context);
        }
        else if (g_str_has_prefix (*line, "PUBLIC")) {
            (*line) += 6; context->colnum += 6;
            if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
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
            gunichar cp;
            char *next;
            gsize bytes;
            if ((*line)[0] == context->quotechar) {
                /* let the AFTER handler handle cur_text */
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
                (*line)++; context->colnum++;
                break;
            }
            cp = g_utf8_get_char (*line);
            if (!XML_IS_CHAR(cp, context)) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
        }
        if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_VALUE && (*line)[0] == '\0') {
            g_string_append_c (context->cur_text, 0xA);
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
                if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
                    ERROR_SYNTAX(context);
                break;
            }
            if (!(c == 0x20 || c == 0xD || c == 0xA ||
                  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '!' || c == '#' || c == '$' || c == '%' || (c >= '\'' && c <= '/') ||
                  c == ':' || c == ';' || c == '=' || c == '?' || c == '@' || c == '_')) {
                ERROR_SYNTAX(context);
            }
            g_string_append_c (context->cur_text, c);
            (*line)++; context->colnum++;
        }
        if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_PUBLIC_VAL && (*line)[0] == '\0') {
            g_string_append_c (context->cur_text, 0xA);
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
            gunichar cp;
            char *next;
            gsize bytes;
            if ((*line)[0] == context->quotechar) {
                context->decl_system = g_string_free (context->cur_text, FALSE);
                context->cur_text = NULL;
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
                (*line)++; context->colnum++;
                break;
            }
            cp = g_utf8_get_char (*line);
            if (!XML_IS_CHAR(cp, context)) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
        }
        if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_SYSTEM_VAL && (*line)[0] == '\0') {
            g_string_append_c (context->cur_text, 0xA);
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_NDATA) {
        EAT_SPACES (*line, *line, -1, context);
        if ((*line)[0] == '\0')
            return;
        XML_GET_NAME(line, context->decl_ndata, context);
        if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0' || (*line)[0] == '>'))
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
            if (!(XML_IS_SPACE((*line)[0]) || (*line)[0] == '\0'))
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
        gunichar cp;
        char *next;
        gsize bytes;
        if ((*line)[0] == ']' && (*line)[1] == ']' && (*line)[2] == '>') {
            (*line) += 3; context->colnum += 3;

            context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
            context->cur_text = NULL;
            context->parser->priv->event_type = AXING_STREAM_EVENT_CDATA;

            g_signal_emit_by_name (context->parser, "stream-event");
            parser_clean_event_data (context->parser);
            context->state = PARSER_STATE_TEXT;
            return;
        }
        cp = g_utf8_get_char (*line);
        if (!XML_IS_CHAR(cp, context)) {
            ERROR_SYNTAX(context);
        }
        next = g_utf8_next_char (*line);
        bytes = next - *line;
        /* FIXME: newlines */
        g_string_append_len (context->cur_text, *line, bytes);
        *line = next; context->colnum += 1;
    }

    if ((*line)[0] == '\0')
        g_string_append_c (context->cur_text, 0xA);

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
        gunichar cp;
        char *next;
        gsize bytes;
        if ((*line)[0] == '-' && (*line)[1] == '-') {
            if ((*line)[2] != '>') {
                ERROR_SYNTAX(context);
            }
            (*line) += 3; context->colnum += 3;

            context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
            context->cur_text = NULL;
            context->parser->priv->event_type = AXING_STREAM_EVENT_COMMENT;

            g_signal_emit_by_name (context->parser, "stream-event");
            parser_clean_event_data (context->parser);
            context->state = context->prev_state;
            return;
        }
        cp = g_utf8_get_char (*line);
        if (!XML_IS_CHAR(cp, context)) {
            ERROR_SYNTAX(context);
        }
        next = g_utf8_next_char (*line);
        bytes = next - *line;
        /* FIXME: newlines */
        g_string_append_len (context->cur_text, *line, bytes);
        *line = next; context->colnum += 1;
    }

    if ((*line)[0] == '\0')
        g_string_append_c (context->cur_text, 0xA);

 error:
    return;
}

static void
context_parse_instruction (ParserContext *context, char **line)
{
    ERROR_FIXME (context);
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
    g_signal_emit_by_name (context->parser, "stream-event");

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
        context_trigger_start_element (context);
        (*line)++; context->colnum++;
        context->state = PARSER_STATE_TEXT;
    }
    else if ((*line)[0] == '/' && (*line)[1] == '>') {
        context->empty = TRUE;
        context_trigger_start_element (context);
        (*line) += 2; context->colnum += 2;
        context->state = PARSER_STATE_TEXT;
    }
    else {
        context->state = PARSER_STATE_STELM_BASE;
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
            context_trigger_start_element (context);
            (*line)++; context->colnum++;
            context->state = PARSER_STATE_TEXT;
            return;
        }
        else if ((*line)[0] == '/' && (*line)[1] == '>') {
            context->empty = TRUE;
            context_trigger_start_element (context);
            (*line) += 2; context->colnum += 2;
            context->state = PARSER_STATE_TEXT;
            return;
        }
        if ((*line)[0] == '\0') {
            return;
        }
        context->attr_linenum = context->linenum;
        context->attr_colnum = context->colnum;

        XML_GET_NAME(line, context->cur_attrname, context);
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
            gunichar cp;
            char *next;
            gsize bytes;
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
                        context->cur_attrs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                    g_free,
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
                      (*line)[0] == '\0' || XML_IS_SPACE ((*line)[0]))) {
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
            cp = g_utf8_get_char (*line);
            if (!XML_IS_CHAR(cp, context)) {
                ERROR_SYNTAX(context);
            }
            next = g_utf8_next_char (*line);
            bytes = next - *line;
            g_string_append_len (context->cur_text, *line, bytes);
            *line = next; context->colnum += 1;
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
            char *value = axing_dtd_schema_get_entity (context->parser->priv->doctype, entname);
            if (value) {
                ParserContext *entctxt = g_new0 (ParserContext, 1);
                /* not duping these two, NULL them before free below */
                entctxt->basename = context->basename;
                entctxt->entname = entname;
                entctxt->showname = g_strdup_printf ("%s(&%s;)", entctxt->basename, entname);
                entctxt->state = context->state;
                entctxt->init_state = context->state;
                entctxt->linenum = 1;
                entctxt->colnum = 1;
                entctxt->parser = g_object_ref (context->parser);
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
            else {
                ERROR_FIXME(context);
            }
        }
    }
 error:
    context->colnum = colnum;
    g_free (entname);
}

static void
context_parse_text (ParserContext *context, char **line)
{
    gunichar cp;
    char *next;
    gsize bytes;
    while ((*line)[0] != '\0') {
        if ((*line)[0] == '<') {
            g_free (context->parser->priv->event_content);
            context->parser->priv->event_content = g_string_free (context->cur_text, FALSE);
            context->cur_text = NULL;
            context->parser->priv->event_type = AXING_STREAM_EVENT_CONTENT;

            g_signal_emit_by_name (context->parser, "stream-event");
            parser_clean_event_data (context->parser);
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
        cp = g_utf8_get_char (*line);
        if (!XML_IS_CHAR (cp, context)) {
            ERROR_SYNTAX(context);
        }
        next = g_utf8_next_char (*line);
        bytes = next - *line;
        /* FIXME: newlines */
        g_string_append_len (context->cur_text, *line, bytes);
        *line = next; context->colnum += 1;
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
    g_signal_emit_by_name (context->parser, "stream-event");

    if (context->empty) {
        if (frame.nshash) {
            g_hash_table_destroy (frame.nshash);
        }
        g_free (frame.qname);
        g_array_remove_index (context->parser->priv->event_stack,
                              context->parser->priv->event_stack->len - 1);
        context->parser->priv->event_type = AXING_STREAM_EVENT_END_ELEMENT;
        g_signal_emit_by_name (context->parser, "stream-event");
        context->empty = FALSE;
    }

 error:
    parser_clean_event_data (context->parser);
}

static void
context_free (ParserContext *context)
{
    if (context->parser)
        g_object_unref (context->parser);
    if (context->srcstream)
        g_object_unref (context->srcstream);
    if (context->datastream)
        g_object_unref (context->datastream);

    g_free (context->basename);
    g_free (context->entname);
    g_free (context->showname);
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
