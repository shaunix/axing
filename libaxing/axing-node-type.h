/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2017-2020 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __AXING_NODE_TYPE_H__
#define __AXING_NODE_TYPE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    AXING_NODE_TYPE_NONE,
    AXING_NODE_TYPE_ERROR,
    AXING_NODE_TYPE_DOCUMENT,
    AXING_NODE_TYPE_END_DOCUMENT, /* only used by stream api */
    AXING_NODE_TYPE_DECLARATION,  /* fixme: use this or props on DOCUMENT? */
    AXING_NODE_TYPE_ELEMENT,
    AXING_NODE_TYPE_END_ELEMENT,  /* only used by stream api */
    AXING_NODE_TYPE_ATTRIBUTE,    /* may be used to add attrs in stream api */
    AXING_NODE_TYPE_CONTENT,
    AXING_NODE_TYPE_COMMENT,
    AXING_NODE_TYPE_CDATA,
    AXING_NODE_TYPE_ENTITY,
    AXING_NODE_TYPE_INSTRUCTION,
} AxingNodeType;

G_END_DECLS

#endif /* __AXING_NODE_TYPE_H__ */
