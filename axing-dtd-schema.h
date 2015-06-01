/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2012 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __AXING_DTD_SCHEMA_H__
#define __AXING_DTD_SCHEMA_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "axing-resolver.h"
#include "axing-resource.h"
#include "axing-stream.h"

G_BEGIN_DECLS

#define AXING_TYPE_DTD_SCHEMA   (axing_dtd_schema_get_type ())
#define AXING_DTD_SCHEMA_ERROR  (axing_dtd_schema_error_quark ())

G_DECLARE_DERIVABLE_TYPE (AxingDtdSchema, axing_dtd_schema, AXING, DTD_SCHEMA, GObject)

struct _AxingDtdSchemaClass {
    GObjectClass parent_class;

    /*< private >*/
    void (*_axing_reserved1) (void);
    void (*_axing_reserved2) (void);
    void (*_axing_reserved3) (void);
    void (*_axing_reserved4) (void);
    void (*_axing_reserved5) (void);
};

typedef enum {
    AXING_DTD_SCHEMA_ERROR_SYNTAX,
    AXING_DTD_SCHEMA_ERROR_OTHER
} AxingDtdSchemaError;

GQuark            axing_dtd_schema_error_quark              (void);

AxingDtdSchema *  axing_dtd_schema_new                      (void);

void              axing_dtd_schema_set_doctype              (AxingDtdSchema   *dtd,
                                                             const char       *doctype);
void              axing_dtd_schema_set_public_id            (AxingDtdSchema   *dtd,
                                                             const char       *public);
void              axing_dtd_schema_set_system_id            (AxingDtdSchema   *dtd,
                                                             const char       *system);

gboolean          axing_dtd_schema_add_element              (AxingDtdSchema   *dtd,
                                                             const char       *name,
                                                             const char       *content,
                                                             GError          **error);
gboolean          axing_dtd_schema_add_attlist              (AxingDtdSchema   *dtd,
                                                             const char       *name,
                                                             const char       *attlist,
                                                             GError          **error);

gboolean          axing_dtd_schema_add_entity               (AxingDtdSchema   *dtd,
                                                             const char       *name,
                                                             const char       *value);
gboolean          axing_dtd_schema_add_external_entity      (AxingDtdSchema   *dtd,
                                                             const char       *name,
                                                             const char       *public,
                                                             const char       *system);
gboolean          axing_dtd_schema_add_unparsed_entity      (AxingDtdSchema   *dtd,
                                                             const char       *name,
                                                             const char       *public,
                                                             const char       *system,
                                                             const char       *ndata);
gboolean          axing_dtd_schema_add_parameter            (AxingDtdSchema  *dtd,
                                                             const char      *name,
                                                             const char      *value);
gboolean          axing_dtd_schema_add_external_parameter   (AxingDtdSchema  *dtd,
                                                             const char      *name,
                                                             const char      *public,
                                                             const char      *system);
gboolean          axing_dtd_schema_add_notation             (AxingDtdSchema  *dtd,
                                                             const char      *name,
                                                             const char      *public,
                                                             const char      *system);

char *            axing_dtd_schema_get_entity               (AxingDtdSchema  *dtd,
                                                             const char      *name);
char *            axing_dtd_schema_get_external_entity      (AxingDtdSchema  *dtd,
                                                             const char      *name);
char *            axing_dtd_schema_get_unparsed_entity      (AxingDtdSchema  *dtd,
                                                             const char      *name);
gboolean          axing_dtd_schema_get_entity_full          (AxingDtdSchema  *dtd,
                                                             const char      *name,
                                                             char           **value,
                                                             char           **public,
                                                             char           **system,
                                                             char           **ndata);
char *            axing_dtd_schema_get_parameter            (AxingDtdSchema  *dtd,
                                                             const char      *name);

G_END_DECLS

#endif /* __AXING_DTD_SCHEMA_H__ */
