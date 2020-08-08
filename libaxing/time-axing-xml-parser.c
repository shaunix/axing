#include <locale.h>

#include "axing-xml-parser.h"
#include "axing-reader.h"

static void noop (void) { return; }

int
main (int argc, char **argv)
{
  GFile *file;
  AxingResource *resource;
  AxingXmlParser *parser;
  int i;

  setlocale(LC_ALL, "");

  for (i = 0; i < 100; i++) {
    file = g_file_new_for_path ("time.xml");
    resource = axing_resource_new (file, NULL);
    parser = axing_xml_parser_new (resource, NULL);

    while (axing_reader_read (AXING_READER (parser), NULL)) { }

    g_object_unref (resource);
    g_object_unref (file);
    g_object_unref (parser);
  }

  return 0;
}
