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

#include "axing-dtd-schema.h"

struct _AxingDtdSchemaPrivate {
    char *doctype;
    char *public;
    char *system;
    GHashTable *general_entities;
    GHashTable *parameter_entities;
    GHashTable *notations;
};

typedef struct {
    char *name;
    char *value;
    char *public;
    char *system;
    char *ndata;
} EntityData;

static void
entity_data_free (EntityData *data)
{
    g_free (data->name);
    g_free (data->value);
    g_free (data->public);
    g_free (data->system);
    g_free (data->ndata);
    g_free (data);
}

static void      axing_dtd_schema_init          (AxingDtdSchema       *dtd);
static void      axing_dtd_schema_class_init    (AxingDtdSchemaClass  *klass);
static void      axing_dtd_schema_dispose       (GObject              *object);
static void      axing_dtd_schema_finalize      (GObject              *object);

G_DEFINE_TYPE_WITH_PRIVATE (AxingDtdSchema, axing_dtd_schema, G_TYPE_OBJECT)

static void
axing_dtd_schema_init (AxingDtdSchema *dtd)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);

    /* Use data->name as key, owned by data, so no key_destroy_func */
    priv->general_entities = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                                    (GDestroyNotify) entity_data_free);
    priv->parameter_entities = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                                      (GDestroyNotify) entity_data_free);
    priv->notations = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                             (GDestroyNotify) entity_data_free);
}

static void
axing_dtd_schema_class_init (AxingDtdSchemaClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = axing_dtd_schema_dispose;
    object_class->finalize = axing_dtd_schema_finalize;
}

static void
axing_dtd_schema_dispose (GObject *object)
{
    G_OBJECT_CLASS (axing_dtd_schema_parent_class)->dispose (object);
}

static void
axing_dtd_schema_finalize (GObject *object)
{
    AxingDtdSchema *dtd = AXING_DTD_SCHEMA (object);
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);

    g_free (priv->doctype);
    g_free (priv->public);
    g_free (priv->system);

    g_hash_table_destroy (priv->general_entities);
    g_hash_table_destroy (priv->parameter_entities);
    g_hash_table_destroy (priv->notations);

    G_OBJECT_CLASS (axing_dtd_schema_parent_class)->finalize (object);
}

AxingDtdSchema *
axing_dtd_schema_new (void)
{
    return g_object_new (AXING_TYPE_DTD_SCHEMA, NULL);
}

void
axing_dtd_schema_set_doctype (AxingDtdSchema *dtd,
                              const char     *doctype)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);

    g_return_if_fail (AXING_IS_DTD_SCHEMA (dtd));

    g_free (priv->doctype);
    priv->doctype = g_strdup (doctype);
}

void
axing_dtd_schema_set_public_id (AxingDtdSchema *dtd,
                                const char     *public)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);

    g_return_if_fail (AXING_IS_DTD_SCHEMA (dtd));

    g_free (priv->public);
    priv->public = g_strdup (public);
}

void
axing_dtd_schema_set_system_id (AxingDtdSchema *dtd,
                                const char     *system)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);

    g_return_if_fail (AXING_IS_DTD_SCHEMA (dtd));

    g_free (priv->system);
    priv->system = g_strdup (system);
}

gboolean
axing_dtd_schema_add_element (AxingDtdSchema *dtd,
                              const char     *name,
                              const char     *content,
                              GError        **error)
{
    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);
    /* FIXME */
    return FALSE;
}

gboolean
axing_dtd_schema_add_attlist (AxingDtdSchema *dtd,
                              const char     *name,
                              const char     *attlist,
                              GError        **error)
{
    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);
    /* FIXME */
    return FALSE;
}


gboolean
axing_dtd_schema_add_entity (AxingDtdSchema *dtd,
                             const char     *name,
                             const char     *value)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);

    if (g_hash_table_lookup (priv->general_entities, name))
        return FALSE;

    data = g_new0 (EntityData, 1);
    data->name = g_strdup (name);
    data->value = g_strdup (value);

    g_hash_table_insert (priv->general_entities, data->name, data);
    return TRUE;
}

gboolean
axing_dtd_schema_add_external_entity (AxingDtdSchema *dtd,
                                      const char     *name,
                                      const char     *public,
                                      const char     *system)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);

    if (g_hash_table_lookup (priv->general_entities, name))
        return FALSE;

    data = g_new0 (EntityData, 1);
    data->name = g_strdup (name);
    data->public = g_strdup (public);
    data->system = g_strdup (system);

    g_hash_table_insert (priv->general_entities, data->name, data);
    return TRUE;
}

gboolean
axing_dtd_schema_add_unparsed_entity (AxingDtdSchema *dtd,
                                      const char     *name,
                                      const char     *public,
                                      const char     *system,
                                      const char     *ndata)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);

    if (g_hash_table_lookup (priv->general_entities, name))
        return FALSE;

    data = g_new0 (EntityData, 1);
    data->name = g_strdup (name);
    data->public = g_strdup (public);
    data->system = g_strdup (system);
    data->ndata = g_strdup (ndata);

    g_hash_table_insert (priv->general_entities, data->name, data);
    return TRUE;
}

gboolean
axing_dtd_schema_add_parameter (AxingDtdSchema *dtd,
                                const char     *name,
                                const char     *value)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);

    if (g_hash_table_lookup (priv->parameter_entities, name))
        return FALSE;

    data = g_new0 (EntityData, 1);
    data->name = g_strdup (name);
    data->value = g_strdup (value);

    g_hash_table_insert (priv->parameter_entities, data->name, data);
    return TRUE;
}

gboolean
axing_dtd_schema_add_external_parameter (AxingDtdSchema *dtd,
                                         const char     *name,
                                         const char     *public,
                                         const char     *system)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);

    if (g_hash_table_lookup (priv->parameter_entities, name))
        return FALSE;

    data = g_new0 (EntityData, 1);
    data->name = g_strdup (name);
    data->public = g_strdup (public);
    data->system = g_strdup (system);

    g_hash_table_insert (priv->parameter_entities, data->name, data);
    return TRUE;
}

gboolean
axing_dtd_schema_add_notation (AxingDtdSchema *dtd,
                               const char     *name,
                               const char     *public,
                               const char     *system)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);

    /* FIXME: Unlike with other declarations, redefining a NOTATION
       is a validity error. We cannot error out here, because this
       function is called where we only care about well-formedness.
       Possibly store this dup for later calls to validate().
    */
    if (g_hash_table_lookup (priv->notations, name))
        return FALSE;

    data = g_new0 (EntityData, 1);
    data->name = g_strdup (name);
    data->public = g_strdup (public);
    data->system = g_strdup (system);

    g_hash_table_insert (priv->notations, data->name, data);
    return TRUE;
}

char *
axing_dtd_schema_get_entity (AxingDtdSchema *dtd,
                             const char     *name)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), NULL);

    data = g_hash_table_lookup (priv->general_entities, name);

    if (data == NULL)
        return NULL;

    return g_strdup (data->value);
}

char *
axing_dtd_schema_get_external_entity (AxingDtdSchema *dtd,
                                      const char     *name)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), NULL);

    data = g_hash_table_lookup (priv->general_entities, name);

    if (data == NULL)
        return NULL;

    if (data->ndata != NULL)
        return NULL;

    return g_strdup (data->system);
}

char *
axing_dtd_schema_get_unparsed_entity (AxingDtdSchema *dtd,
                                      const char     *name)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), NULL);

    data = g_hash_table_lookup (priv->general_entities, name);

    if (data == NULL)
        return NULL;

    if (data->ndata == NULL)
        return NULL;

    return g_strdup (data->system);
}

gboolean
axing_dtd_schema_get_entity_full (AxingDtdSchema  *dtd,
                                  const char      *name,
                                  char           **value,
                                  char           **public,
                                  char           **system,
                                  char           **ndata)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), FALSE);

    data = g_hash_table_lookup (priv->general_entities, name);

    if (data == NULL)
        return FALSE;

    *value = g_strdup (data->value);
    *public = g_strdup (data->public);
    *system = g_strdup (data->system);
    *ndata = g_strdup (data->ndata);

    return TRUE;
}

char *
axing_dtd_schema_get_parameter (AxingDtdSchema *dtd,
                                const char     *name)
{
    AxingDtdSchemaPrivate *priv = axing_dtd_schema_get_instance_private (dtd);
    EntityData *data;

    g_return_val_if_fail (AXING_IS_DTD_SCHEMA (dtd), NULL);

    data = g_hash_table_lookup (priv->parameter_entities, name);

    if (data == NULL)
        return NULL;

    return g_strdup (data->value);
}
