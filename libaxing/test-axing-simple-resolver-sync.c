/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2013 Shaun McCance  <shaunm@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#include <locale.h>

#include "axing-resource.h"
#include "axing-simple-resolver.h"

int
main (int argc, char **argv)
{
    AxingResolver *resolver;
    AxingResource *base, *rel;
    gchar *reluri, *expect;
    GError *error = NULL;
    int retcode = 0;

    setlocale(LC_ALL, "");

    resolver = axing_simple_resolver_new ();

    if (argc > 2) {
        base = axing_resource_new (g_file_new_for_uri (argv[1]), NULL);
        rel = axing_resolver_resolve (resolver,
                                      base,
                                      NULL, /* xml:base */
                                      argv[2],
                                      NULL, /* pubid */
                                      AXING_RESOLVER_HINT_OTHER,
                                      NULL, /* cancellable */
                                      &error);
        expect = NULL;
    }
    else if (argc > 1) {
        char *contents;
        char **uris;

        g_file_get_contents (argv[1], &contents, NULL, NULL);
        uris = g_strsplit (contents, "\n", 0);

        base = axing_resource_new (g_file_new_for_uri (uris[0]), NULL);
        rel = axing_resolver_resolve (resolver,
                                      base,
                                      NULL, /* xml:base */
                                      uris[1],
                                      NULL, /* pubid */
                                      AXING_RESOLVER_HINT_OTHER,
                                      NULL, /* cancellable */
                                      &error);
        expect = g_strdup (uris[2]);
        g_strfreev (uris);
        g_free (contents);
    }
    else {
        return 1;
    }

    if (rel) {
        reluri = g_file_get_uri (axing_resource_get_file (rel));

        if (expect) {
            if (!g_str_equal (reluri, expect)) {
                retcode = 1;
                g_print ("%s: %s\n", argv[1], reluri);
            }
            g_free (expect);
        }
        else {
            g_print ("%s\n", reluri);
        }

        g_free (reluri);
        g_object_unref (rel);
    }

    g_object_unref (base);
    g_object_unref (resolver);
    return retcode;
}
