/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "context.h"
#include "util.h"

#include "idl/string.h"
#include "idl/tree.h"
#include "idl/stream.h"
#include "idl/file.h"
#include "idl/version.h"
#include "idl/visit.h"


/// * IDL context * ///
typedef struct idlpy_module_ctx_s *idlpy_module_ctx;
struct idlpy_module_ctx_s
{
    /// Parent
    idlpy_module_ctx parent;

    /// Strings
    char* name;
    char* path;
    char* fullname;
};

typedef struct idlpy_entity_ctx_s
{
    char* name;
    FILE* fp;
} *idlpy_entity_ctx;

struct idlpy_ctx_s
{
    idlpy_module_ctx module;
    idlpy_module_ctx root_module;
    idlpy_entity_ctx entity;
    char* basepath;
};

idlpy_ctx idlpy_ctx_new(const char *path)
{
    idlpy_ctx ctx = (idlpy_ctx)malloc(sizeof(struct idlpy_ctx_s));
    if (ctx == NULL) return NULL;

    ctx->basepath = idlpy_strdup(path);
    ctx->module = NULL;
    ctx->root_module = NULL;
    ctx->entity = NULL;

    if (ctx->basepath == NULL) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void idlpy_ctx_free(idlpy_ctx octx)
{
    assert(octx);
    assert(octx->basepath);
    assert(octx->module == NULL);
    assert(octx->root_module == NULL);
    assert(octx->entity == NULL);

    free(octx->basepath);
    free(octx);
}

static idlpy_module_ctx idlpy_module_ctx_new()
{
    idlpy_module_ctx ctx = (idlpy_module_ctx) malloc(sizeof(struct idlpy_module_ctx_s));

    if (ctx == NULL) return NULL;

    ctx->parent = NULL;
    ctx->name = NULL;
    ctx->path = NULL;
    ctx->fullname = NULL;

    return ctx;
}

static void idlpy_module_ctx_free(idlpy_module_ctx ctx)
{
    assert(ctx);

    if (ctx->name) free(ctx->name);
    if (ctx->path) free(ctx->path);
    if (ctx->fullname) free(ctx->fullname);

    free(ctx);
}

static idlpy_entity_ctx idlpy_entity_ctx_new()
{
    idlpy_entity_ctx ctx = (idlpy_entity_ctx) malloc(sizeof(struct idlpy_entity_ctx_s));

    if (ctx == NULL) return NULL;

    ctx->name = NULL;
    ctx->fp = NULL;

    return ctx;
}

static void idlpy_entity_ctx_free(idlpy_entity_ctx ctx)
{
    assert(ctx);
    assert(ctx->fp == NULL);

    if (ctx->name != NULL) free(ctx->name);
    free(ctx);
}

idl_retcode_t idlpy_ctx_enter_module(idlpy_ctx octx, const char *name)
{
    assert(octx);
    assert(name);

    idlpy_module_ctx ctx = idlpy_module_ctx_new();
    idl_retcode_t ret = IDL_VISIT_REVISIT;

    if (ctx == NULL) {
        return IDL_RETCODE_NO_MEMORY;
    }

    ctx->name = idlpy_strdup(name);

    if (ctx->name == NULL) {
        ret = IDL_RETCODE_NO_MEMORY;
        goto err;
    }

    ctx->parent = octx->module;
    octx->module = ctx;

    if (ctx->parent == NULL) {
        size_t s_base_path = strlen(octx->basepath);
        size_t s_name = strlen(ctx->name);

        ctx->fullname = idlpy_strdup(ctx->name);
        octx->root_module = ctx;

        if (ctx->fullname == NULL) {
            idlpy_module_ctx_free(ctx);
            return IDL_RETCODE_NO_MEMORY;
        }

        ctx->path = (char*) malloc(s_base_path + s_name + 1);
        if (!ctx->path) {
            idlpy_ctx_report_error(octx, "Could not format path to module.");
            ret = IDL_RETCODE_NO_MEMORY;
            goto err;
        }
        idl_snprintf(ctx->path, s_base_path + s_name + 1, "%s%s", octx->basepath, ctx->name);
    } else {
        size_t s_parent_fullname = strlen(ctx->parent->fullname);
        size_t s_parent_path = strlen(ctx->parent->path);
        size_t s_name = strlen(ctx->name);

        ctx->fullname = (char*) malloc(s_parent_fullname + s_name + 2);
        ctx->path = (char*) malloc(s_parent_path + s_name + 2);

        if (!ctx->fullname) {
            idlpy_ctx_report_error(octx, "Could not format fullname of module.");
            ret = IDL_RETCODE_NO_MEMORY;
            goto err;
        }
        if (!ctx->path) {
            idlpy_ctx_report_error(octx, "Could not format path of module.");
            ret = IDL_RETCODE_NO_MEMORY;
            goto err;
        }
        idl_snprintf(ctx->fullname, s_parent_fullname + s_name + 2, "%s.%s", ctx->parent->fullname, ctx->name);
        idl_snprintf(ctx->path, s_parent_path + s_name + 2, "%s/%s", ctx->parent->path, ctx->name);
    }

    mkdir(ctx->path, 0775);
err:
    if (ret != IDL_VISIT_REVISIT) {
        idlpy_module_ctx_free(ctx);
    }

    return ret;
}

static idl_retcode_t write_module_headers(FILE *fh)
{
    static const char *fmt =
        "\"\"\"\n"
        "  Generated by Eclipse Cyclone DDS idlc Python Backend\n"
        "  Cyclone DDS IDL version: v%s\n"
        "\n"
        "\"\"\"\n"
        "\n"
        "from cyclonedds.idl.module_helper import get_idl_entities\n"
        "\n"
        "__idl_module__ = True\n"
        "__all__ = get_idl_entities(__name__)\n";


    idlpy_fprintf(fh, fmt, IDL_VERSION);

    return IDL_RETCODE_OK;
}

static void write_entity_headers(idlpy_ctx octx)
{
    assert(octx);
    assert(octx->module);
    assert(octx->module->fullname);
    assert(octx->entity);
    assert(octx->entity->fp);
    assert(octx->entity->name);

    static const char *fmt =
        "\"\"\"\n"
        "  Generated by Eclipse Cyclone DDS idlc Python Backend\n"
        "  Cyclone DDS IDL version: v%s\n"
        "  Module: %s\n"
        "  Entity: %s\n"
        "\n"
        "\"\"\"\n"
        "\n"
        "from dataclasses import dataclass\n\n"
        "import cyclonedds.idl as idl\n"
        "import cyclonedds.idl.annotations as annotate\n"
        "import cyclonedds.idl.types as types\n\n"
        "import %s\n"
        "from . import __idl_module__\n\n";

    idlpy_fprintf(octx->entity->fp, fmt, IDL_VERSION, octx->module->fullname, octx->entity->name, octx->root_module->fullname);
}

void idlpy_ctx_consume(idlpy_ctx octx, char *data)
{
    assert(octx);

    idlpy_ctx_printf(octx, data);
    free(data);
}

void idlpy_ctx_write(idlpy_ctx octx, const char *data)
{
    assert(octx);

    idlpy_ctx_printf(octx, data);
}


void idlpy_ctx_printf(idlpy_ctx octx, const char *fmt, ...)
{
    assert(octx);
    assert(octx->entity);
    assert(octx->entity->fp);

    va_list ap, cp;
    va_start(ap, fmt);
    va_copy(cp, ap);

    idlpy_vfprintf(octx->entity->fp, fmt, ap);

    va_end(ap);
    va_end(cp);
}

idl_retcode_t idlpy_ctx_exit_module(idlpy_ctx octx)
{
    assert(octx);
    assert(octx->module);
    assert(octx->module->path);

    FILE *file;
    char* init_path;
    idlpy_module_ctx ctx = octx->module;
    idl_retcode_t ret = IDL_RETCODE_OK;
    size_t s_path = strlen(ctx->path);

    init_path = (char*) malloc(s_path + strlen("/__init__.py") + 1);

    if (!init_path) {
        idlpy_ctx_report_error(octx, "Could not construct path for module init writing");
        ret = IDL_RETCODE_NO_MEMORY;
        goto path_err;
    }
    idl_snprintf(init_path, s_path + strlen("/__init__.py") + 1, "%s/__init__.py", ctx->path);

    file = idlpy_open_file(init_path, "w");

    if (file == NULL) {
        idlpy_ctx_report_error(octx, "Failed to open file for writing.");
        ret = IDL_RETCODE_NO_ACCESS;
        goto file_err;
    }

    write_module_headers(file);
    fclose(file);

file_err:

    free(init_path);

path_err:

    octx->module = ctx->parent;
    if (ctx->parent == NULL) octx->root_module = NULL;

    idlpy_module_ctx_free(ctx);
    return ret;
}

idl_retcode_t idlpy_ctx_enter_entity(idlpy_ctx octx, const char* name)
{
    assert(octx);
    assert(octx->entity == NULL);
    assert(octx->module);

    size_t s_module_path = strlen(octx->module->path);
    size_t s_name = strlen(name);

    char *path = (char*) malloc(s_module_path + s_name + 5);
    if (!path) {
        idlpy_ctx_report_error(octx, "Could not construct path for entity writing.");
        return IDL_RETCODE_NO_MEMORY;
    }
    idl_snprintf(path, s_module_path + s_name + 5, "%s/%s.py", octx->module->path, name);

    octx->entity = idlpy_entity_ctx_new();
    if (octx->entity == NULL) {
        free(path);
        idlpy_ctx_report_error(octx, "Could not construct new entity context.");
        return IDL_RETCODE_NO_MEMORY;
    }

    octx->entity->name = idlpy_strdup(name);
    if (octx->entity->name == NULL) {
        free(path);
        idlpy_entity_ctx_free(octx->entity);
        idlpy_ctx_report_error(octx, "Failed to duplicate name string.");
        return IDL_RETCODE_NO_MEMORY;
    }

    octx->entity->fp = idlpy_open_file(path, "w");

    if (octx->entity->fp == NULL) {
        idlpy_entity_ctx_free(octx->entity);
        idlpy_ctx_report_error(octx, "Failed to open file for writing.");
        return IDL_RETCODE_NO_MEMORY;
    }

    free(path);
    write_entity_headers(octx);
    return IDL_RETCODE_OK;
}

idl_retcode_t idlpy_ctx_exit_entity(idlpy_ctx octx)
{
    assert(octx);
    assert(octx->entity);
    assert(octx->entity->fp);

    idlpy_ctx_write(octx, "\n");
    fclose(octx->entity->fp);
    octx->entity->fp = NULL;
    idlpy_entity_ctx_free(octx->entity);
    octx->entity = NULL;

    return IDL_RETCODE_OK;
}

void idlpy_ctx_report_error(idlpy_ctx ctx, const char* error)
{
    fprintf(stderr, "[ERROR] Module %s: %s\n", ctx->module->fullname, error);
}
