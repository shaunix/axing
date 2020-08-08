#include <locale.h>

#include "axing-xml-parser.h"
#include "axing-reader.h"

int indent;

#if 0
static void
stream_event (AxingStream *stream,
              gpointer     data)
{
  gchar *encval;
  int i;
  switch (axing_stream_get_event_type (stream)) {
  case AXING_STREAM_EVENT_START_ELEMENT:
    NOW ATTRIBUTES
    break;
  default:
    g_print ("event\n");
  }
}
#endif

int
main (int argc, char **argv)
{
  int i;
  int errcode = 0;
  AxingXmlParser *parser;
  AxingReader *reader;
  char *encval;
  char **attrs;

  setlocale(LC_ALL, "");

  indent = 0;
  errcode = 0;

  if (argc > 1) {
    GFile *file;
    AxingResource *resource;
    GError *error = NULL;
    gulong handler;
    file = g_file_new_for_commandline_arg (argv[1]);
    resource = axing_resource_new (file, NULL);
    parser = axing_xml_parser_new (resource, NULL);
    reader = AXING_READER (parser);
    g_object_unref (resource);
    g_object_unref (file);

    while (axing_reader_read (AXING_READER (parser), &error)) {
      AxingNodeType type = axing_reader_get_node_type (reader);
      switch (type) {
      case AXING_NODE_TYPE_ELEMENT:
        for (i = 0; i < indent; i++) g_print ("  ");
        indent++;
        g_print ("[ %s %s|%s (%s) %s %i:%i\n",
                 axing_reader_get_qname (reader),
                 axing_reader_get_prefix (reader),
                 axing_reader_get_localname (reader),
                 axing_reader_get_namespace (reader),
                 axing_reader_get_nsname (reader),
                 axing_reader_get_linenum (reader),
                 axing_reader_get_colnum (reader) );
        attrs = (char **) axing_reader_get_attrs (reader);
        while ((*attrs) != NULL) {
          for (i = 0; i < indent; i++) g_print ("  ");
          encval = g_uri_escape_string (axing_reader_get_attr_value (reader, *attrs),
                                        NULL, FALSE);
          g_print ("@ %s %s|%s (%s) %s %i:%i \"%s\"\n",
                   *attrs,
                   axing_reader_get_attr_prefix (reader, *attrs),
                   axing_reader_get_attr_localname (reader, *attrs),
                   axing_reader_get_attr_namespace (reader, *attrs),
                   axing_reader_get_attr_nsname (reader, *attrs),
                   axing_reader_get_attr_linenum (reader, *attrs),
                   axing_reader_get_attr_colnum (reader, *attrs),
                   encval);
          g_free (encval);
          attrs++;
        }
        break;
      case AXING_NODE_TYPE_END_ELEMENT:
        indent--;
        for (i = 0; i < indent; i++) g_print ("  ");
        g_print ("] %s %s|%s (%s) %s %i:%i\n",
                 axing_reader_get_qname (reader),
                 axing_reader_get_prefix (reader),
                 axing_reader_get_localname (reader),
                 axing_reader_get_namespace (reader),
                 axing_reader_get_nsname (reader),
                 axing_reader_get_linenum (reader),
                 axing_reader_get_colnum (reader));
        break;
      case AXING_NODE_TYPE_CONTENT:
        for (i = 0; i < indent; i++) g_print ("  ");
        encval = g_uri_escape_string (axing_reader_get_content (reader),
                                      NULL, FALSE);
        g_print ("# %s\n", encval);
        g_free (encval);
        break;
      case AXING_NODE_TYPE_INSTRUCTION:
        for (i = 0; i < indent; i++) g_print ("  ");
        encval = g_uri_escape_string (axing_reader_get_content (reader),
                                      NULL, FALSE);
        g_print ("? %s %s\n",
                 axing_reader_get_qname (reader),
                 encval);
        g_free (encval);
        break;
      case AXING_NODE_TYPE_COMMENT:
        for (i = 0; i < indent; i++) g_print ("  ");
        encval = g_uri_escape_string (axing_reader_get_content (reader),
                                      NULL, FALSE);
        g_print ("! %s\n", encval);
        g_free (encval);
        break;
      case AXING_NODE_TYPE_CDATA:
        for (i = 0; i < indent; i++) g_print ("  ");
        encval = g_uri_escape_string (axing_reader_get_content (reader),
                                      NULL, FALSE);
        g_print ("* %s\n", encval);
        g_free (encval);
        break;
      default:
        g_print("DEFAULT\n");
        break;
      }
    }

    if (error) {
      errcode = 1;
      g_print ("error: %s\n", error->message);
    }
    else {
      g_print ("finish\n");
    }

    g_object_unref (parser);
  }

 error:
  return errcode;
}
