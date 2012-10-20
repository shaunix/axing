#include <locale.h>

#include "axing-xml-parser.h"
#include "axing-stream.h"

GMainLoop *loop;

int indent;

static void
stream_event (AxingStream *stream,
              gpointer     data)
{
  gchar *checksum;
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
    checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5,
                                              axing_stream_get_event_content (stream), -1);
    g_print ("# %s\n", checksum);
    g_free (checksum);
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
  if (error)
    g_print ("error: %s\n", error->message);
  else
    g_print ("finish\n");
  g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
  AxingXmlParser *parser;

  g_type_init ();
  setlocale(LC_ALL, "");

  indent = 0;

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
  return 0;
}
