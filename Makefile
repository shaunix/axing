all: test-axing-parser-async test-axing-parser-sync test-axing-uri-resolver

test-axing-parser-async: *.c *.h
	gcc -g -o test-axing-parser-async \
	`pkg-config --cflags --libs gio-2.0` \
	axing-dtd-schema.c \
	axing-namespace-map.c \
	axing-resolver.c \
	axing-resource.c \
	axing-simple-resolver.c \
	axing-stream.c \
	axing-xml-parser.c \
	axing-utils.c \
	test-axing-parser-async.c

test-axing-parser-sync: *.c *.h
	gcc -g -o test-axing-parser-sync \
	`pkg-config --cflags --libs gio-2.0` \
	axing-dtd-schema.c \
	axing-namespace-map.c \
	axing-resolver.c \
	axing-resource.c \
	axing-simple-resolver.c \
	axing-stream.c \
	axing-xml-parser.c \
	axing-utils.c \
	test-axing-parser-sync.c

test-axing-uri-resolver: *.c *.h
	gcc -g -o test-axing-uri-resolver \
	`pkg-config --cflags --libs gio-2.0` \
	axing-utils.c \
	test-axing-uri-resolver.c

time-axing: *.c *.h
	gcc -o time-axing \
	`pkg-config --cflags --libs gio-2.0` \
	axing-dtd-schema.c \
	axing-namespace-map.c \
	axing-resolver.c \
	axing-resource.c \
	axing-simple-resolver.c \
	axing-stream.c \
	axing-xml-parser.c \
	axing-utils.c \
	time-axing.c

time-libxml2: time-libxml2.c
	gcc -o time-libxml2 \
	`pkg-config --cflags --libs libxml-2.0` \
	time-libxml2.c
