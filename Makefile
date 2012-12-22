all: test-axing-parser-async test-axing-parser-sync

test-axing-parser-async: *.c *.h
	gcc -g -o test-axing-parser-async \
	`pkg-config --cflags --libs gio-2.0` \
	axing-dtd-schema.c \
	axing-namespace-map.c \
	axing-resolver.c \
	axing-resource.c \
	axing-stream.c \
	axing-xml-parser.c \
	test-axing-parser-async.c

test-axing-parser-sync: *.c *.h
	gcc -g -o test-axing-parser-sync \
	`pkg-config --cflags --libs gio-2.0` \
	axing-dtd-schema.c \
	axing-namespace-map.c \
	axing-resolver.c \
	axing-resource.c \
	axing-stream.c \
	axing-xml-parser.c \
	test-axing-parser-sync.c
