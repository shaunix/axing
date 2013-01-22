#include <locale.h>

#include "axing-xml-parser.h"
#include "axing-stream.h"

static void noop (void) { return; }

int
main (int argc, char **argv)
{
  GFile *file;
  AxingResource *resource;
  AxingXmlParser *parser;
  int i;

  g_type_init();
  setlocale(LC_ALL, "");

  for (i = 0; i < 100; i++) {
    file = g_file_new_for_path ("time.xml");
    resource = axing_resource_new (file, NULL);
    parser = axing_xml_parser_new (resource, NULL);
    g_signal_connect (parser, "stream-event", G_CALLBACK (noop), NULL);
    g_object_unref (resource);
    g_object_unref (file);
    axing_xml_parser_parse (parser, NULL, NULL);
    g_object_unref (parser);
  }

  return 0;
}
