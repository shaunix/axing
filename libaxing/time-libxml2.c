#include <locale.h>

#include <libxml/xmlreader.h>

int
main (int argc, char **argv)
{
  xmlTextReaderPtr reader;
  int i;

  setlocale(LC_ALL, "");

  for (i = 0; i < 100; i++) {
    reader = xmlReaderForFile (argv[1] ? argv[1] : "time.xml", NULL, XML_PARSE_NOENT);

    while (xmlTextReaderRead (reader)) {}

    xmlFreeTextReader (reader);
  }

  return 0;
}
