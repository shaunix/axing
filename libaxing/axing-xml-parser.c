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

#include "axing-enums.h"
#include "axing-dtd-schema.h"
#include "axing-private.h"
#include "axing-resource.h"
#include "axing-reader.h"
#include "axing-xml-parser.h"
#include "axing-utf8.h"


#if 1
#define AXING_DEBUG(args...) if (g_getenv("AXING_DEBUG")) g_print(args);
#else
#define AXING_DEBUG(args...)
#endif


#define NS_XML "http://www.w3.org/XML/1998/namespace"

#define EVENTPOOLSIZE 32

#define EQ2(s, c1, c2) \
    ((guchar)(s)[0] == c1 && ((guchar)(s)[1] == c2))
#define EQ3(s, c1, c2, c3) \
    (EQ2(s, c1, c2) && ((guchar)(s)[2] == c3))
#define EQ4(s, c1, c2, c3, c4) \
    (EQ3(s, c1, c2, c3) && ((guchar)(s)[3] == c4))
#define EQ5(s, c1, c2, c3, c4, c5) \
    (EQ4(s, c1, c2, c3, c4) && ((guchar)(s)[4] == c5))
#define EQ6(s, c1, c2, c3, c4, c5, c6) \
    (EQ5(s, c1, c2, c3, c4, c5) && ((guchar)(s)[5] == c6))
#define EQ7(s, c1, c2, c3, c4, c5, c6, c7) \
    (EQ6(s, c1, c2, c3, c4, c5, c6) && ((guchar)(s)[6] == c7))
#define EQ8(s, c1, c2, c3, c4, c5, c6, c7, c8) \
    (EQ7(s, c1, c2, c3, c4, c5, c6, c7) && ((guchar)(s)[7] == c8))
#define EQ9(s, c1, c2, c3, c4, c5, c6, c7, c8, c9) \
    (EQ8(s, c1, c2, c3, c4, c5, c6, c7, c8) && ((guchar)(s)[8] == c9))
#define EQ10(s, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10) \
    (EQ9(s, c1, c2, c3, c4, c5, c6, c7, c8, c9) && ((guchar)(s)[9] == c10))


typedef enum {
    PARSER_STATE_NONE,

    PARSER_STATE_START,    /* starting state for main files */
    PARSER_STATE_TEXTDECL, /* starting state for external parsed entities */
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

#define STATE_TO_STRING(state)                                             \
    (state == PARSER_STATE_NONE) ? "PARSER_STATE_NONE" :                   \
    (state == PARSER_STATE_START) ? "PARSER_STATE_START" :                 \
    (state == PARSER_STATE_TEXTDECL) ? "PARSER_STATE_TEXTDECL" :           \
    (state == PARSER_STATE_PROLOG) ? "PARSER_STATE_PROLOG" :               \
    (state == PARSER_STATE_EPILOG) ? "PARSER_STATE_EPILOG" :               \
    (state == PARSER_STATE_STELM_BASE) ? "PARSER_STATE_STELM_BASE" :       \
    (state == PARSER_STATE_STELM_ATTNAME) ? "PARSER_STATE_STELM_ATTNAME" : \
    (state == PARSER_STATE_STELM_ATTEQ) ? "PARSER_STATE_STELM_ATTEQ" :     \
    (state == PARSER_STATE_STELM_ATTVAL) ? "PARSER_STATE_STELM_ATTVAL" :   \
    (state == PARSER_STATE_ENDELM) ? "PARSER_STATE_ENDELM" :               \
    (state == PARSER_STATE_TEXT) ? "PARSER_STATE_TEXT" :                   \
    (state == PARSER_STATE_COMMENT) ? "PARSER_STATE_COMMENT" :             \
    (state == PARSER_STATE_CDATA) ? "PARSER_STATE_CDATA" :                 \
    (state == PARSER_STATE_INSTRUCTION) ? "PARSER_STATE_INSTRUCTION" :     \
    (state == PARSER_STATE_DOCTYPE) ? "PARSER_STATE_DOCTYPE" :             \
    (state == PARSER_STATE_NULL) ? "PARSER_STATE_NULL" :                   \
    "PARSER_STATE_????"

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

// FIXME I think I'd like this public, but we don't have an accessor yet
typedef enum {
    BOM_ENCODING_NONE,
    BOM_ENCODING_UCS4_BE,
    BOM_ENCODING_UCS4_LE,
    BOM_ENCODING_UTF16_BE,
    BOM_ENCODING_UTF16_LE,
    BOM_ENCODING_4BYTE_BE,
    BOM_ENCODING_4BYTE_LE,
    BOM_ENCODING_2BYTE_BE,
    BOM_ENCODING_2BYTE_LE,
    BOM_ENCODING_UTF8
} BomEncoding;

typedef struct _Context Context;
struct _Context {
    Context           *parent;
    AxingXmlParser    *parser;
    AxingResource     *resource;
    GInputStream      *srcstream;
    GDataInputStream  *datastream;
    char              *basename;
    char              *entname;
    char              *showname;

    char          *line;
    char          *linecur; /* points inside line, do not free */

    ParserState    state;
    /* For primary contexts, init_state is always PROLOG. When parsing
       entities, init_state is the state of the surrounding context,
       e.g. STELM_ATTVAL or TEXT. FIXME: Should we drop init_state and
       just check for a parent context?
    */
    ParserState    init_state;
    ParserState    prev_state;
    DoctypeState   doctype_state;
    BomEncoding    bom_encoding;
    gboolean       bom_checked;
    int            linenum;
    int            colnum;

    char          *pause_line;
    char          *cur_qname;
    char           quotechar;
    char          *decl_system;
    char          *decl_public;
    gboolean       decl_pedef;
    char          *decl_ndata;
};

typedef struct _Event Event;
struct _Event {
    Event *parent;

    gboolean inuse;

    char *qname;
    char *prefix;
    char *localname;
    char *namespace;
    char *nsname;
    char *content;

    char **attrkeys; /* strings owned by attrs structs, just g_free */
    Event *attrs;
    Event *xmlns;

    gboolean empty;

    int linenum;
    int colnum;

    Context *context;
};

struct _AxingXmlParser {
    GObject     parent;

    gboolean    async;

    AxingResource      *resource;
    AxingResolver      *resolver;
    Context            *context;
    GCancellable       *cancellable;
    GSimpleAsyncResult *result;
    GError             *error;

    AxingXmlVersion     xml_version;

    AxingDtdSchema      *doctype;

    AxingNodeType        event_type;
    Event               *event;

    GString             *cur_text;

    int                  txtlinenum;
    int                  txtcolnum;

    Event                eventpool[EVENTPOOLSIZE];
    int                  eventpoolstart;
};


static void      axing_xml_parser_init          (AxingXmlParser       *parser);
static void      axing_xml_parser_class_init    (AxingXmlParserClass  *klass);
static void      axing_xml_parser_init_reader   (AxingReaderInterface *iface);
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

static void      parser_clear_event             (AxingXmlParser       *parser);

static gboolean              reader_read                    (AxingReader        *reader,
                                                             GError            **error);
static void                  reader_read_async              (AxingReader        *reader,
                                                             GCancellable       *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer            user_data);
static gboolean              reader_read_finish             (AxingReader        *reader,
                                                             GAsyncResult       *result,
                                                             GError            **error);

static AxingNodeType         reader_get_node_type           (AxingReader    *reader);
static const char *          reader_get_qname               (AxingReader    *reader);
static const char *          reader_get_prefix              (AxingReader    *reader);
static const char *          reader_get_localname           (AxingReader    *reader);
static const char *          reader_get_namespace           (AxingReader    *reader);
static const char *          reader_get_nsname              (AxingReader    *reader);

static const char *          reader_get_content             (AxingReader    *reader);

static int                   reader_get_linenum             (AxingReader    *reader);
static int                   reader_get_colnum              (AxingReader    *reader);

static const char *          reader_lookup_namespace        (AxingReader    *reader,
                                                             const char     *prefix);

static const char * const *  reader_get_attrs               (AxingReader    *reader);
static const char *          reader_get_attr_localname      (AxingReader    *reader, const char *qname);
static const char *          reader_get_attr_prefix         (AxingReader    *reader, const char *qname);
static const char *          reader_get_attr_namespace      (AxingReader    *reader, const char *qname);
static const char *          reader_get_attr_nsname         (AxingReader    *reader, const char *qname);
static const char *          reader_get_attr_value          (AxingReader    *reader, const char *qname);
static int                   reader_get_attr_linenum        (AxingReader    *reader, const char *qname);
static int                   reader_get_attr_colnum         (AxingReader    *reader, const char *qname);


#ifdef REFACTOR
static void      context_resource_read_cb       (AxingResource        *resource,
                                                 GAsyncResult         *result,
                                                 Context              *context);
#endif /* REFACTOR */

static void      context_start_sync             (Context              *context);

#ifdef REFACTOR
static void      context_start_async            (Context              *context);
static void      context_start_cb               (GBufferedInputStream *stream,
                                                 GAsyncResult         *res,
                                                 Context              *context);
static void      context_read_data_cb           (GDataInputStream     *stream,
                                                 GAsyncResult         *res,
                                                 Context              *context);
#endif /* REFACTOR */

static void      context_check_end              (Context              *context);
static void      context_set_encoding           (Context              *context,
                                                 const char           *encoding);
static gboolean  context_parse_bom              (Context              *context);
static void      context_parse_xml_decl         (Context              *context);
static void      context_parse_line             (Context              *context);

static void      context_parse_doctype          (Context              *context);
static void      context_parse_doctype_element  (Context              *context);
static void      context_parse_doctype_attlist  (Context              *context);
static void      context_parse_doctype_notation (Context              *context);
static void      context_parse_doctype_entity   (Context              *context);
static void      context_parse_parameter        (Context              *context);
static void      context_parse_cdata            (Context              *context);
static void      context_parse_comment          (Context              *context);
static void      context_parse_instruction      (Context              *context);
static void      context_parse_end_element      (Context              *context);
static void      context_parse_start_element    (Context              *context);
static void      context_parse_attrs            (Context              *context);
static void      context_parse_entity           (Context              *context);
static void      context_parse_text             (Context              *context);
static void      context_finish_start_element   (Context              *context);

#ifdef REFACTOR
static void      context_complete               (Context              *context);
#endif /* REFACTOR */

static void      context_process_entity         (Context              *context,
                                                 const char           *entname);

#ifdef REFACTOR
static void      context_process_entity_resolved(AxingResolver        *resolver,
                                                 GAsyncResult         *result,
                                                 Context              *context);
static void      context_process_entity_finish  (Context              *contetext);
#endif /* REFACTOR */

static char *    resource_get_basename          (AxingResource        *resource);

static inline Context *  context_new            (AxingXmlParser       *parser);
static inline void       context_free           (Context              *context);

static inline Event *    event_new              (Context              *context);
static inline void       event_free             (Event                *data);


enum {
    PROP_0,
    PROP_RESOURCE,
    PROP_RESOLVER,
    N_PROPS
};

G_DEFINE_TYPE_WITH_CODE (AxingXmlParser, axing_xml_parser, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (AXING_TYPE_READER,
                                                axing_xml_parser_init_reader))

static void
axing_xml_parser_init (AxingXmlParser *parser)
{
    parser->context = context_new (parser);
    parser->context->state = PARSER_STATE_START;
    parser->cur_text = g_string_sized_new (128);
}

static void
axing_xml_parser_class_init (AxingXmlParserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
axing_xml_parser_init_reader (AxingReaderInterface *iface)
{
    iface->read = reader_read;
    iface->read_async = reader_read_async;
    iface->read_finish = reader_read_finish;

    iface->get_node_type = reader_get_node_type;

    iface->get_qname = reader_get_qname;
    iface->get_localname = reader_get_localname;
    iface->get_prefix = reader_get_prefix;
    iface->get_namespace = reader_get_namespace;
    iface->get_nsname = reader_get_nsname;

    iface->get_content = reader_get_content;
    iface->get_linenum = reader_get_linenum;
    iface->get_colnum = reader_get_colnum;

    iface->get_attrs = reader_get_attrs;
    iface->get_attr_localname = reader_get_attr_localname;
    iface->get_attr_prefix = reader_get_attr_prefix;
    iface->get_attr_namespace = reader_get_attr_namespace;
    iface->get_attr_nsname = reader_get_attr_nsname;
    iface->get_attr_value = reader_get_attr_value;
    iface->get_attr_linenum = reader_get_attr_linenum;
    iface->get_attr_colnum = reader_get_attr_colnum;
}

static void
axing_xml_parser_dispose (GObject *object)
{
    AxingXmlParser *parser = AXING_XML_PARSER (object);

    g_clear_object (&parser->resource);
    g_clear_object (&parser->resolver);
    g_clear_object (&parser->cancellable);
    g_clear_object (&parser->result);
    g_clear_object (&parser->doctype);

    while (parser->context) {
        Context *parent = parser->context->parent;
        context_free (parser->context);
        parser->context = parent;
    }

    G_OBJECT_CLASS (axing_xml_parser_parent_class)->dispose (object);
}

static void
axing_xml_parser_finalize (GObject *object)
{
    AxingXmlParser *parser = AXING_XML_PARSER (object);

    g_clear_error (&(parser->error));

    parser_clear_event (parser);

/* REFACTOR remove and free ->event if necessary
    if (parser->event_stack)
        g_array_free (parser->event_stack, TRUE);
*/

    if (parser->cur_text)
        g_string_free (parser->cur_text, TRUE);

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
        g_value_set_object (value, parser->resource);
        break;
    case PROP_RESOLVER:
        g_value_set_object (value, parser->resolver);
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
        if (parser->resource)
            g_object_unref (parser->resource);
        parser->resource = AXING_RESOURCE (g_value_dup_object (value));
        parser->context->resource = g_object_ref (parser->resource);
        parser->context->basename = resource_get_basename (parser->resource);
        break;
    case PROP_RESOLVER:
        if (parser->resolver)
            g_object_unref (parser->resolver);
        parser->resolver = AXING_RESOLVER (g_value_dup_object (value));
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
    if (resolver) {
        return g_object_new (AXING_TYPE_XML_PARSER,
                             "resource", resource,
                             "resolver", resolver,
                             NULL);
    }
    else {
        return g_object_new (AXING_TYPE_XML_PARSER,
                             "resource", resource,
                             NULL);
    }
}


static void
parser_clear_event (AxingXmlParser *parser)
{
    Event *event;
    if (parser->event_type == AXING_NODE_TYPE_ELEMENT && parser->event->empty) {
        parser->event_type = AXING_NODE_TYPE_END_ELEMENT;
        return;
    }
    switch (parser->event_type) {
    case AXING_NODE_TYPE_COMMENT:
    case AXING_NODE_TYPE_CONTENT:
    case AXING_NODE_TYPE_CDATA:
        g_string_truncate (parser->cur_text, 0);
        break;
    case AXING_NODE_TYPE_INSTRUCTION:
        g_string_truncate (parser->cur_text, 0);
        if (parser->context->cur_qname)
            g_clear_pointer (&(parser->context->cur_qname), g_free);
        break;
    case AXING_NODE_TYPE_END_ELEMENT:
        event = parser->event;
        parser->event = parser->event->parent;
        event_free (event);
        break;
    default:
        break;
    }
    parser->event_type = AXING_NODE_TYPE_NONE;
}


gboolean
reader_read (AxingReader  *reader,
             GError      **error)
{
    AxingXmlParser *parser = AXING_XML_PARSER (reader);

    AXING_DEBUG ("reader_read\n");

    if (parser->context->state == PARSER_STATE_START) {
        context_start_sync (parser->context);
        /* REFACTOR FIXME: return TRUE if decl? */
    }

    parser_clear_event (parser);
    if (parser->event_type != AXING_NODE_TYPE_NONE)
        return TRUE;

    while (TRUE) {
        if (parser->context->line == NULL) {
            /* If just parsing a string, there's no stream, and this context is done */
            if (parser->context->datastream) {
                AXING_DEBUG ("  READ %i\n", parser->context->datastream);
                parser->context->line = g_data_input_stream_read_line (parser->context->datastream, NULL,
                                                                       parser->context->parser->cancellable,
                                                                       &(parser->error));
                if (parser->error)
                    goto error;
            }
            if (parser->context->line == NULL) {
                context_check_end (parser->context);
                if (parser->error)
                    goto error;
                if (parser->context->parent != NULL) {
                    Context *parent = parser->context->parent;
                    AXING_DEBUG ("  POP CONTEXT\n");
                    context_free (parser->context);
                    parser->context = parent;
                }
                else {
                    /* REFACTOR
                       return END OF DOCUMENT?
                    */
                    return FALSE;
                }
            }
            else {
                parser->context->linecur = parser->context->line;
            }
        }
        while (parser->context->linecur && parser->context->linecur[0] != '\0') {
            context_parse_line (parser->context);
            if (parser->error)
                goto error;
            if (parser->event_type != AXING_NODE_TYPE_NONE)
                return TRUE;
        }
        if (parser->context->linecur && parser->context->linecur[0] == '\0') {
            /* FIXME: I really don't like this. This is the only spot we reliably hit
               at the end of each line. g_data_input_stream_read_line doesn't give us
               the newline character, so we'll hand a newline to context_parse_line.
               But if the last character was CR, then the parser has already done a
               line break. XML normalizes line breaks, so in that case we don't feed
               the extra LF in.

               Aside from being kind of ugly, we also have the problem that using
               g_data_input_stream_read_line will only ever be able to give a single
               chunk for files using non-LF line endings, making large documents
               impossible without LF.

               This also introduces a bug by always putting a line ending at the
               end of a parsed resource. This bug is tested in entities28, which
               is currently commented out because it fails.

               Ideally, we'd want to use a read function that gives as much data as
               it can from its internal buffer, up to and including the last-seen
               space, newline, or perhaps right angle bracket. I might need to write
               my own GInputStream wrapper, but I really don't want to.
            */
            char eol[2] = {0x0A, 0x00};
            if (parser->context->datastream &&
                (parser->context->line == parser->context->linecur ||
                 (parser->context->linecur - 1)[0] != 0x0D)) {
                g_free (parser->context->line);
                parser->context->line = eol;
                parser->context->linecur = parser->context->line;
                context_parse_line (parser->context);
            }
            else {
                g_free (parser->context->line);
            }
            parser->context->line = NULL;
            parser->context->linecur = NULL;
        }
    }

 error:
    if (parser->error) {
        parser->event_type = AXING_NODE_TYPE_ERROR;
        *error = parser->error;
    }
    return FALSE;
}


void
reader_read_async (AxingReader        *reader,
                   GCancellable       *cancellable,
                   GAsyncReadyCallback callback,
                   gpointer            user_data)
{
    /* FIXME REFACTOR */
    /*
    if (cancellable)
        parser->priv->cancellable = g_object_ref (cancellable);
    */
}


gboolean
reader_read_finish (AxingReader   *reader,
                    GAsyncResult  *result,
                    GError       **error)
{
    /* FIXME REFACTOR */
}


static AxingNodeType
reader_get_node_type (AxingReader *reader)
{
    return AXING_XML_PARSER (reader)->event_type;
}


static const char *
reader_get_qname (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT ||
                          parser->event_type == AXING_NODE_TYPE_END_ELEMENT ||
                          parser->event_type == AXING_NODE_TYPE_INSTRUCTION,
                          NULL);
    if (parser->event_type == AXING_NODE_TYPE_INSTRUCTION)
        return parser->context->cur_qname;
    return parser->event->qname;
}


static const char *
reader_get_prefix (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT ||
                          parser->event_type == AXING_NODE_TYPE_END_ELEMENT,
                          NULL);
    return parser->event->prefix ? parser->event->prefix : "";
}


static const char *
reader_get_localname (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT ||
                          parser->event_type == AXING_NODE_TYPE_END_ELEMENT,
                          NULL);
    return parser->event->localname ? parser->event->localname : parser->event->qname;
}


static const char *
reader_get_namespace (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT ||
                          parser->event_type == AXING_NODE_TYPE_END_ELEMENT,
                          NULL); 
    return parser->event->namespace ? parser->event->namespace : "";
}


static const char *
reader_get_nsname (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT ||
                          parser->event_type == AXING_NODE_TYPE_END_ELEMENT,
                          NULL);
    if (parser->event->nsname == NULL)
        parser->event->nsname = g_strjoin (NULL,
                                           "{",
                                           parser->event->namespace ? parser->event->namespace : "",
                                           "}",
                                           parser->event->localname ? parser->event->localname : parser->event->qname,
                                           NULL);
    return parser->event->nsname;
}


static const char *
reader_lookup_namespace (AxingReader *reader,
                         const char  *prefix)
{
    AxingXmlParser *parser = AXING_XML_PARSER (reader);
    Event *event, *xmlns;
    if (EQ4 (prefix, 'x', 'm', 'l', '\0')) {
        return NS_XML;
    }
    for (event = parser->event; event; event = event->parent) {
        for (xmlns = event->xmlns; xmlns; xmlns = xmlns->parent) {
            /* Fun fact: we only put things in here when the qname is xmlns or
               starts with xmlns:, and we just leave the qname alone instead
               of duping out the prefix. So this is totally safe.
             */
            if (EQ6 (xmlns->qname, 'x', 'm', 'l', 'n', 's', '\0')) {
                if (prefix[0] == '\0')
                    return xmlns->content;
            }
            else if (g_str_equal (prefix, xmlns->qname + 6)) {
                return xmlns->content;
            }
        }
    }
    return NULL;
}


static const char *
reader_get_content (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_CONTENT ||
                          parser->event_type == AXING_NODE_TYPE_COMMENT ||
                          parser->event_type == AXING_NODE_TYPE_CDATA   ||
                          parser->event_type == AXING_NODE_TYPE_INSTRUCTION,
                          NULL);
    return parser->cur_text->str;
}


static int
reader_get_linenum(AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), 0);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type != AXING_NODE_TYPE_NONE, 0);
    if (parser->event_type == AXING_NODE_TYPE_CONTENT ||
        parser->event_type == AXING_NODE_TYPE_COMMENT ||
        parser->event_type == AXING_NODE_TYPE_CDATA   ||
        parser->event_type == AXING_NODE_TYPE_INSTRUCTION)
        return parser->txtlinenum;
    else
        return parser->event->linenum;
}


static int
reader_get_colnum (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), 0);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type != AXING_NODE_TYPE_NONE, 0);
    if (parser->event_type == AXING_NODE_TYPE_CONTENT ||
        parser->event_type == AXING_NODE_TYPE_COMMENT ||
        parser->event_type == AXING_NODE_TYPE_CDATA   ||
        parser->event_type == AXING_NODE_TYPE_INSTRUCTION)
        return parser->txtcolnum;
    else
        return parser->event->colnum;
}


static char *nokeys[1] = {NULL};

static const char * const *
reader_get_attrs (AxingReader *reader)
{
    AxingXmlParser *parser;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, NULL);
    if (parser->event->attrkeys == NULL) {
        return (const char * const *) nokeys;
    }
    return (const char * const *) parser->event->attrkeys;
}


static const char *
reader_get_attr_localname (AxingReader *reader, const char *qname)
{
    AxingXmlParser *parser;
    Event *attr;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, NULL);
    for (attr = parser->event->attrs; attr; attr = attr->parent)
        if (g_str_equal (qname, attr->qname))
            return attr->localname ? attr->localname : attr->qname;
    return NULL;
}


static const char *
reader_get_attr_prefix (AxingReader *reader, const char *qname)
{
    AxingXmlParser *parser;
    Event *attr;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, NULL);
    for (attr = parser->event->attrs; attr; attr = attr->parent)
        if (g_str_equal (qname, attr->qname))
            return attr->prefix ? attr->prefix : "";
    return NULL;
}


static const char *
reader_get_attr_namespace (AxingReader *reader, const char *qname)
{
    AxingXmlParser *parser;
    Event *attr;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, NULL);
    for (attr = parser->event->attrs; attr; attr = attr->parent)
        if (g_str_equal (qname, attr->qname))
            return attr->namespace ? attr->namespace : "";
    return NULL;
}


static const char *
reader_get_attr_nsname (AxingReader *reader, const char *qname)
{
    AxingXmlParser *parser;
    Event *attr;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, NULL);
    for (attr = parser->event->attrs; attr; attr = attr->parent)
        if (g_str_equal (qname, attr->qname)) {
            if (attr->nsname == NULL)
                attr->nsname = g_strjoin (NULL,
                                          "{",
                                          attr->namespace ? attr->namespace : "",
                                          "}",
                                          attr->localname ? attr->localname : attr->qname,
                                          NULL);
            return attr->nsname;
        }
}


const char *
reader_get_attr_value (AxingReader *reader,
                       const char  *qname)
{
    AxingXmlParser *parser;
    Event *attr;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), NULL);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, NULL);
    for (attr = parser->event->attrs; attr; attr = attr->parent)
        if (g_str_equal (qname, attr->qname))
            return attr->content ? attr->content : "";
    return NULL;
}


static int
reader_get_attr_linenum (AxingReader *reader, const char *qname)
{
    AxingXmlParser *parser;
    Event *attr;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), 0);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, 0);
    for (attr = parser->event->attrs; attr; attr = attr->parent)
        if (g_str_equal (qname, attr->qname))
            return attr->linenum;
    return 0;
}


static int
reader_get_attr_colnum (AxingReader *reader, const char *qname)
{
    AxingXmlParser *parser;
    Event *attr;
    g_return_val_if_fail (AXING_IS_XML_PARSER (reader), 0);
    parser = (AxingXmlParser *) reader;
    g_return_val_if_fail (parser->event_type == AXING_NODE_TYPE_ELEMENT, 0);
    for (attr = parser->event->attrs; attr; attr = attr->parent)
        if (g_str_equal (qname, attr->qname))
            return attr->colnum;
    return 0;
}



#ifdef REFACTOR
void
axing_xml_parser_parse (AxingXmlParser  *parser,
                        GCancellable    *cancellable,
                        GError         **error)
{
    g_return_if_fail (parser->priv->context == NULL);

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
#endif /* REFACTOR */


static void
context_start_sync (Context *context)
{
    char *line;
    AXING_DEBUG ("context_start_sync\n");

    context->srcstream = axing_resource_read (context->resource,
                                              context->parser->cancellable,
                                              &(context->parser->error));
    if (context->parser->error)
        goto error;

    /* The AxingResource accessors don't ref. Maybe they should. */
    g_object_ref (context->srcstream);

    context->datastream = g_data_input_stream_new (context->srcstream);
    if (context->state == PARSER_STATE_START || context->state == PARSER_STATE_TEXTDECL) {
        gboolean reencoded;
        g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (context->datastream),
                                      1024,
                                      context->parser->cancellable,
                                      &(context->parser->error));
        if (context->parser->error)
            goto error;

        reencoded = context_parse_bom (context);
        if (context->parser->error)
            goto error;

        if (reencoded) {
            g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (context->datastream),
                                          1024,
                                          context->parser->cancellable,
                                          &(context->parser->error));
            if (context->parser->error)
                goto error;
        }

        context_parse_xml_decl (context);
        if (context->parser->error)
            goto error;

        if (context->state == PARSER_STATE_TEXTDECL)
            context->state = context->init_state;
    }

    g_data_input_stream_set_newline_type (context->datastream,
                                          G_DATA_STREAM_NEWLINE_TYPE_LF);
 error:
    return;
}


#ifdef REFACTOR
void
axing_xml_parser_parse_async (AxingXmlParser      *parser,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
    Context *context;
    GFile *file;
    GInputStream *stream;

    g_return_if_fail (parser->priv->context == NULL);

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

#endif /* REFACTOR */


#define XML_IS_CHAR(cp, context)                                          \
    ((context->parser->xml_version == AXING_XML_VERSION_1_1) ?            \
     (cp == 0x09 || cp == 0x0A || cp == 0x0D || cp == 0x85 ||             \
      (cp >= 0x20 && cp <= 0x7E) || (cp  >= 0xA0 && cp <= 0xD7FF) ||      \
      (cp >= 0xE000 && cp <= 0xFFFD) || (cp >= 0x10000 && cp <= 0x10FFFF) \
     ) :                                                                  \
     (cp == 0x9 || cp == 0x0A || cp == 0x0D ||                            \
      (cp >= 0x20 && cp <= 0xD7FF) || (cp >= 0xE000 && cp <= 0xFFFD) ||   \
      (cp >= 0x10000 && cp <= 0x10FFFF)))

#define XML_IS_CHAR_RESTRICTED(cp, context) ((context->parser->xml_version == AXING_XML_VERSION_1_1) && ((cp >= 0x1 && cp <= 0x8) || (cp >= 0xB && cp <= 0xC) || (cp >= 0xE && cp <= 0x1F) || (cp >= 0x7F && cp <= 0x84) || (cp >= 0x86 && cp <= 0x9F)))

#define IS_1_1(context) (context->state != PARSER_STATE_START && context->parser->xml_version == AXING_XML_VERSION_1_1)

/* FIXME: put this in axing-utf8.h */
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

#define CONTEXT_GET_NAME(context, namevar) {                            \
    char *start = context->linecur;                                     \
    gsize bytes = axing_utf8_bytes_name_start (context->linecur);       \
    if (bytes == 0)                                                     \
        ERROR_SYNTAX_MSG (context, "Expected name start character");    \
    context->linecur += bytes;                                          \
    context->colnum++;                                                  \
    while (1) {                                                         \
        bytes = axing_utf8_bytes_name (context->linecur);               \
        if (bytes == 0)                                                 \
            break;                                                      \
        context->linecur += bytes;                                      \
        context->colnum++;                                              \
    }                                                                   \
    namevar = g_strndup (start, context->linecur - start);              \
    }


/* AXING_XML_PARSER_ERROR_SYNTAX
   We got the wrong kind of special character. Generally, only use this for errors
   we would detect in tokenization, if we had a separate tokenization step. Try to
   use ERROR_SYNTAX_MSG to provide better error messages.
*/
#define ERROR_SYNTAX(context) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_SYNTAX, "%s:%i:%i: Syntax error.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }
#define ERROR_SYNTAX_MSG(context, msg) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_SYNTAX, "%s:%i:%i: Syntax error: %s.", context->showname ? context->showname : context->basename, context->linenum, context->colnum, msg); goto error; }

/* AXING_XML_PARSER_ERROR_ENTITY
   There was an error parsing or dereferencing an entity reference. This is not
   for errors in parsing the actual entity after dereferencing. Try to use
   ERROR_ENTITY_MSG to provide better error messages.
*/
/* REFACTOR
   Make the error message always reference entity references. Be consistent.
   Maybe rename the error code to ENTITYREF?
*/
#define ERROR_ENTITY(context) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_ENTITY, "%s:%i:%i: Entity error.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }
#define ERROR_ENTITY_MSG(context, msg) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_ENTITY, "%s:%i:%i: Entity error: %s.", context->showname ? context->showname : context->basename, context->linenum, context->colnum, msg); goto error; }

/* AXING_XML_PARSER_ERROR_CHARSET
   Something went wrong with detecting the charset. ERROR_BOM_ENCODING is used
   specifically when the encoding from the BOM doesn't match the declaration.
*/
#define ERROR_BOM_ENCODING(context, bomenc, encoding) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_CHARSET, "%s:%i:%i: Detected encoding \"%s\" from BOM, but got \"%s\" from declaration.", context->showname ? context->showname : context->basename, context->linenum, context->colnum, bomenc, encoding); goto error; }

/* AXING_XML_PARSER_ERROR_DUPATTR
   Two attritbutes on the same element have the same qname. If they have the
   same expanded name, use AXING_XML_PARSER_ERROR_NS_DUPATTR instead.
*/
#define ERROR_DUPATTR(context, attr) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_DUPATTR, "%s:%i:%i: Duplicate attribute \"%s\".", context->showname ? context->showname : context->basename, attr->linenum, attr->colnum, attr->qname); goto error; }

/* AXING_XML_PARSER_ERROR_UNBALANCED
   Something is unbalanced in the tree structure. This could be an incorrect
   end tag, missing end tags at the end of a resource, or extra content at
   the end of a resource.
 */
#define ERROR_MISSINGEND(context, qname) { context->parser->error = g_error_new (AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_UNBALANCED, "%s:%i:%i: Missing end tag for \"%s\".", context->showname ? context->showname : context->basename, context->linenum, context->colnum, qname); goto error; }
#define ERROR_EXTRACONTENT(context) { context->parser->error = g_error_new (AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_UNBALANCED, "%s:%i:%i: Extra content at end of resource.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }
#define ERROR_WRONGEND(context, qname) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_UNBALANCED, "%s:%i:%i: Incorrect end tag \"%s\".", context->showname ? context->showname : context->basename, context->linenum, context->colnum, qname); goto error; }





/*
  REFACTOR comments
    AXING_XML_PARSER_ERROR_NS_QNAME,
    AXING_XML_PARSER_ERROR_NS_NOTFOUND,
    AXING_XML_PARSER_ERROR_NS_DUPATTR,
    AXING_XML_PARSER_ERROR_NS_INVALID,

*/

/* REFACTOR comment */
#define ERROR_NS_QNAME(context) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_QNAME, "%s:%i:%i: Could not parse qname \"%s\".", context->showname ? context->showname : context->basename, context->parser->event->linenum, context->parser->event->colnum, context->parser->event->qname); goto error; }

/* REFACTOR comment */
#define ERROR_NS_QNAME_ATTR(context, data) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_QNAME, "%s:%i:%i: Could not parse qname \"%s\".", context->showname ? context->showname : context->basename, data->linenum, data->colnum, data->qname); goto error; }

/* REFACTOR comment */
#define ERROR_NS_NOTFOUND(context) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_NOTFOUND, "%s:%i:%i: Could not find namespace for prefix \"%s\".", context->showname ? context->showname : context->basename, context->parser->event->linenum, context->parser->event->colnum, context->parser->event->prefix); goto error; }

/* REFACTOR comment */
#define ERROR_NS_NOTFOUND_ATTR(context, data) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_NOTFOUND, "%s:%i:%i: Could not find namespace for prefix \"%s\".", context->showname ? context->showname : context->basename, data->linenum, data->colnum, data->prefix); goto error; }

/* REFACTOR comment */
#define ERROR_NS_DUPATTR(context, attr) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_DUPATTR, "%s:%i:%i: Duplicate expanded name for attribute \"%s\".", context->showname ? context->showname : context->basename, attr->linenum, attr->colnum, attr->qname); goto error; }

/* REFACTOR comment */
#define ERROR_NS_INVALID(context, prefix) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_NS_INVALID, "%s:%i:%i: Invalid namespace for prefix \"%s\".", context->showname ? context->showname : context->basename, context->parser->event->xmlns->linenum, context->parser->event->xmlns->colnum, prefix); goto error; }

/* AXING_XML_PARSER_ERROR_OTHER
   Never use this error code or the ERROR_FIXME macro, except as a FIXME
   that you actually intend to fix.
 */
#define ERROR_FIXME(context) { context->parser->error = g_error_new(AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_OTHER, "%s:%i:%i: Unsupported feature.", context->showname ? context->showname : context->basename, context->linenum, context->colnum); goto error; }


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

#define CONTEXT_EAT_SPACES(context)                             \
    EAT_SPACES(context->linecur, context->linecur, -1, context)

#define CONTEXT_ADVANCE_CHAR(context, cur, check) {                     \
    gsize bytes;                                                        \
    AxingXmlVersion ver = IS_1_1(context) ?                             \
        AXING_XML_VERSION_1_1 : AXING_XML_VERSION_1_0;                  \
    bytes = axing_utf8_bytes_newline (cur, ver);                        \
    if (bytes) {                                                        \
        if (cur != context->linecur)                                    \
            g_string_append_len(context->parser->cur_text,              \
                                context->linecur,                       \
                                cur - context->linecur);                \
        g_string_append_c (context->parser->cur_text, 0x0A);            \
        cur += bytes;                                                   \
        context->linenum++; context->colnum = 1;                        \
        context->linecur = cur;                                         \
    }                                                                   \
    else {                                                              \
        bytes = axing_utf8_bytes_character (cur, ver);                  \
        if (bytes) {                                                    \
            cur += bytes;                                               \
            context->colnum++;                                          \
        }                                                               \
        else {                                                          \
            ERROR_SYNTAX_MSG (context, "Invalid character");            \
        }                                                               \
    }                                                                   \
    }

#define CHECK_BUFFER(c, num, buf, bufsize, context)                     \
    if (c - buf + num > bufsize) {                                      \
        context->parser->error =                                        \
            g_error_new(AXING_XML_PARSER_ERROR,                         \
                        AXING_XML_PARSER_ERROR_BUFFER,                  \
                        "Insufficient buffer for XML declaration\n");   \
        goto error;                                                     \
    }

#define READ_TO_QUOTE(c, buf, bufsize, context, quot)                   \
    EAT_SPACES (c, buf, bufsize, context);                              \
    CHECK_BUFFER (c, 1, buf, bufsize, context);                         \
    if (c[0] != '=') { ERROR_SYNTAX (context); }                        \
    c += 1; context->colnum += 1;                                       \
    EAT_SPACES (c, buf, bufsize, context);                              \
    CHECK_BUFFER (c, 1, buf, bufsize, context);                         \
    if (c[0] != '"' && c[0] != '\'') { ERROR_SYNTAX (context); }        \
    quot = c[0];                                                        \
    c += 1; context->colnum += 1;


#ifdef REFACTOR

static void
context_resource_read_cb (AxingResource *resource,
                          GAsyncResult  *result,
                          Context       *context)
{
    context->srcstream = G_INPUT_STREAM (g_object_ref (axing_resource_read_finish (resource, result,
                                                                     &(context->parser->error))))
    if (context->parser->error) {
        context_complete (context);
        return;
    }
    context_start_async (context);
}

static void
context_start_async (Context *context)
{
    context->datastream = g_data_input_stream_new (context->srcstream);

    g_buffered_input_stream_fill_async (G_BUFFERED_INPUT_STREAM (context->datastream),
                                        1024,
                                        G_PRIORITY_DEFAULT,
                                        context->parser->cancellable,
                                        (GAsyncReadyCallback) context_start_cb,
                                        context);
}

static void
context_start_cb (GBufferedInputStream *stream,
                  GAsyncResult         *res,
                  Context              *context)
{
    g_buffered_input_stream_fill_finish (stream, res, NULL);
    if (context->state == PARSER_STATE_START || context->state == PARSER_STATE_TEXTDECL) {
        if (!context->bom_checked) {
            gboolean reencoded;
            context->bom_checked = TRUE;
            reencoded = context_parse_bom (context);
            if (context->parser->error) {
                context_complete (context);
                return;
            }
            if (reencoded) {
                g_buffered_input_stream_fill_async (G_BUFFERED_INPUT_STREAM (context->datastream),
                                                    1024,
                                                    G_PRIORITY_DEFAULT,
                                                    context->parser->cancellable,
                                                    (GAsyncReadyCallback) context_start_cb,
                                                    context);
                return;
            }
        }

        context_parse_xml_decl (context);
        if (context->parser->error) {
            context_complete (context);
            return;
        }

        if (context->state == PARSER_STATE_TEXTDECL)
            context->state = context->init_state;
    }
    g_data_input_stream_set_newline_type (context->datastream,
                                          G_DATA_STREAM_NEWLINE_TYPE_LF);
/*
    g_data_input_stream_read_upto_async (context->datastream,
                                         " ", -1,
                                         G_PRIORITY_DEFAULT,
                                         context->parser->cancellable,
                                         (GAsyncReadyCallback) context_read_data_cb,
                                         context);
*/
}

static void
context_read_data_cb (GDataInputStream *stream,
                      GAsyncResult     *res,
                      Context          *context)
{
    char *line;
    if (context->parser->error) {
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
/*
        line = g_data_input_stream_read_upto_finish (stream, res, NULL, &(context->parser->error));
*/
        if (line == NULL && context->parser->error == NULL) {
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
        if (line == NULL || context->parser->error) {
            context_complete (context);
            return;
        }
        context_parse_data (context, line);
        g_free (line);
        if (context->parser->error) {
            context_complete (context);
            return;
        }
    bug692101:
        if (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (context->datastream)) > 0) {
            char eol[2] = {0, 0};
/*
            eol[0] = g_data_input_stream_read_byte (context->datastream,
                                                    context->parser->cancellable,
                                                    &(context->parser->error));
*/
            if (context->parser->error) {
                context_complete (context);
                return;
            }
            context_parse_data (context, eol);
            if (context->parser->error) {
                context_complete (context);
                return;
            }
        }
    }
    if (context->pause_line == NULL)
/*
        g_data_input_stream_read_upto_async (context->datastream,
                                             " ", -1,
                                             G_PRIORITY_DEFAULT,
                                             context->parser->cancellable,
                                             (GAsyncReadyCallback) context_read_data_cb,
                                             context);
*/
}

#endif /* REFACTOR */


static void
context_check_end (Context *context)
{
    AXING_DEBUG ("context_check_end\n");

    if (context->state != PARSER_STATE_TEXT) {
        if (context->state != context->init_state &&
            !(context->state == PARSER_STATE_EPILOG && context->init_state == PARSER_STATE_PROLOG)) {
            ERROR_SYNTAX_MSG (context, "Incomplete data at end of parser context"); // test: entities19
        }
    }

    if (context->state == PARSER_STATE_DOCTYPE) {
        if (context->doctype_state != DOCTYPE_STATE_INT) {
            ERROR_SYNTAX_MSG (context, "Incomplete data at end of parser context"); // test: parameters04
        }
    }

    if (context->parser->event != NULL) {
        Event *event = context->parser->event;
        if (event->qname == NULL)
            event = event->parent;
        if (event && event->context == context)
            ERROR_MISSINGEND(context, event->qname);
    }

 error:
    return;
}


static void
context_set_encoding (Context *context, const char *encoding)
{
    GConverter *converter;
    GInputStream *cstream;
    AXING_DEBUG ("context_set_encoding: %s\n", encoding);
    converter = (GConverter *) g_charset_converter_new ("UTF-8", encoding, NULL);
    if (converter == NULL) {
/* REFACTOR why isn't there an ERROR macro for this? because there's no goto? */
        context->parser->error = g_error_new (AXING_XML_PARSER_ERROR, AXING_XML_PARSER_ERROR_CHARSET,
                                                    "Unsupported character encoding %s\n", encoding);
        return;
    }
    cstream = g_converter_input_stream_new (G_INPUT_STREAM (context->datastream), converter);
    g_object_unref (converter);
    g_object_unref (context->datastream);
    context->datastream = g_data_input_stream_new (cstream);
    g_object_unref (cstream);
}


/* returns whether context->datastream changed, requiring the buffer to be refilled */
static gboolean
context_parse_bom (Context *context)
{
    guchar *buf;
    gsize bufsize;
    AXING_DEBUG ("context_parse_bom\n");

    g_return_val_if_fail (context->state == PARSER_STATE_START || context->state == PARSER_STATE_TEXTDECL, FALSE);

    buf = (guchar *) g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (context->datastream), &bufsize);

    if (bufsize >= 4 && EQ4 (buf, 0x00, 0x00, 0xFE, 0xFF)) {
        context->bom_encoding = BOM_ENCODING_UCS4_BE;
    }
    else if (bufsize >= 4 && EQ4 (buf, 0xFF, 0xFE, 0x00, 0x00)) {
        context->bom_encoding = BOM_ENCODING_UCS4_LE;
    }
    else if (bufsize >= 2 && EQ2 (buf, 0xFE, 0xFF)) {
        context->bom_encoding = BOM_ENCODING_UTF16_BE;
    }
    else if (bufsize >= 2 && EQ2 (buf, 0xFF, 0xFE)) {
        context->bom_encoding = BOM_ENCODING_UTF16_LE;
    }
    else if (bufsize >= 3 && EQ3 (buf, 0xEF, 0xBB, 0xBF)) {
        context->bom_encoding = BOM_ENCODING_UTF8;
    }
    else if (bufsize >= 4 && EQ4 (buf, 0x00, 0x00, 0x00, 0x3C)) {
        context->bom_encoding = BOM_ENCODING_4BYTE_BE;
    }
    else if (bufsize >= 4 && EQ4 (buf, 0x3C, 0x00, 0x00, 0x00)) {
        context->bom_encoding = BOM_ENCODING_4BYTE_LE;
    }
    else if (bufsize >= 4 && EQ4 (buf, 0x00, 0x3C, 0x00, 0x3F)) {
        context->bom_encoding = BOM_ENCODING_2BYTE_BE;
    }
    else if (bufsize >= 4 && EQ4 (buf, 0x3C, 0x00, 0x3F, 0x00)) {
        context->bom_encoding = BOM_ENCODING_2BYTE_LE;
    }

    switch (context->bom_encoding) {
    case BOM_ENCODING_UCS4_BE:
    case BOM_ENCODING_4BYTE_BE:
        context_set_encoding (context, "UCS-4BE");
        return TRUE;
    case BOM_ENCODING_UCS4_LE:
    case BOM_ENCODING_4BYTE_LE:
        context_set_encoding (context, "UCS-4LE");
        return TRUE;
    case BOM_ENCODING_UTF16_BE:
    case BOM_ENCODING_2BYTE_BE:
        context_set_encoding (context, "UTF-16BE");
        return TRUE;
    case BOM_ENCODING_UTF16_LE:
    case BOM_ENCODING_2BYTE_LE:
        context_set_encoding (context, "UTF-16LE");
        return TRUE;
    case BOM_ENCODING_UTF8:
    case BOM_ENCODING_NONE:
        return FALSE;
    }
}


static void
context_parse_xml_decl (Context *context)
{
    int i;
    gsize bufsize;
    guchar *buf, *c;
    char quot;
    char *encoding = NULL;
    AXING_DEBUG ("context_parse_xml_decl\n");

    g_return_if_fail (context->state == PARSER_STATE_START || context->state == PARSER_STATE_TEXTDECL);

    c = buf = (guchar *) g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (context->datastream), &bufsize);

    if (bufsize >= 3 && c[0] == 0xEF && c[1] == 0xBB && c[2] == 0xBF)
        c = c + 3;

    if (!(bufsize >= 6 + (c - buf) && !strncmp(c, "<?xml", 5) && XML_IS_SPACE(c + 5, context) )) {
        if (c != buf)
            g_input_stream_skip (G_INPUT_STREAM (context->datastream), c - buf, NULL, NULL);
        return;
    }
    
    c += 6;
    context->colnum += 6;
    EAT_SPACES (c, buf, bufsize, context);
    CHECK_BUFFER (c, 8, buf, bufsize, context);
    if (c[0] == 'v') {
        if (!(!strncmp(c, "version", 7) && (c[7] == '=' || XML_IS_SPACE(c + 7, context)) )) {
            ERROR_SYNTAX (context);
        }
        c += 7; context->colnum += 7;

        READ_TO_QUOTE (c, buf, bufsize, context, quot);

        CHECK_BUFFER (c, 4, buf, bufsize, context);
        if (EQ3 (c, '1', '.', '0')) {
            context->parser->xml_version = AXING_XML_VERSION_1_0;
        }
        else if (EQ3 (c, '1', '.', '1')) {
            context->parser->xml_version = AXING_XML_VERSION_1_1;
        }
        else {
            ERROR_SYNTAX (context);
        }
        c += 4; context->colnum += 4;
    }
    else if (context->state != PARSER_STATE_TEXTDECL) {
        ERROR_SYNTAX (context);
    }

    EAT_SPACES (c, buf, bufsize, context);
    CHECK_BUFFER (c, 1, buf, bufsize, context);
    if (c[0] == 'e') {
        GString *enc;
        CHECK_BUFFER (c, 9, buf, bufsize, context);
        if (!(!strncmp(c, "encoding", 8) && (c[8] == '=' || XML_IS_SPACE(c + 8, context)) )) {
            ERROR_SYNTAX (context);
        }
        c += 8; context->colnum += 8;

        READ_TO_QUOTE (c, buf, bufsize, context, quot);

        CHECK_BUFFER (c, 1, buf, bufsize, context);
        if (!((c[0] >= 'A' && c[0] <= 'Z') || (c[0] >= 'a' && c[0] <= 'z'))) {
            ERROR_SYNTAX (context);
        }

        enc = g_string_sized_new (16);
        while ((c[0] >= 'A' && c[0] <= 'Z') || (c[0] >= 'a' && c[0] <= 'z') ||
               (c[0] >= '0' && c[0] <= '9') || c[0] == '-' || c[0] == '.' || c[0] == '_') {
            CHECK_BUFFER (c, 2, buf, bufsize, context);
            g_string_append_c (enc, c[0]);
            c += 1; context->colnum += 1;
        }
        if (c[0] != quot) {
            ERROR_SYNTAX (context);
        }
        c += 1; context->colnum += 1;

        encoding = g_string_free (enc, FALSE);
    }
    else if (context->state == PARSER_STATE_TEXTDECL) {
        ERROR_SYNTAX_MSG (context, "Expected encoding in text declaration"); // test: entities35
    }

    EAT_SPACES (c, buf, bufsize, context);
    CHECK_BUFFER (c, 1, buf, bufsize, context);
    if (c[0] == 's') {
        if (context->state == PARSER_STATE_TEXTDECL) {
            ERROR_SYNTAX_MSG (context, "Expected end of text declaration");
        }

        CHECK_BUFFER (c, 11, buf, bufsize, context);
        if (!(!strncmp(c, "standalone", 10) && (c[10] == '=' || XML_IS_SPACE(c + 10, context)) )) {
            ERROR_SYNTAX (context);
        }
        c += 10; context->colnum += 10;

        READ_TO_QUOTE (c, buf, bufsize, context, quot);

        CHECK_BUFFER (c, 3, buf, bufsize, context);
        if (EQ3 (c, 'y', 'e', 's')) {
            c += 3; context->colnum += 3;
        }
        else if (EQ2 (c, 'n', 'o')) {
            c += 2; context->colnum += 2;
        }
        else {
            ERROR_SYNTAX (context);
        }
        if (c[0] != quot) {
            ERROR_SYNTAX (context);
        }
        c += 1; context->colnum += 1;
    }

    EAT_SPACES (c, buf, bufsize, context);
    CHECK_BUFFER (c, 2, buf, bufsize, context);
    if (!EQ2 (c, '?', '>')) {
        ERROR_SYNTAX (context);
    }
    c += 2; context->colnum += 2;

    g_input_stream_skip (G_INPUT_STREAM (context->datastream), c - buf, NULL, NULL);

    if (encoding != NULL) {
        switch (context->bom_encoding) {
        case BOM_ENCODING_UCS4_BE:
            if (g_ascii_strcasecmp ("ucs-4be", encoding) &&
                g_ascii_strcasecmp ("ucs4be", encoding) &&
                g_ascii_strcasecmp ("ucs-4", encoding) &&
                g_ascii_strcasecmp ("ucs4", encoding))
                ERROR_BOM_ENCODING(context, "UCS-4BE", encoding);
            break;
        case BOM_ENCODING_UCS4_LE:
            if (g_ascii_strcasecmp ("ucs-4le", encoding) &&
                g_ascii_strcasecmp ("ucs4le", encoding) &&
                g_ascii_strcasecmp ("ucs-4", encoding) &&
                g_ascii_strcasecmp ("ucs4", encoding))
                ERROR_BOM_ENCODING(context, "UCS-4LE", encoding);
            break;
        case BOM_ENCODING_4BYTE_BE:
            if (g_ascii_strcasecmp ("ucs-4be", encoding) &&
                g_ascii_strcasecmp ("ucs4be", encoding) &&
                g_ascii_strcasecmp ("ucs-4", encoding) &&
                g_ascii_strcasecmp ("ucs4", encoding))
                ERROR_FIXME(context);
            break;
        case BOM_ENCODING_4BYTE_LE:
            if (g_ascii_strcasecmp ("ucs-4le", encoding) &&
                g_ascii_strcasecmp ("ucs4le", encoding) &&
                g_ascii_strcasecmp ("ucs-4", encoding) &&
                g_ascii_strcasecmp ("ucs4", encoding))
                ERROR_FIXME(context);
            break;
        case BOM_ENCODING_UTF16_BE:
            if (g_ascii_strcasecmp ("utf-16be", encoding) &&
                g_ascii_strcasecmp ("utf16be", encoding) &&
                g_ascii_strcasecmp ("utf-16", encoding) &&
                g_ascii_strcasecmp ("utf16", encoding))
                ERROR_BOM_ENCODING(context, "UTF-16BE", encoding);
            break;
        case BOM_ENCODING_UTF16_LE:
            if (g_ascii_strcasecmp ("utf-16le", encoding) &&
                g_ascii_strcasecmp ("utf16le", encoding) &&
                g_ascii_strcasecmp ("utf-16", encoding) &&
                g_ascii_strcasecmp ("utf16", encoding))
                ERROR_BOM_ENCODING(context, "UTF-16LE", encoding);
            break;
        case BOM_ENCODING_2BYTE_BE:
            if (g_ascii_strcasecmp ("utf-16be", encoding) &&
                g_ascii_strcasecmp ("utf16be", encoding) &&
                g_ascii_strcasecmp ("utf-16", encoding) &&
                g_ascii_strcasecmp ("utf16", encoding))
                ERROR_FIXME(context);
            break;
        case BOM_ENCODING_2BYTE_LE:
            if (g_ascii_strcasecmp ("utf-16le", encoding) &&
                g_ascii_strcasecmp ("utf16le", encoding) &&
                g_ascii_strcasecmp ("utf-16", encoding) &&
                g_ascii_strcasecmp ("utf16", encoding))
                ERROR_FIXME(context);
            break;
        case BOM_ENCODING_UTF8:
            if (g_ascii_strcasecmp ("utf-8", encoding) &&
                g_ascii_strcasecmp ("utf8", encoding))
                ERROR_BOM_ENCODING(context, "UTF-8", encoding);
            break;
        case BOM_ENCODING_NONE:
            context_set_encoding (context, encoding);
            break;
        }
        g_free (encoding);
    }

    if (context->state == PARSER_STATE_TEXTDECL)
        context->state = context->init_state;
    else
        context->state = PARSER_STATE_PROLOG;

 error:
    return;
}


static void
context_parse_line (Context *context)
{
    AXING_DEBUG ("context_parse_line: %s\n", context->linecur);
    /* The parsing functions make these assumptions about the data that gets passed in:
       1) It always has complete UTF-8 characters.
       2) It always terminates somewhere where a space is permissable, e.g. never in
          the middle of an element name.
       3) Mutli-character newline sequences are not split into separate chunks of data.
     */
    while (context->linecur != NULL && context->linecur[0] != '\0') {
        switch (context->state) {
        case PARSER_STATE_TEXTDECL:
            context->state = context->init_state;
            /* no break */
        case PARSER_STATE_START:
        case PARSER_STATE_PROLOG:
        case PARSER_STATE_EPILOG:
        case PARSER_STATE_TEXT:
            if (context->state != PARSER_STATE_TEXT) {
                CONTEXT_EAT_SPACES (context);
            }
            if (context->linecur[0] == '<') {
                if (context->state == PARSER_STATE_TEXT) {
                    if (context->parser->cur_text->len != 0) {
                        context->parser->event_type = AXING_NODE_TYPE_CONTENT;
                        return;
                    }
                }
                switch (context->linecur[1]) {
                case '!':
                    if (context->state == PARSER_STATE_TEXT &&
                        EQ7 (context->linecur + 2, '[', 'C', 'D', 'A', 'T', 'A', '[')){
                        context_parse_cdata (context);
                        if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                            return;
                        break;
                    }
                    else if (context->state != PARSER_STATE_TEXT &&
                             EQ7 (context->linecur + 2, 'D', 'O', 'C', 'T', 'Y', 'P', 'E')) {
                        context_parse_doctype (context);
                        break;
                    }
                    else if (EQ2 (context->linecur + 2, '-', '-')) {
                        context_parse_comment (context);
                        if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                            return;
                    }
                    else {
                        if (context->state == PARSER_STATE_TEXT) {
                            ERROR_SYNTAX_MSG (context, "Expected comment or CDATA"); // test: entities34
                        }
                        else {
                            ERROR_SYNTAX_MSG (context, "Expected comment or DOCTYPE");
                        }
                    }
                    break;
                case '?':
                    context_parse_instruction (context);
                    if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                        return;
                    break;
                case '/':
                    context_parse_end_element (context);
                    if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                        return;
                    break;
                default:
                    if (context->state == PARSER_STATE_EPILOG)
                        ERROR_EXTRACONTENT (context); // test: element10
                    context_parse_start_element (context);
                    if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                        return;
                }
            }
            else if (context->state == PARSER_STATE_EPILOG && context->linecur[0] != '\0') {
                ERROR_EXTRACONTENT (context); // test: element11
            }
            else if (context->state == PARSER_STATE_TEXT) {
                context_parse_text (context);
                /* entities could give us a new context, bubble out */
                if (context != context->parser->context)
                    return;
                if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                    return;
            }
            else if (context->linecur[0] != '\0') {
                ERROR_SYNTAX_MSG (context, "Expected start element"); // test: element16 element17
            }
            break;
        case PARSER_STATE_STELM_BASE:
        case PARSER_STATE_STELM_ATTNAME:
        case PARSER_STATE_STELM_ATTEQ:
        case PARSER_STATE_STELM_ATTVAL:
            context_parse_attrs (context);
            if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                return;
            /* entities could give us a new context, bubble out */
            if (context != context->parser->context)
                return;
            break;
        case PARSER_STATE_ENDELM:
            context_parse_end_element (context);
            if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                return;
            break;
        case PARSER_STATE_CDATA:
            context_parse_cdata (context);
            if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                return;
            break;
        case PARSER_STATE_COMMENT:
            context_parse_comment (context);
            if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                return;
            break;
        case PARSER_STATE_INSTRUCTION:
            context_parse_instruction (context);
            if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                return;
            break;
        case PARSER_STATE_DOCTYPE:
            context_parse_doctype (context);
            if (context->parser->event_type != AXING_NODE_TYPE_NONE)
                return;
            /* parameter entities could give us a new context, bubble out */
            if (context != context->parser->context)
                return;
            break;
        default:
            g_assert_not_reached ();
        }
        if (context->parser->error != NULL) {
            return;
        }
    }

 error:
    return;
}


static void
context_parse_doctype (Context *context)
{
    AXING_DEBUG ("context_parse_doctype: %s\n", context->linecur);
    switch (context->state) {
    case PARSER_STATE_START:
    case PARSER_STATE_PROLOG:
        g_assert (context->parser->doctype == NULL);
        g_assert (EQ9 (context->linecur, '<', '!', 'D', 'O', 'C', 'T', 'Y', 'P', 'E'));
        context->linecur += 9; context->colnum += 9;
        CONTEXT_EAT_SPACES (context);
        context->state = PARSER_STATE_DOCTYPE;
        context->doctype_state = DOCTYPE_STATE_START;
        if (context->linecur[0] == '\0')
            return;
        break;
    case PARSER_STATE_DOCTYPE:
        break;
    default:
        g_assert_not_reached ();
    }

    if (context->doctype_state == DOCTYPE_STATE_START) {
        char *doctype;
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        CONTEXT_GET_NAME (context, doctype);
        if (!(XML_IS_SPACE(context->linecur, context) ||
              context->linecur[0] == '>' || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space or closing angle bracket"); // test: doctype18
        context->parser->doctype = axing_dtd_schema_new ();
        axing_dtd_schema_set_doctype (context->parser->doctype, doctype);
        g_free (doctype);
        context->doctype_state = DOCTYPE_STATE_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_NAME) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '>') {
            context->doctype_state = DOCTYPE_STATE_NULL;
            context->state = PARSER_STATE_PROLOG;
            context->linecur++; context->colnum++;
            return;
        }
        else if (context->linecur[0] == '[') {
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT;
        }
        else if (EQ6 (context->linecur, 'S', 'Y', 'S', 'T', 'E', 'M')) {
            context->linecur += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_SYSTEM;
        }
        else if (EQ6 (context->linecur, 'P', 'U', 'B', 'L', 'I', 'C')) {
            context->linecur += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_PUBLIC;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected PUBLIC, SYSTEM, internal subset, or closing angle bracket") // test: doctype32
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_PUBLIC) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_PUBLIC_VAL;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character");
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_PUBLIC_VAL) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            gsize bytes;
            if (cur[0] == context->quotechar) {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                axing_dtd_schema_set_public_id (context->parser->doctype, context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
                context->doctype_state = DOCTYPE_STATE_SYSTEM;
                cur++; context->colnum++;
                context->linecur = cur;
                if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0'))
                    ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype11
                break;
            }
            /* Not using axing_utf8_bytes_newline because CR and LF are the only newline
               characters allowed in a pubid, even in XML 1.1. */
            if (cur[0] == 0x0D || cur[0] == 0x0A) {
                if (cur != context->linecur)
                    g_string_append_len(context->parser->cur_text, context->linecur, cur - context->linecur);
                g_string_append_c (context->parser->cur_text, 0x0A);
                context->linenum++;
                context->colnum = 1;
                if (cur[0] == 0x0D && cur[1] == 0x0A)
                    cur += 2;
                else
                    cur += 1;
                continue;
            }
            bytes = axing_utf8_bytes_pubid (cur);
            if (bytes == 0)
                ERROR_SYNTAX_MSG (context, "Invalid character in public identifier"); // test: doctype16
            context->colnum++;
            cur += bytes;
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_SYSTEM) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_SYSTEM_VAL;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character");
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_SYSTEM_VAL) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            if (cur[0] == context->quotechar) {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                axing_dtd_schema_set_system_id (context->parser->doctype, context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
                context->doctype_state = DOCTYPE_STATE_EXTID;
                cur++; context->colnum++;
                context->linecur = cur;
                break;
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_EXTID) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        switch (context->linecur[0]) {
        case '\0':
            return;
        case '[':
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT;
            break;
        case '>':
            context->doctype_state = DOCTYPE_STATE_NULL;
            context->state = PARSER_STATE_PROLOG;
            context->linecur++; context->colnum++;
            return;
        default:
            ERROR_SYNTAX_MSG (context, "Expected internal subset or closing angle bracket"); // test: doctype33
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0') {
            return;
        }
        else if (context->linecur[0] == ']') {
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_AFTER_INT;
        }
        else if (context->linecur[0] == '%') {
            context_parse_parameter (context);
            if (context->parser->error) goto error;
            /* parameter entities could give us a new context, bubble out */
            if (context != context->parser->context)
                return;
        }
        else if (EQ9 (context->linecur, '<', '!', 'E', 'L', 'E', 'M', 'E', 'N', 'T')) {
            context_parse_doctype_element (context);
            if (context->parser->error) goto error;
        }
        else if (EQ9 (context->linecur, '<', '!', 'A', 'T', 'T', 'L', 'I', 'S', 'T')) {
            context_parse_doctype_attlist (context);
            if (context->parser->error) goto error;
        }
        else if (EQ8 (context->linecur, '<', '!', 'E', 'N', 'T', 'I', 'T', 'Y')) {
            context_parse_doctype_entity (context);
            if (context->parser->error) goto error;
        }
        else if (EQ10 (context->linecur, '<', '!', 'N', 'O', 'T', 'A', 'T', 'I', 'O', 'N')) {
            context_parse_doctype_notation (context);
            if (context->parser->error) goto error;
        }
        else if (EQ4 (context->linecur, '<', '!', '-', '-')) {
            context_parse_comment (context);
            if (context->parser->error) goto error;
        }
        else if (EQ2 (context->linecur, '<', '?')) {
            context_parse_instruction (context);
            if (context->parser->error) goto error;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected declaration, comment, or processing instruction"); // test: doctype34
        }
        return;
    }

    switch (context->doctype_state) {
    case DOCTYPE_STATE_INT_ELEMENT_START:
    case DOCTYPE_STATE_INT_ELEMENT_NAME:
    case DOCTYPE_STATE_INT_ELEMENT_VALUE:
    case DOCTYPE_STATE_INT_ELEMENT_AFTER:
        context_parse_doctype_element (context);
        break;
    case DOCTYPE_STATE_INT_ATTLIST_START:
    case DOCTYPE_STATE_INT_ATTLIST_NAME:
    case DOCTYPE_STATE_INT_ATTLIST_VALUE:
    case DOCTYPE_STATE_INT_ATTLIST_QUOTE:
    case DOCTYPE_STATE_INT_ATTLIST_AFTER:
        context_parse_doctype_attlist (context);
        break;
    case DOCTYPE_STATE_INT_NOTATION_START:
    case DOCTYPE_STATE_INT_NOTATION_NAME:
    case DOCTYPE_STATE_INT_NOTATION_SYSTEM:
    case DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL:
    case DOCTYPE_STATE_INT_NOTATION_PUBLIC:
    case DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL:
    case DOCTYPE_STATE_INT_NOTATION_PUBLIC_AFTER:
    case DOCTYPE_STATE_INT_NOTATION_AFTER:
        context_parse_doctype_notation (context);
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
        context_parse_doctype_entity (context);
        break;
    default:
        break;
    }

    if (context->doctype_state == DOCTYPE_STATE_AFTER_INT) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] != '>')
            ERROR_SYNTAX_MSG (context,  "Expected closing angle bracket"); // test: doctype06
        context->doctype_state = DOCTYPE_STATE_NULL;
        context->state = PARSER_STATE_PROLOG;
        context->linecur++; context->colnum++;
        return;
    }

 error:
    return;
}


static void
context_parse_doctype_element (Context *context)
{
    AXING_DEBUG ("context_parse_doctype_element: %s\n", context->linecur);
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (EQ9 (context->linecur, '<', '!', 'E', 'L', 'E', 'M', 'E', 'N', 'T'));
        context->linecur += 9; context->colnum += 9;
        if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype19
        context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_START;
        CONTEXT_EAT_SPACES (context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_START) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        CONTEXT_GET_NAME (context, context->cur_qname);
        if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype20
        context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_NAME) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (EQ5 (context->linecur, 'E', 'M', 'P', 'T', 'Y')) {
            g_string_overwrite (context->parser->cur_text, 0, "EMPTY");
            context->linecur += 5; context->colnum += 5;
            context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_AFTER;
        }
        else if (EQ3 (context->linecur, 'A', 'N', 'Y')) {
            g_string_overwrite (context->parser->cur_text, 0, "ANY");
            context->linecur += 3; context->colnum += 3;
            context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_AFTER;
        }
        else if (context->linecur[0] == '(') {
            g_string_overwrite (context->parser->cur_text, 0, "(");
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_VALUE;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected EMPTY, ANY, or left parenthesis");
        }
    }

    /* not checking the internal syntax here; valid chars only */
    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_VALUE) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            gunichar cp;
            if (cur[0] == '>') {
                context->doctype_state = DOCTYPE_STATE_INT_ELEMENT_AFTER;
                break;
            }
            cp = g_utf8_get_char (cur);
            if (!(XML_IS_NAME_CHAR (cp) || XML_IS_SPACE (cur, context) ||
                  cur[0] == '(' || cur[0] == ')' || cur[0] == '|' ||
                  cur[0] == ',' || cur[0] == '+' || cur[0] == '*' ||
                  cur[0] == '?' || cur[0] == '#' )) {
                ERROR_SYNTAX_MSG (context, "Expected valid element production character"); // test: doctype35
            }
            /* FIXME: we can replace the above character checks with something
               that gives us bytes, so we don't have to get bytes again with
               CONTEXT_ADVANCE_CHAR.
             */
            CONTEXT_ADVANCE_CHAR (context, cur, FALSE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ELEMENT_AFTER) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] != '>')
            ERROR_SYNTAX_MSG (context, "Expected closing angle bracket");
        axing_dtd_schema_add_element (context->parser->doctype,
                                      context->cur_qname,
                                      context->parser->cur_text->str,
                                      &(context->parser->error));
        g_string_truncate (context->parser->cur_text, 0);
        g_free (context->cur_qname);
        context->cur_qname = NULL;
        context->linecur++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }
 error:
    return;
}


static void
context_parse_doctype_attlist (Context *context)
{
    AXING_DEBUG ("context_parse_doctype_attlist: %s\n", context->linecur);
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (EQ9 (context->linecur, '<', '!', 'A', 'T', 'T', 'L', 'I', 'S', 'T'));
        context->linecur += 9; context->colnum += 9;
        if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype21
        context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_START;
        CONTEXT_EAT_SPACES (context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_START) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        CONTEXT_GET_NAME (context, context->cur_qname);
        if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype22
        context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_NAME) {
        gunichar cp;
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        cp = g_utf8_get_char (context->linecur);
        if (XML_IS_NAME_START_CHAR (cp)) {
            g_string_truncate (context->parser->cur_text, 0);
            context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_VALUE;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected name start character");
        }
    }

    /* not checking the internal syntax here; valid chars only */
    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_VALUE) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            gunichar cp;
            if (cur[0] == '"') {
                cur++; context->colnum++;
                context->linecur = cur;
                context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_QUOTE;
                break;
            }
            if (cur[0] == '>') {
                context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_AFTER;
                break;
            }
            cp = g_utf8_get_char (cur);
            if (!(XML_IS_NAME_CHAR (cp) || XML_IS_SPACE (cur, context) ||
                  cur[0] == '#' || cur[0] == '|' || cur[0] == '(' || cur[0] == ')' )) {
                ERROR_SYNTAX_MSG (context, "Expected valid attribute value production character"); // test: doctype36
            }
            /* FIXME: we can replace the above character checks with something
               that gives us bytes, so we don't have to get bytes again with
               CONTEXT_ADVANCE_CHAR.
             */
            CONTEXT_ADVANCE_CHAR (context, cur, FALSE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_QUOTE) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            if (cur[0] == '"') {
                cur++; context->colnum++;
                context->linecur = cur;
                context->doctype_state = DOCTYPE_STATE_INT_ATTLIST_VALUE;
                break;
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ATTLIST_AFTER) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] != '>')
            /* The way the code above is structured, we should never hit this error.
               I'm leaving it in as an extra check, because code changes over time.
               But I can't create a test case for it. */
            ERROR_SYNTAX_MSG (context, "This error is a redundant check, and you should never see it");
        axing_dtd_schema_add_attlist (context->parser->doctype,
                                      context->cur_qname,
                                      context->parser->cur_text->str,
                                      &(context->parser->error));
        g_string_truncate (context->parser->cur_text, 0);
        g_free (context->cur_qname);
        context->cur_qname = NULL;
        context->linecur++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }

 error:
    return;
}


static void
context_parse_doctype_notation (Context *context) {
    AXING_DEBUG ("context_parse_doctype_notation: %s\n", context->linecur);
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (EQ10 (context->linecur, '<', '!', 'N', 'O', 'T', 'A', 'T', 'I', 'O', 'N'));
        context->linecur += 10; context->colnum += 10;
        if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype23
        context->doctype_state = DOCTYPE_STATE_INT_NOTATION_START;
        CONTEXT_EAT_SPACES (context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_START) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        CONTEXT_GET_NAME (context, context->cur_qname);
        if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype24
        context->doctype_state = DOCTYPE_STATE_INT_NOTATION_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_NAME) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (EQ6 (context->linecur, 'S', 'Y', 'S', 'T', 'E', 'M')) {
            context->linecur += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_SYSTEM;
        }
        else if (EQ6 (context->linecur, 'P', 'U', 'B', 'L', 'I', 'C')) {
            context->linecur += 6; context->colnum += 6;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_PUBLIC;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected SYSTEM or PUBLIC"); // test: doctype25
        }
        if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: doctype26
        CONTEXT_EAT_SPACES (context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_SYSTEM) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '"' || context->linecur[0] == '\'') {
            context->quotechar = context->linecur[0];
            context->linecur++; context->colnum++;
            g_string_truncate (context->parser->cur_text, 0);
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character"); // test: doctype27
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            if (cur[0] == context->quotechar) {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                context->decl_system = g_strdup (context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
                context->doctype_state = DOCTYPE_STATE_INT_NOTATION_AFTER;
                cur++; context->colnum++;
                context->linecur = cur;
                break;
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_PUBLIC) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            g_string_truncate (context->parser->cur_text, 0);
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character"); // test: doctype37
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_PUBLIC_VAL) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            gsize bytes;
            if (cur[0] == context->quotechar) {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                context->decl_public = g_strdup (context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
                context->doctype_state = DOCTYPE_STATE_INT_NOTATION_PUBLIC_AFTER;
                cur++; context->colnum++;
                context->linecur = cur;
                if (!(XML_IS_SPACE (cur, context) || cur[0] == '\0' || cur[0] == '>'))
                    ERROR_SYNTAX_MSG (context, "Expected space or closing angle bracket"); // test: doctype12
                break;
            }
            bytes = axing_utf8_bytes_pubid (cur);
            if (bytes == 0)
                ERROR_SYNTAX_MSG (context, "Expected pubid character or quote character"); // test: doctype28
            context->colnum++;
            cur += bytes;
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_PUBLIC_AFTER) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            g_string_truncate (context->parser->cur_text, 0);
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_SYSTEM_VAL;
        }
        else if (context->linecur[0] == '>') {
            context->doctype_state = DOCTYPE_STATE_INT_NOTATION_AFTER;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character or closing angle bracket"); // test: doctype29
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_NOTATION_AFTER) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] != '>')
            ERROR_SYNTAX_MSG (context, "Expected closing angle bracket"); // test: doctype30
        axing_dtd_schema_add_notation (context->parser->doctype,
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
        context->linecur++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }

 error:
    return;
}


static void
context_parse_doctype_entity (Context *context) {
    AXING_DEBUG ("context_parse_doctype_entity: %s\n", context->linecur);
    if (context->doctype_state == DOCTYPE_STATE_INT) {
        g_assert (EQ8 (context->linecur, '<', '!', 'E', 'N', 'T', 'I', 'T', 'Y'));
        context->linecur += 8; context->colnum += 8;
        if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: entities40
        context->doctype_state = DOCTYPE_STATE_INT_ENTITY_START;
        context->decl_pedef = FALSE;
        CONTEXT_EAT_SPACES (context);
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_START) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '%') {
            /* if already true, we've seen a % in this decl */
            if (context->decl_pedef)
                ERROR_SYNTAX_MSG (context, "Expected name start character"); // test: parameters05
            context->decl_pedef = TRUE;
            context->linecur++; context->colnum++;
            if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
                ERROR_SYNTAX_MSG (context, "Expected space"); // test: parameters06
            CONTEXT_EAT_SPACES (context);
            if (context->linecur[0] == '\0')
                return;
        }
        CONTEXT_GET_NAME (context, context->cur_qname);
        if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
            ERROR_SYNTAX_MSG (context, "Expected space"); // test: parameters07
        context->doctype_state = DOCTYPE_STATE_INT_ENTITY_NAME;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_NAME) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (EQ6 (context->linecur, 'S', 'Y', 'S', 'T', 'E', 'M')) {
            context->linecur += 6; context->colnum += 6;
            if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
                ERROR_SYNTAX_MSG (context, "Expected space"); // test: entities41
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_SYSTEM;
            CONTEXT_EAT_SPACES (context);
        }
        else if (EQ6 (context->linecur, 'P', 'U', 'B', 'L', 'I', 'C')) {
            context->linecur += 6; context->colnum += 6;
            if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
                ERROR_SYNTAX_MSG (context, "Expected space"); // test: entities42
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_PUBLIC;
            CONTEXT_EAT_SPACES (context);
        }
        else if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            g_string_truncate (context->parser->cur_text, 0);
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_VALUE;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected PUBLIC, SYSTEM, or quote character"); // test: entities43
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_VALUE) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            if (cur[0] == context->quotechar) {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
                cur++; context->colnum++;
                context->linecur = cur;
                break;
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_PUBLIC) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            g_string_truncate (context->parser->cur_text, 0);
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_PUBLIC_VAL;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character"); // test: entities44
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_PUBLIC_VAL) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            gsize bytes;
            if (cur[0] == context->quotechar) {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                context->decl_public = g_strdup (context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_SYSTEM;
                cur++; context->colnum++;
                context->linecur = cur;
                if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0'))
                    ERROR_SYNTAX_MSG (context, "Expected space"); // test: entities45
                break;
            }
            bytes = axing_utf8_bytes_pubid (cur);
            if (bytes == 0)
                ERROR_SYNTAX_MSG (context, "Expected pubid character or quote character"); // test: entities46
            context->colnum++;
            cur += bytes;
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_SYSTEM) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            g_string_truncate (context->parser->cur_text, 0);
            context->linecur++; context->colnum++;
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_SYSTEM_VAL;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character"); // test: entities47
        }
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_SYSTEM_VAL) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            if (cur[0] == context->quotechar) {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                context->decl_system = g_strdup (context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
                context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
                cur++; context->colnum++;
                context->linecur = cur;
                break;
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_NDATA) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        CONTEXT_GET_NAME (context, context->decl_ndata);
        if (!(XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0' || context->linecur[0] == '>'))
            ERROR_SYNTAX_MSG (context, "Expected space or closing angle bracket"); // test: doctype31
        context->doctype_state = DOCTYPE_STATE_INT_ENTITY_AFTER;
    }

    if (context->doctype_state == DOCTYPE_STATE_INT_ENTITY_AFTER) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
        if (EQ5 (context->linecur, 'N', 'D', 'A', 'T', 'A')) {
            if (context->parser->cur_text->len != 0 || /* not after an EntityValue */
                context->decl_system == NULL        || /* only after PUBLIC or SYSTEM */
                context->decl_pedef == TRUE         || /* no NDATA on param entities */
                context->decl_ndata != NULL         )  /* only one NDATA */
                ERROR_SYNTAX_MSG (context, "Did not expect NDATA here"); // test: entities48;
            context->linecur += 5; context->colnum += 5;
            if (!(XML_IS_SPACE (context->linecur, context) || context->linecur[0] == '\0'))
                ERROR_SYNTAX_MSG (context, "Expected space"); // test: entities49
            context->doctype_state = DOCTYPE_STATE_INT_ENTITY_NDATA;
            return;
        }
        if (context->linecur[0] != '>')
            ERROR_SYNTAX_MSG (context, "Expected closing angle bracket"); // test: entities50
        if (context->decl_pedef) {
            if (context->parser->cur_text->len != 0) {
                axing_dtd_schema_add_parameter (context->parser->doctype,
                                                context->cur_qname,
                                                context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
            }
            else {
                axing_dtd_schema_add_external_parameter (context->parser->doctype,
                                                         context->cur_qname,
                                                         context->decl_public,
                                                         context->decl_system);
            }
        }
        else {
            if (context->parser->cur_text->len != 0) {
                axing_dtd_schema_add_entity (context->parser->doctype,
                                             context->cur_qname,
                                             context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
            }
            else if (context->decl_ndata) {
                axing_dtd_schema_add_unparsed_entity (context->parser->doctype,
                                                      context->cur_qname,
                                                      context->decl_public,
                                                      context->decl_system,
                                                      context->decl_ndata);
            }
            else {
                axing_dtd_schema_add_external_entity (context->parser->doctype,
                                                      context->cur_qname,
                                                      context->decl_public,
                                                      context->decl_system);
            }
        }
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
        if (context->decl_ndata) {
            g_free (context->decl_ndata);
            context->decl_ndata = NULL;
        }
        context->linecur++; context->colnum++;
        context->doctype_state = DOCTYPE_STATE_INT;
    }

 error:
    return;
}


static void
context_parse_parameter (Context *context)
{
    const char *beg = context->linecur + 1;
    char *entname = NULL;
    char *value = NULL;
    int colnum = context->colnum;

    AXING_DEBUG ("context_parse_parameter: %s\n", context->linecur);
    g_assert (context->linecur[0] == '%');
    context->linecur++; colnum++;

    while (context->linecur[0] != '\0') {
        gunichar cp;
        if (context->linecur[0] == ';') {
            break;
        }
        /* FIXME */
        cp = g_utf8_get_char (context->linecur);
        if (context->linecur == beg) {
            if (!XML_IS_NAME_START_CHAR (cp)) {
                ERROR_ENTITY (context);
            }
        }
        else {
            if (!XML_IS_NAME_CHAR (cp)) {
                ERROR_ENTITY (context);
            }
        }
        context->linecur = g_utf8_next_char (context->linecur);
        colnum += 1;
    }
    if (context->linecur[0] != ';') {
        ERROR_ENTITY (context);
    }
    entname = g_strndup (beg, context->linecur - beg);
    context->linecur++; colnum++;

    value = axing_dtd_schema_get_parameter (context->parser->doctype, entname);
    if (value) {
        Context *entctxt = context_new (context->parser);
        AXING_DEBUG ("  PUSH PARAMETER STRING CONTEXT\n");

        entctxt->parent = context;
        /* FIXME: maybe we could avoid the strdup, and have context_free
           check if ->basename == ->parent->basename before freeing */
        entctxt->basename = g_strdup (context->basename);
        /* FIXME: we can hand ownership over without the strdup */
        entctxt->entname = g_strdup (entname);
        entctxt->showname = g_strdup_printf ("%s(%%%s;)", entctxt->basename, entname);
        entctxt->state = context->state;
        entctxt->init_state = context->state;
        entctxt->doctype_state = context->doctype_state;

        entctxt->parent = context;
        context->parser->context = entctxt;
        /* Let entctxt own value */
        entctxt->line = value;
        value = NULL;
        entctxt->linecur = entctxt->line;
    }
    else {
        ERROR_FIXME (context);
    }

 error:
    context->colnum = colnum;
    g_free (entname);
    g_free (value);
}


static void
context_parse_cdata (Context *context)
{
    char *start;

    AXING_DEBUG ("context_parse_cdata: %s\n", context->linecur);
    if (context->state != PARSER_STATE_CDATA) {
        g_assert (EQ9 (context->linecur, '<', '!', '[', 'C', 'D', 'A', 'T', 'A', '['));
        context->parser->txtlinenum = context->linenum;
        context->parser->txtcolnum = context->colnum;
        context->linecur += 9; context->colnum += 9;
        context->state = PARSER_STATE_CDATA;
    }

    start = context->linecur;
    while (context->linecur[0] != '\0') {
        if (EQ3 (context->linecur, ']', ']', '>')) {
            g_string_append_len (context->parser->cur_text, start, context->linecur - start);
            context->linecur += 3; context->colnum += 3;
            context->parser->event_type = AXING_NODE_TYPE_CDATA;
            context->state = PARSER_STATE_TEXT;
            return;
        }
        CONTEXT_ADVANCE_CHAR (context, context->linecur, TRUE);
    }
    if (context->linecur != start)
        g_string_append_len (context->parser->cur_text, start, context->linecur - start);

 error:
    return;
}


static void
context_parse_comment (Context *context)
{
    AXING_DEBUG ("context_parse_comment: %s\n", context->linecur);
    if (context->state != PARSER_STATE_COMMENT) {
        g_assert (EQ4 (context->linecur, '<', '!', '-', '-'));
        context->parser->txtlinenum = context->linenum;
        context->parser->txtcolnum = context->colnum;
        context->linecur += 4; context->colnum += 4;
        context->prev_state = context->state;
        context->state = PARSER_STATE_COMMENT;
    }

    if (context->state == PARSER_STATE_COMMENT) {
        char *cur = context->linecur;

        while (cur[0] != '\0') {
            if (cur[0] == '-' && cur[1] == '-') {
                if (cur[2] != '>') {
                    ERROR_SYNTAX_MSG (context, "Two hyphens not allowed in comment"); // test: comment03
                }
                if (context->prev_state == PARSER_STATE_DOCTYPE) {
                    /* currently not doing anything with comments in the internal subset */
                    cur += 3; context->colnum += 3;
                    g_string_truncate (context->parser->cur_text, 0);
                }
                else {
                    if (cur != context->linecur)
                        g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                    cur += 3; context->colnum += 3;
                    context->parser->event_type = AXING_NODE_TYPE_COMMENT;
                }
                context->linecur = cur;
                context->state = context->prev_state;
                if (context->state == PARSER_STATE_TEXTDECL)
                    context->state = context->init_state;
                else if (context->state == PARSER_STATE_START)
                    context->state = PARSER_STATE_PROLOG;
                return;
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

 error:
    return;
}


static void
context_parse_instruction (Context *context)
{
    AXING_DEBUG ("context_parse_instruction: %s\n", context->linecur);
    if (context->state != PARSER_STATE_INSTRUCTION) {
        g_assert (EQ2 (context->linecur, '<', '?'));
        context->parser->txtlinenum = context->linenum;
        context->parser->txtcolnum = context->colnum;
        context->linecur += 2; context->colnum += 2;

        CONTEXT_GET_NAME (context, context->cur_qname);
        if (!(XML_IS_SPACE(context->linecur, context) ||
              context->linecur[0] == '\0' || context->linecur[0] == '?')) {
            ERROR_SYNTAX_MSG (context, "Expected space or question mark"); // test: newlines03
        }

        context->prev_state = context->state;
        context->state = PARSER_STATE_INSTRUCTION;
    }

    if (context->parser->cur_text->len == 0) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\0')
            return;
    }

    if (context->state == PARSER_STATE_INSTRUCTION) {
        char *cur = context->linecur;

        while (cur[0] != '\0') {
            if (EQ2 (cur, '?', '>')) {
                if (context->prev_state == PARSER_STATE_DOCTYPE) {
                    /* currently not doing anything with PIs in the internal subset */
                    cur += 2; context->colnum += 2;
                    g_string_truncate (context->parser->cur_text, 0);
                    g_clear_pointer (&(context->cur_qname), g_free);
                }
                else {
                    if (cur != context->linecur)
                        g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                    cur += 2; context->colnum += 2;
                    context->parser->event_type = AXING_NODE_TYPE_INSTRUCTION;
                }
                context->linecur = cur;
                context->state = context->prev_state;
                if (context->state == PARSER_STATE_TEXTDECL)
                    context->state = context->init_state;
                else if (context->state == PARSER_STATE_START)
                    context->state = PARSER_STATE_PROLOG;
                return;
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }

 error:
    return;
}

static void
context_parse_end_element (Context *context)
{
    gchar *qname = NULL;
    AXING_DEBUG ("context_parse_end_element: %s\n", context->linecur);

    if (context->state != PARSER_STATE_ENDELM) {
        gboolean matches;
        int i;
        int colnum;
        /* We've just encountered an end tag. This could be skipped if there is
           space or newlines between the qname and the ">". That would set the
           state and potentially re-enter this function later.
         */
        g_assert (EQ2 (context->linecur, '<', '/'));

        if (context->parser->event == NULL ||
            context->parser->event->context != context) {
            ERROR_EXTRACONTENT (context); // test: element13
        }

        colnum = context->colnum;
        context->colnum += 2;
        context->linecur += 2; 

        /* Don't have to actually read/dup the name. Just check that it's
           byte-for-byte the same as the start element name. But then also
           check that the start element name isn't a prefix of this name. */
        matches = TRUE;
        for (i = 0; context->parser->event->qname[i] != '\0'; i++) {
            if (context->linecur[i] != context->parser->event->qname[i]) {
                matches = FALSE;
                break;
            }
        }
        if (matches && axing_utf8_bytes_name(context->linecur + i)) {
            matches = FALSE;
        }
        if (!matches) {
            /* But if it's not a match, we go ahead and burn cycles reading
               the name for the error reporting. */
            CONTEXT_GET_NAME (context, qname);
            context->colnum = colnum;
            ERROR_WRONGEND (context, qname);
        }
        context->linecur += i;
        /* I wonder if I could just accumulate this count during the loop above. */
        context->colnum += g_utf8_strlen (context->parser->event->qname, -1);

        /* Now we just re-use the start element event */
        context->parser->event->linenum = context->linenum;
        context->parser->event->colnum = colnum;
        if (!(XML_IS_SPACE(context->linecur, context) ||
              context->linecur[0] == '\0' || context->linecur[0] == '>')) {
            ERROR_SYNTAX_MSG (context, "Expected space or closing angle bracket"); // test: element08
        }
    }

    CONTEXT_EAT_SPACES (context);
    if (context->linecur[0] == '\0') {
        context->state = PARSER_STATE_ENDELM;
        return;
    }
    if (context->linecur[0] != '>') {
        ERROR_SYNTAX_MSG (context, "Expected space or closing angle bracket"); // test: element14
    }
    context->linecur++; context->colnum++;

    if (context->parser->event->parent == NULL) {
        context->state = context->init_state;
        if (context->state == PARSER_STATE_PROLOG)
            context->state = PARSER_STATE_EPILOG;
    }
    else {
        context->state = PARSER_STATE_TEXT;
    }

    context->parser->event_type = AXING_NODE_TYPE_END_ELEMENT;
    return;

 error:
    g_free (qname);
    return;
}


static void
context_parse_start_element (Context *context)
{
    Event *event;
    AXING_DEBUG ("context_parse_start_element: %s\n", context->linecur);
    g_assert (context->linecur[0] == '<');

    event = event_new (context);
    event->parent = context->parser->event;
    context->parser->event = event;
    event->linenum = context->linenum;
    event->colnum = context->colnum;
    context->linecur++; context->colnum++;

    CONTEXT_GET_NAME (context, event->qname);

    if (context->linecur[0] == '>') {
        context->linecur++; context->colnum++;
        context_finish_start_element (context);
        return;
    }
    else if (EQ2 (context->linecur, '/', '>')) {
        event->empty = TRUE;
        context->linecur += 2; context->colnum += 2;
        context_finish_start_element (context);
        return;
    }
    else if (XML_IS_SPACE(context->linecur, context) || context->linecur[0] == '\0') {
        context->state = PARSER_STATE_STELM_BASE;
    }
    else {
        ERROR_SYNTAX_MSG (context, "Expected space, slash, or closing angle bracket"); // test: element07
    }

 error:
    return;
}


static void
context_parse_attrs (Context *context)
{
    Event *attr;
    AXING_DEBUG ("context_parse_attrs: %s\n", context->linecur);

    /* expecting the attr key, as a qname */
    if (context->state == PARSER_STATE_STELM_BASE) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '>') {
            context->linecur++; context->colnum++;
            context_finish_start_element (context);
            return;
        }
        else if (EQ2 (context->linecur, '/', '>')) {
            context->parser->event->empty = TRUE;
            context->linecur += 2; context->colnum += 2;
            context_finish_start_element (context);
            return;
        }
        if (context->linecur[0] == '\0') {
            return;
        }

        attr = event_new (context);
        attr->parent = context->parser->event->attrs;
        attr->linenum = context->linenum;
        attr->colnum = context->colnum;
        context->parser->event->attrs = attr;

        CONTEXT_GET_NAME (context, context->parser->event->attrs->qname);
        if (!(XML_IS_SPACE(context->linecur, context) ||
              context->linecur[0] == '\0' || context->linecur[0] == '=')) {
            ERROR_SYNTAX_MSG (context, "Expected space or equals sign"); // test: element09
        }
        context->state = PARSER_STATE_STELM_ATTNAME;
    }
    /* expecting an equals sign, which can be surrounded by space */
    if (context->state == PARSER_STATE_STELM_ATTNAME) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '=') {
            context->linecur++; context->colnum++;
            context->state = PARSER_STATE_STELM_ATTEQ;
        }
        else if (context->linecur[0] == '\0') {
            return;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected space or equals sign"); // test: element15
        }
    }
    /* expecting a quote char, single or double */
    if (context->state == PARSER_STATE_STELM_ATTEQ) {
        CONTEXT_EAT_SPACES (context);
        if (context->linecur[0] == '\'' || context->linecur[0] == '"') {
            context->quotechar = context->linecur[0];
            context->linecur++; context->colnum++;
            context->state = PARSER_STATE_STELM_ATTVAL;
        }
        else if (context->linecur[0] == '\0') {
            return;
        }
        else {
            ERROR_SYNTAX_MSG (context, "Expected quote character"); // test: attribute04
        }
    }
    /* in the quoted value */
    if (context->state == PARSER_STATE_STELM_ATTVAL) {
        char *cur = context->linecur;
        while (cur[0] != '\0') {
            if (cur[0] == context->quotechar) {
                char *xmlns = NULL;
                char *attrval;
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                attrval = g_strdup (context->parser->cur_text->str);
                g_string_truncate (context->parser->cur_text, 0);
                attr = context->parser->event->attrs;

                if (EQ6 (attr->qname, 'x', 'm', 'l', 'n', 's', ':')) {
                    /* FIXME: if cur_attrname == "xmlns:"? */
                    xmlns = attr->qname + 6;
                }
                else if (EQ6 (attr->qname, 'x', 'm', 'l', 'n', 's', '\0')) {
                    xmlns = "";
                }

                if (xmlns != NULL) {
                    Event *xmlnsev;
                    /* This is actually a namespace declaration, which we treat differently */
                    xmlnsev = context->parser->event->attrs;
                    context->parser->event->attrs = context->parser->event->attrs->parent;
                    xmlnsev->parent = context->parser->event->xmlns;
                    context->parser->event->xmlns = xmlnsev;

                    if (EQ4 (xmlns, 'x', 'm', 'l', '\0')) {
                        if (!g_str_equal(attrval, NS_XML))
                            ERROR_NS_INVALID (context, xmlns); // test: xmlns06
                    }
                    else {
                        if (g_str_equal(attrval, NS_XML))
                            ERROR_NS_INVALID (context, xmlns); // test: xmlns07
                    }
                    if (EQ6 (xmlns, 'x', 'm', 'l', 'n', 's', '\0')) {
                        ERROR_NS_INVALID (context, xmlns); // test: xmlns08
                    }
                    /* FIXME REFACTOR look for dups
                    if (g_hash_table_lookup (context->cur_nshash, xmlns) != NULL) {
                        ERROR_DUPATTR (context);
                    }
                    */
                    /* FIXME: ensure namespace is valid URI(1.0) or IRI(1.1) */
                    xmlnsev->content = attrval;
                }
                else {
                    Event *attrup;
                    for (attrup = attr->parent; attrup; attrup = attrup->parent) {
                        if (g_str_equal (attrup->qname, attr->qname))
                            ERROR_DUPATTR (context, attr); // test: attribute03
                    }
                    /* Can't check the expanded name yet. We'll do it in finish_start_element */
                    attr->content = attrval;
                }

                context->state = PARSER_STATE_STELM_BASE;
                cur++; context->colnum++;
                context->linecur = cur;
                if (!(cur[0] == '>' || cur[0] == '/' ||
                      cur[0] == '\0' || XML_IS_SPACE (cur, context))) {
                    ERROR_SYNTAX_MSG (context, "Expected space or closing angle bracket"); // test: attribute05
                }
                return;
            }
            else if (cur[0] == '&') {
                if (cur != context->linecur)
                    g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
                context->linecur = cur;
                context_parse_entity (context);
                if (context->parser->error)
                    goto error;
                /* entities could give us a new context, bubble out */
                if (context != context->parser->context)
                    return;
                cur = context->linecur;
                continue;
            }
            else if (context->linecur[0] == '<') {
                ERROR_SYNTAX_MSG (context, "Opening angle bracket not allowed in attribute values"); // test: entities18
            }
            CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
        }
        if (cur != context->linecur)
            g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
        context->linecur = cur;
    }
 error:
    return;
}


static void
context_parse_entity (Context *context)
{
    char *entname = NULL;
    int colnum = context->colnum;
    AXING_DEBUG ("context_parse_entity: %s\n", context->linecur);
    g_assert (context->linecur[0] == '&');
    context->linecur++; context->colnum++;

    if (context->linecur[0] == '#') {
        gunichar cp = 0;
        context->linecur++; context->colnum++;
        if (context->linecur[0] == 'x') {
            context->linecur++; context->colnum++;
            while (context->linecur[0] != '\0') {
                if (context->linecur[0] == ';')
                    break;
                else if (context->linecur[0] >= '0' && context->linecur[0] <= '9')
                    cp = 16 * cp + (context->linecur[0] - '0');
                else if (context->linecur[0] >= 'A' && context->linecur[0] <= 'F')
                    cp = 16 * cp + 10 + (context->linecur[0] - 'A');
                else if (context->linecur[0] >= 'a' && context->linecur[0] <= 'f')
                    cp = 16 * cp + 10 + (context->linecur[0] - 'a');
                else
                    ERROR_ENTITY_MSG (context, "Expected hexadecimal digit"); // test: entities36
                context->linecur++; context->colnum++;
            }
            if (context->linecur[0] != ';')
                ERROR_ENTITY_MSG (context, "Expected hexadecimal digit"); // test: entities37
            context->linecur++; context->colnum++;
        }
        else {
            while (context->linecur[0] != '\0') {
                if (context->linecur[0] == ';')
                    break;
                else if (context->linecur[0] >= '0' && context->linecur[0] <= '9')
                    cp = 10 * cp + (context->linecur[0] - '0');
                else
                    ERROR_ENTITY_MSG (context, "Expected decimal digit"); // test: entities09
                context->linecur++; context->colnum++;
            }
            if (context->linecur[0] != ';')
                ERROR_ENTITY_MSG (context, "Expected decimal digit"); // test: entities38
            context->linecur++; context->colnum++;
        }
        if (XML_IS_CHAR(cp, context) || XML_IS_CHAR_RESTRICTED(cp, context)) {
            g_string_append_unichar (context->parser->cur_text, cp);
        }
        else {
            context->colnum = colnum;
            ERROR_ENTITY_MSG (context, "Replacement text contains invalid character"); // test: entities39
        }
    }
    else {
        const char *beg = context->linecur;
        char builtin = '\0';

        while (context->linecur[0] != '\0') {
            gunichar cp;
            if (context->linecur[0] == ';') {
                break;
            }
            cp = g_utf8_get_char (context->linecur);
            if (context->linecur == beg) {
                if (!XML_IS_NAME_START_CHAR(cp))
                    ERROR_ENTITY_MSG (context, "Expected name start character"); // test: entities04 entities08
            }
            else {
                if (!XML_IS_NAME_CHAR(cp))
                    ERROR_ENTITY_MSG (context, "Expected name character or semicolon"); // test: entities03 entities07
            }
            context->linecur = g_utf8_next_char (context->linecur);
            context->colnum += 1;
        }
        if (context->linecur[0] != ';') {
            ERROR_ENTITY_MSG (context, "Expected name character or semicolon"); // test: entities02 entities06
        }
        entname = g_strndup (beg, context->linecur - beg);
        context->linecur++; context->colnum++;

        if (EQ3 (entname, 'l', 't', '\0'))
            builtin = '<';
        else if (EQ3 (entname, 'g', 't', '\0'))
            builtin = '>';
        else if (EQ4 (entname, 'a', 'm', 'p', '\0'))
            builtin = '&';
        else if (EQ5 (entname, 'a', 'p', 'o', 's', '\0'))
            builtin = '\'';
        else if (EQ5 (entname, 'q', 'u', 'o', 't', '\0'))
            builtin = '"';

        if (builtin != '\0') {
            g_string_append_c (context->parser->cur_text, builtin);
        }
        else {
            context_process_entity (context, entname);
        }
    }
 error:
    g_free (entname);
    return;
}


static void
context_process_entity (Context *context, const char *entname)
{ 
    Context *parent;
    char *value=NULL, *system=NULL, *public=NULL, *ndata=NULL;
    AXING_DEBUG ("context_process_entity: %s\n", entname);

    for (parent = context->parent; parent != NULL; parent = parent->parent) {
        if (parent->entname && g_str_equal (entname, parent->entname)) {
            context->colnum -= strlen(entname) + 2;
            ERROR_ENTITY_MSG (context, "Entity reference loop"); // test: entities26 entities27
        }
    }

    if (axing_dtd_schema_get_entity_full (context->parser->doctype, entname,
                                          &value, &public, &system, &ndata)) {
        if (value) {
            Context *entctxt = context_new (context->parser);
            AXING_DEBUG ("  PUSH ENTITY STRING CONTEXT\n");
            entctxt->parent = context;
            context->parser->context = entctxt;
            entctxt->basename = g_strdup (context->basename);
            entctxt->entname = g_strdup ((char *) entname);
            /* Let entctxt own value */
            entctxt->line = value;
            value = NULL;
            entctxt->linecur = entctxt->line;

            entctxt->showname = g_strdup_printf ("%s(&%s;)", entctxt->basename, entname);
            entctxt->state = context->state;
            entctxt->init_state = context->state;
        }
        else if (ndata) {
            ERROR_FIXME (context);
        }
        else {
            if (context->state == PARSER_STATE_STELM_ATTVAL) {
                context->colnum -= strlen(entname) + 2;
                ERROR_ENTITY_MSG (context, "Attributes cannot reference external entities"); // test: entities22
            }

            if (context->parser->resolver == NULL)
                context->parser->resolver = axing_resolver_get_default ();

#ifdef REFACTOR
            if (context->parser->async) {
                Context *entctxt;
                AXING_DEBUG ("  PUSH ENTITY SYSTEM CONTEXT ASYNC\n");
                entctxt = context_new (context->parser);
                entctxt->parent = context;
                entctxt->entname = g_strdup (entname);
                entctxt->state = PARSER_STATE_TEXTDECL;
                entctxt->init_state = context->state;
                axing_resolver_resolve_async (resolver, context->resource,
                                              NULL, system, public, "xml:entity",
                                              context->parser->cancellable,
                                              (GAsyncReadyCallback) context_process_entity_resolved,
                                              entctxt);
                context->pause_line = g_strdup (*line);
                while ((*line)[0])
                    (*line)++;
            }
            else {
#endif /* REFACTOR */
            if (TRUE) {
                Context *entctxt;
                AxingResource *resource;
                GFile *file;
                resource = axing_resolver_resolve (context->parser->resolver,
                                                   context->resource,
                                                   NULL, system, public,
                                                   AXING_RESOLVER_HINT_ENTITY,
                                                   context->parser->cancellable,
                                                   &(context->parser->error));
                if (context->parser->error)
                    goto error;

                AXING_DEBUG ("  PUSH ENTITY SYSTEM CONTEXT SYNC\n");
                entctxt = context_new (context->parser);
                entctxt->parent = context;

                entctxt->resource = resource;
                entctxt->basename = resource_get_basename (resource);
                entctxt->entname = g_strdup (entname);
                entctxt->state = PARSER_STATE_TEXTDECL;
                entctxt->init_state = context->state;
                context->parser->context = entctxt;

                context_start_sync (entctxt);
                /* REFACTOR FIXME: return TRUE if decl? */
            }
        }
    }
    else {
        g_assert_not_reached();
        ERROR_FIXME (context); // entity not found, handle correctly
    }            

 error:
    g_free (value);
    g_free (public);
    g_free (system);
    g_free (ndata);
}


#ifdef REFACTOR
static void
context_process_entity_resolved (AxingResolver *resolver,
                                 GAsyncResult  *result,
                                 Context       *context)
{
    AxingResource *resource;

    resource = axing_resolver_resolve_finish (resolver, result,
                                              &(context->parser->error));
    if (context->parser->error)
        goto error;

    context->resource = resource;
    context->basename = resource_get_basename (resource);

    axing_resource_read_async (context->resource,
                               context->parser->cancellable,
                               (GAsyncReadyCallback) context_resource_read_cb,
                               context);
 error:
    g_object_unref (resolver);
}

static void
context_process_entity_finish (Context *context)
{
    Context *parent;
    parent = context->parent;
    context->parent = NULL;

    parent->state = context->state;
    context_free (context);

    context_read_data_cb (NULL, NULL, parent);
}

#endif /* REFACTOR */

static void
context_parse_text (Context *context)
{
    char *cur = context->linecur;
    AXING_DEBUG ("context_parse_text: %s\n", context->linecur);
    while (cur[0] != '\0') {
        if (cur[0] == '<') {
            if (cur != context->linecur)
                g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
            if (context->parser->cur_text->len != 0)
                context->parser->event_type = AXING_NODE_TYPE_CONTENT;
            context->linecur = cur;
            return;
        }
        if (context->parser->cur_text->len == 0) {
            context->parser->txtlinenum = context->linenum;
            context->parser->txtcolnum = context->colnum;
        }
        if (cur[0] == '&') {
            if (cur != context->linecur)
                g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
            context->linecur = cur;
            context_parse_entity (context);
            if (context->parser->error)
                goto error;
            /* entities could give us a new context, bubble out */
            if (context != context->parser->context)
                return;
            cur = context->linecur;
            continue;
        }
        CONTEXT_ADVANCE_CHAR (context, cur, TRUE);
    }
    if (cur != context->linecur)
        g_string_append_len (context->parser->cur_text, context->linecur, cur - context->linecur);
    context->linecur = cur;

 error:
    context->linecur = cur;
    return;
}


static void
context_finish_start_element (Context *context)
{
    Event *event, *attr;
    gint attrnum = 0, numattrs = 0;
    /* At this point we only know that we have a syntactically correct qname,
       but we haven't yet tried to resolve the namespace. Do that now.
     */
    event = context->parser->event;
    const char *colon = strchr (event->qname, ':');
    if (colon != NULL) {
        const char *localname;
        const char *namespace;
        if (colon == event->qname) {
            ERROR_NS_QNAME (context); // test: xmlns09
        }
        localname = colon + 1;
        if (localname[0] == '\0' || strchr (localname, ':')) {
            ERROR_NS_QNAME (context); // test: xmlns10
        }
        if (!axing_utf8_bytes_name_start (localname)) {
            ERROR_NS_QNAME (context); // test: xmlns11
        }
        event->prefix = g_strndup (event->qname,
                                   colon - event->qname);
        /* FIXME: I feel like localname could always point inside qname and not get dupd or freed */
        event->localname = g_strdup (localname);
        namespace = reader_lookup_namespace (AXING_READER (context->parser),
                                             event->prefix);
        if (namespace == NULL) {
            ERROR_NS_NOTFOUND (context); // test: xmlns12
        }
        event->namespace = g_strdup (namespace);
    }
    else {
        const char *namespace = reader_lookup_namespace (AXING_READER (context->parser), "");
        if (namespace != NULL)
            event->namespace = g_strdup (namespace);
    }

    /* Similarly, we only know that we could parse the attributes.
       We still need to check those namespaces.
     */
    for (attr = event->attrs; attr; attr = attr->parent)
        numattrs++;
    event->attrkeys = g_new0 (char *, numattrs + 1);
    for (attr = event->attrs; attr; attr = attr->parent) {
        Event *attrup;
        const char *colon = strchr (attr->qname, ':');
        event->attrkeys[numattrs - ++attrnum] = attr->qname;
        /* If it's a no-namespace attr, leave prefix, localname, and namespace NULL.
           Let the getters detect and return "", qname, and "", respectively.
         */
        if (colon != NULL) {
            const char *localname;
            const char *namespace;
            int pre;
            if (colon == attr->qname) {
                ERROR_NS_QNAME_ATTR (context, attr); // test: xmlns13
            }
            localname = colon + 1;
            if (localname[0] == '\0' || strchr (localname, ':')) {
                ERROR_NS_QNAME_ATTR (context, attr); // test: xmlns14
            }
            if (!axing_utf8_bytes_name_start (localname)) {
                ERROR_NS_QNAME_ATTR (context, attr); // test: xmlns15
            }
            attr->prefix = g_strndup (attr->qname, colon - attr->qname);
            attr->localname = g_strdup (localname);
            namespace = reader_lookup_namespace (AXING_READER (context->parser),
                                                 attr->prefix);
            if (namespace == NULL) {
                ERROR_NS_NOTFOUND_ATTR (context, attr); // test: xmlns16
            }
            attr->namespace = g_strdup (namespace);
        }
        for (attrup = event->attrs; attrup != attr; attrup = attrup->parent) {
            if (attrup->namespace && attr->namespace &&
                attrup->localname && attr->localname &&
                g_str_equal (attrup->namespace, attr->namespace) &&
                g_str_equal (attrup->localname, attr->localname) )
                ERROR_NS_DUPATTR (context, attrup); // test: xmlns04
        }
    }

    if (event->empty && event->parent == NULL) {
        context->state = context->init_state;
        if (context->state == PARSER_STATE_PROLOG)
            context->state = PARSER_STATE_EPILOG;
    }
    else {
        context->state = PARSER_STATE_TEXT;
    }

    context->parser->event_type = AXING_NODE_TYPE_ELEMENT;
    return;

 error:
    return;
}

#ifdef REFACTOR

static void
context_complete (Context *context)
{
    if (context->parent)
        context_process_entity_finish (context);
    else
        g_simple_async_result_complete (context->parser->result);
}
#endif /* REFACTOR */

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


static inline Context *
context_new (AxingXmlParser *parser)
{
    Context *context;

    context = g_new0 (Context, 1);
    context->state = PARSER_STATE_NONE;
    context->init_state = PARSER_STATE_PROLOG;
    context->bom_encoding = BOM_ENCODING_NONE;
    context->bom_checked = FALSE;
    context->linenum = 1;
    context->colnum = 1;
    /* Not reffing parser because then we get an unbreakable ref loop.
       Contexts shouls always get freed with the parser anyway. */
    context->parser = parser;

    return context;
}


static inline void
context_free (Context *context)
{
    g_clear_object (&context->resource);

    g_clear_object (&context->srcstream);
    g_clear_object (&context->datastream);

    g_free (context->line);

    g_free (context->basename);
    g_free (context->entname);
    g_free (context->showname);
    g_free (context->pause_line);
    g_free (context->cur_qname);

    g_free (context->decl_system);
    g_free (context->decl_public);
    g_free (context->decl_ndata);

    g_free (context);
}


static inline Event *
event_new (Context *context)
{
    Event *event;

    while (context->parser->eventpoolstart < EVENTPOOLSIZE) {
        if (!context->parser->eventpool[context->parser->eventpoolstart].inuse) {
            event = &(context->parser->eventpool[context->parser->eventpoolstart]);
            event->inuse = TRUE;
            event->context = context;
            event->empty = FALSE;
            return event;
        }
        context->parser->eventpoolstart++;
    }
    event = g_new0 (Event, 1);
    event->context = context;
    return event;
}


static inline void
event_free (Event *event)
{
    event->parent = NULL;

    if (event->qname)
        g_clear_pointer (&(event->qname), g_free);
    if (event->prefix)
        g_clear_pointer (&(event->prefix), g_free);
    if (event->localname)
        g_clear_pointer (&(event->localname), g_free);
    if (event->namespace)
        g_clear_pointer (&(event->namespace), g_free);
    if (event->nsname)
        g_clear_pointer (&(event->nsname), g_free);
    if (event->content)
        g_clear_pointer (&(event->content), g_free);

    if (event->attrkeys)
        g_clear_pointer (&(event->attrkeys), g_free); /* actual strings owned by attr structs */

    while (event->attrs) {
        Event *freeattr = event->attrs;
        event->attrs = event->attrs->parent;
        event_free (freeattr);
    }

    while (event->xmlns) {
        Event *freeattr = event->xmlns;
        event->xmlns = event->xmlns->parent;
        event_free (freeattr);
    }

    if (event->context &&
        event >= event->context->parser->eventpool &&
        event < event->context->parser->eventpool + EVENTPOOLSIZE) {
        event->context->parser->eventpoolstart = MIN (event - event->context->parser->eventpool,
                                                      event->context->parser->eventpoolstart);
        event->inuse = 0;
    } else {
        g_free (event);
    }
}
