#!/bin/sh

gcc -g3 -o test-axing-uri-resolver \
    $(pkg-config --libs gio-2.0 --cflags gio-2.0) \
    axing-utils.c \
    test-axing-uri-resolver.c

gcc -g3 -o test-axing-simple-resolver-sync \
    $(pkg-config --libs gio-2.0 --cflags gio-2.0) \
    axing-resolver.c \
    axing-resource.c \
    axing-simple-resolver.c \
    axing-utils.c \
    test-axing-simple-resolver-sync.c

gcc -g3 -o test-axing-xml-parser-sync \
    $(pkg-config --libs gio-2.0 --cflags gio-2.0) \
    axing-dtd-schema.c \
    axing-reader.c \
    axing-resolver.c \
    axing-resource.c \
    axing-simple-resolver.c \
    axing-xml-parser.c \
    axing-utils.c \
    test-axing-xml-parser-sync.c

gcc -g3 -o time-axing-xml-parser \
    $(pkg-config --libs gio-2.0 --cflags gio-2.0) \
    axing-dtd-schema.c \
    axing-reader.c \
    axing-resolver.c \
    axing-resource.c \
    axing-simple-resolver.c \
    axing-xml-parser.c \
    axing-utils.c \
    time-axing-xml-parser.c

gcc -g3 -o time-libxml2 \
    $(pkg-config --libs libxml-2.0 --cflags libxml-2.0) \
    time-libxml2.c
