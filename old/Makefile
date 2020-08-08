CC ?= gcc
PKGCONFIG = $(shell which pkg-config)
CFLAGS = $(shell $(PKGCONFIG) --cflags gio-2.0)
LIBS = $(shell $(PKGCONFIG) --libs gio-2.0)
LIBXML2_CFLAGS = $(shell $(PKGCONFIG) --cflags libxml-2.0)
LIBXML2_LIBS = $(shell $(PKGCONFIG) --libs libxml-2.0)

TESTS = \
	test-axing-parser-async \
	test-axing-parser-sync \
	test-axing-uri-resolver \
	time-axing \
	time-libxml2

SRCS = \
	axing-dtd-schema.c \
	axing-namespace-map.c \
	axing-resolver.c \
	axing-resource.c \
	axing-simple-resolver.c \
	axing-stream.c \
	axing-xml-parser.c\
	axing-utils.c

OBJS = $(SRCS:.c=.o)

all: gitignore $(TESTS)

clean:
	rm -f $(TESTS)
	rm -f *.o

gitignore: Makefile
	( echo "*~" ; \
	  echo "*.o" ; \
	  echo ".gitignore" ; \
	) > .gitignore ; \
	for p in $(TESTS); do \
	  echo "/$$p" >> .gitignore ; \
	done

PHONY = gitignore

%.o: %.c
	$(CC) -c -o $(@F) $(CFLAGS) $<

test-axing-parser-async: Makefile $(OBJS) test-axing-parser-async.o
	$(CC) -o $(@F) $(LIBS) $(OBJS) $(addsuffix .o,$(@F))


test-axing-parser-sync: Makefile $(OBJS) test-axing-parser-sync.o
	$(CC) -o $(@F) $(LIBS) $(OBJS) $(addsuffix .o,$(@F))

test-axing-uri-resolver: Makefile $(OBJ) test-axing-uri-resolver.o
	$(CC) -o $(@F) $(LIBS) $(OBJS) $(addsuffix .o,$(@F))

time-axing: Makefile $(OBJ) time-axing.o
	$(CC) -o $(@F) $(LIBS) $(OBJS) $(addsuffix .o,$(@F))

time-libxml2: Makefile
	$(CC) -c -o $(addsuffix .o,$(@F)) $(LIBXML2_CFLAGS) $(addsuffix .c,$(@F))
	$(CC) -o $(@F) $(LIBXML2_LIBS) $(addsuffix .o,$(@F))
