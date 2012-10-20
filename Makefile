test-axion-parser-async: *.c *.h
	gcc -g -o test-axing-parser-async \
	`pkg-config --cflags --libs gio-2.0` \
	test-axing-parser-async.c \
	axing-namespace-map.c \
	axing-resolver.c \
	axing-resource.c \
	axing-stream.c \
	axing-xml-parser.c
