#include <locale.h>

#include "axing-xml-parser.h"
#include "axing-stream.h"

GMainLoop *loop;

int errcode;
int indent;

static void
stream_event (AxingStream *stream,
              gpointer     data)
{
  char **attrs;
  gchar *encval;
  int i;
  switch (axing_stream_get_event_type (stream)) {
  case AXING_STREAM_EVENT_START_ELEMENT:
    for (i = 0; i < indent; i++) g_print ("  ");
    indent++;
    g_print ("[ %s %s|%s (%s)\n",
             axing_stream_get_event_qname (stream),
             axing_stream_get_event_prefix (stream),
             axing_stream_get_event_localname (stream),
             axing_stream_get_event_namespace (stream));
    attrs = (char **) axing_stream_get_attributes (stream);
    while ((*attrs) != NULL) {
      for (i = 0; i < indent; i++) g_print ("  ");
      encval = g_uri_escape_string (axing_stream_get_attribute_value (stream, *attrs, NULL),
                                    NULL, TRUE);
      g_print ("@ %s %s|%s (%s) %s\n",
               *attrs,
               axing_stream_get_attribute_prefix (stream, *attrs),
               axing_stream_get_attribute_localname (stream, *attrs),
               axing_stream_get_attribute_namespace (stream, *attrs),
               encval);
      g_free (encval);
      attrs++;
    }
    break;
  case AXING_STREAM_EVENT_END_ELEMENT:
    indent--;
    for (i = 0; i < indent; i++) g_print ("  ");
    g_print ("] %s %s|%s (%s)\n",
             axing_stream_get_event_qname (stream),
             axing_stream_get_event_prefix (stream),
             axing_stream_get_event_localname (stream),
             axing_stream_get_event_namespace (stream));
    break;
  case AXING_STREAM_EVENT_CONTENT:
    for (i = 0; i < indent; i++) g_print ("  ");
    encval = g_uri_escape_string (axing_stream_get_event_content (stream),
                                  NULL, TRUE);
    g_print ("# %s\n", encval);
    g_free (encval);
    break;
  case AXING_STREAM_EVENT_CDATA:
    for (i = 0; i < indent; i++) g_print ("  ");
    encval = g_uri_escape_string (axing_stream_get_event_content (stream),
                                  NULL, TRUE);
    g_print ("* %s\n", encval);
    g_free (encval);
    break;
  case AXING_STREAM_EVENT_COMMENT:
    for (i = 0; i < indent; i++) g_print ("  ");
    encval = g_uri_escape_string (axing_stream_get_event_content (stream),
                                  NULL, TRUE);
    g_print ("! %s\n", encval);
    g_free (encval);
    break;
  default:
    g_print ("event\n");
  }
}

static void
parse_finish (AxingXmlParser  *parser,
              GAsyncResult    *res,
              gpointer         data)
{
  GError *error = NULL;
  axing_xml_parser_parse_finish (parser, res, &error);
  if (error) {
    errcode = 1;
    g_print ("error: %s\n", error->message);
  }
  else {
    g_print ("finish\n");
  }
  g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
  AxingXmlParser *parser;

  g_type_init ();
  setlocale(LC_ALL, "");

  indent = 0;
  errcode = 0;

  if (argc > 1) {
    GFile *file;
    AxingResource *resource;
    file = g_file_new_for_commandline_arg (argv[1]);
    resource = axing_resource_new (file, NULL);
    parser = axing_xml_parser_new (resource, NULL);
    g_object_unref (resource);
    g_object_unref (file);

    g_signal_connect (parser, "stream-event", G_CALLBACK (stream_event), NULL);
    axing_xml_parser_parse_async (parser,
                                  NULL,
                                  (GAsyncReadyCallback) parse_finish,
                                  NULL);
  }
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  g_object_unref (parser);
  return errcode;
}
