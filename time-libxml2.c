#include <locale.h>

#include <libxml/parser.h>

static void noop (void) { return; }

int
main (int argc, char **argv)
{
  xmlSAXHandler handler = {0,};
  int i;

  handler.startDocument = (startDocumentSAXFunc) noop;
  handler.endDocument = (endDocumentSAXFunc) noop;
  handler.startElement = (startElementSAXFunc) noop;
  handler.endElement = (endElementSAXFunc) noop;
  handler.characters = (charactersSAXFunc) noop;
  handler.ignorableWhitespace = (ignorableWhitespaceSAXFunc) noop;
  handler.processingInstruction = (processingInstructionSAXFunc) noop;
  handler.comment = (commentSAXFunc) noop;

  setlocale(LC_ALL, "");

  for (i = 0; i < 100; i++) {
    xmlSAXParseFile (&handler, "time.xml", 0);
  }

  return 0;
}
