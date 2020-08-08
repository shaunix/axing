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

#include "axing-utils.h"

int
main (int argc, char **argv)
{
    int errcode;

    setlocale(LC_ALL, "");

    errcode = 1;

    if (argc > 2) {
        char *result = axing_uri_resolve_relative (argv[1], argv[2]);
        g_print ("%s\n", result);
        g_free (result);
        return 0;
    }
    else if (argc > 1) {
        char *contents, *result;
        char **uris;

        errcode = 0;
        g_file_get_contents (argv[1], &contents, NULL, NULL);
        uris = g_strsplit (contents, "\n", 0);

        result = axing_uri_resolve_relative (uris[0], uris[1]);
        if (!g_str_equal (result, uris[2])) {
            errcode = 1;
            g_print ("%s: %s\n", argv[1], result);
        }

        g_strfreev (uris);
        g_free (contents);
        g_free (result);
    }

    return errcode;
}
