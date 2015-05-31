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

#include <string.h>
#include "axing-utils.h"

typedef struct {
    char *scheme;
    char *authority;
    char *path;
    char *query;
    char *fragment;
} AxingUri;

static void
axing_uri_free (AxingUri *uri)
{
    g_free (uri->scheme);
    g_free (uri->authority);
    g_free (uri->path);
    g_free (uri->query);
    g_free (uri->fragment);
    g_free (uri);
}

static AxingUri *
axing_uri_parse (const char *str)
{
    char *delim, *path;
    AxingUri *uri = g_new0 (AxingUri, 1);

    path = (char *) str;
    /* want at least one char in scheme, else just treat it as path */
    for (delim = path + 1; delim[0] != '\0'; delim++)
        if (delim[0] == ':' || delim[0] == '/' || delim[0] == '?' || delim[0] == '#')
            break;
    if (delim[0] == ':') {
        uri->scheme = g_strndup (str, delim - str);
        path = delim + 1;
    }

    if (path[0] == '/' && path[1] == '/') {
        path = path + 2;
        for (delim = path; delim[0] != '\0'; delim++)
            if (delim[0] == '/' || delim[0] == '?' || delim[0] == '#')
                break;
        uri->authority = g_strndup (path, delim - path);
        path = delim;
    }

    if (path[0] != '?' && path[0] != '#') {
        for (delim = path; delim[0] != '\0'; delim++)
            if (delim[0] == '?' || delim[0] == '#')
                break;
        uri->path = g_strndup (path, delim - path);
        path = delim;
    }

    if (path[0] == '?') {
        path = path + 1;
        for (delim = path; delim[0] != '\0'; delim++)
            if (delim[0] == '#')
                break;
        uri->query = g_strndup (path, delim - path);
        path = delim;
    }

    if (path[0] == '#')
        uri->fragment = g_strdup (path + 1);

    return uri;
}

static char *
axing_uri_remove_dots (const char *path)
{
    GString *ret = g_string_new_len (NULL, strlen(path));
    char *cur = (char *) path;

    while (cur[0]) {
        if (cur[0] == '/') {
            g_string_append_c (ret, '/');
            cur++;
        }
        else if (g_str_has_prefix (cur, "./") || g_str_equal (cur, ".")) {
            cur++;
            if (cur[0])
                cur++;
        }
        else if (g_str_has_prefix (cur, "../") || g_str_equal (cur, "..")) {
            gsize top, end;
            for (top = ret->len - 1; top > 0; top--)
                if (ret->str[top] != '/')
                    break;
            if (top > 0) {
                for (end = top; end > 0 && ret->str[end] != '/'; end--);
                top++;
                while (ret->str[top]) {
                    ret->str[end] = ret->str[top];
                    end++; top++;
                }
                ret->str[end] = '\0';
                ret->len = end;
            }
            cur += 2;
            if (cur[0])
                cur++;
        }
        else {
            while (cur[0] && cur[0] != '/') {
                g_string_append_c (ret, cur[0]);
                cur++;
            }
        }
    }

    return g_string_free (ret, FALSE);
}

static char *
axing_uri_to_string (AxingUri *uri)
{
    return g_strconcat (uri->scheme ? uri->scheme : "",
                        uri->scheme ? ":" : "",
                        uri->authority ? "//" : "",
                        uri->authority ? uri->authority : "",
                        uri->path ? uri->path : "",
                        uri->query ? "?" : "",
                        uri->query ? uri->query : "",
                        uri->fragment ? "#" : "",
                        uri->fragment ? uri->fragment : "",
                        NULL);
}

char *
axing_uri_resolve_relative (const char *base, const char *link)
{
    AxingUri *baseu, *linku, *ret;
    char *retstr;

    linku = axing_uri_parse (link);
    if (linku->scheme && linku->scheme[0]) {
        axing_uri_free (linku);
        return g_strdup (link);
    }

    baseu = axing_uri_parse (base);
    ret = g_new0 (AxingUri, 1);

    ret->scheme = g_strdup (baseu->scheme);
    if (linku->authority) {
        ret->authority = g_strdup (linku->authority);
        ret->path = g_strdup (linku->path);
        ret->query = g_strdup (linku->query);
    }
    else {
        ret->authority = g_strdup (baseu->authority);
        if (linku->path && linku->path[0]) {
            if (linku->path[0] == '/') {
                ret->path = axing_uri_remove_dots (linku->path);
            }
            else {
                char *tmp, *slash;
                slash = strrchr (baseu->path, '/');
                if (slash == NULL) {
                    tmp = g_strconcat ("/", linku->path, NULL);
                }
                else if (slash[1] == '\0') {
                    tmp = g_strconcat (baseu->path, linku->path, NULL);
                }
                else {
                    slash[1] = '\0';
                    tmp = g_strconcat (baseu->path, linku->path, NULL);
                }
                ret->path = axing_uri_remove_dots (tmp);
                g_free (tmp);
            }
            ret->query = g_strdup (linku->query);
        }
        else {
            ret->path = g_strdup (baseu->path);
            if (linku->query)
                ret->query = g_strdup (linku->query);
            else
                ret->query = g_strdup (baseu->query);
        }
    }
    ret->fragment = g_strdup (linku->fragment);

    retstr = axing_uri_to_string (ret);

    axing_uri_free (linku);
    axing_uri_free (baseu);
    axing_uri_free (ret);

    return retstr;
}
