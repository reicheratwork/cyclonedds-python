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
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "idl/stream.h"
#include "idl/string.h"
#include "idl/processor.h"

#include "context.h"
#include "naming.h"
#include "types.h"


static idl_retcode_t
emit_module(
    const idl_pstate_t *pstate,
    bool revisit,
    const idl_path_t *path,
    const void *node,
    void *user_data)
{
    idlpy_ctx ctx = (idlpy_ctx) user_data;
    idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;

    if (!revisit)
    {
        ret = idlpy_ctx_enter_module(ctx, idl_identifier(node));
    }
    else
    {
        ret = idlpy_ctx_exit_module(ctx);
    }

    (void)pstate;
    (void)path;

    return ret;
}


/* members with multiple declarators result in multiple members */
static idl_retcode_t
emit_field(
    const idl_pstate_t *pstate,
    bool revisit,
    const idl_path_t *path,
    const void *node,
    void *user_data)
{
    idlpy_ctx ctx = (idlpy_ctx) user_data;

    const char *name = idl_identifier(node);
    char *type = typename(ctx, idl_type_spec(node));

    idlpy_ctx_printf(ctx, "\n    %s: %s", name, type);

    free(type);
    (void)pstate;
    (void)revisit;
    (void)path;

    return IDL_RETCODE_OK;
}


static void struct_decoration(idlpy_ctx ctx, const void *node)
{
    idl_struct_t *_struct = (idl_struct_t*) node;

    idlpy_ctx_printf(ctx, "@dataclass\n");

    if (_struct->keylist) {
        idlpy_ctx_printf(ctx, "@annotate.keylist([");

        idl_key_t* key = _struct->keylist->keys;

        if(key) {
            idlpy_ctx_printf(ctx, "\"%s\"", key->field_name->identifier);
            key++;
        }

        while(key) {
            idlpy_ctx_printf(ctx, ", \"%s\"", key->field_name->identifier);
            key++;
        }

        idlpy_ctx_printf(ctx, "])\n");
    }

    switch(_struct->extensibility) {
        case IDL_EXTENSIBILITY_FINAL:
        idlpy_ctx_printf(ctx, "@annotate.final\n");
            break;
        case IDL_EXTENSIBILITY_APPENDABLE:
        idlpy_ctx_printf(ctx, "@annotate.appendable\n");
            break;
        case IDL_EXTENSIBILITY_MUTABLE:
        idlpy_ctx_printf(ctx, "@annotate.mutable\n");
            break;
        default:
            break;
    }

    switch(_struct->autoid) {
        case IDL_AUTOID_HASH:
            idlpy_ctx_printf(ctx, "@annotate.autoid(\"hash\")\n");
            break;
        case IDL_AUTOID_SEQUENTIAL:
            idlpy_ctx_printf(ctx, "@annotate.autoid(\"sequential\")\n");
            break;
        default:
            break;
    }

    if (_struct->nested.value) {
        idlpy_ctx_printf(ctx, "@annotate.nested\n");
    }
}

static idl_retcode_t
emit_struct(
    const idl_pstate_t *pstate,
    bool revisit,
    const idl_path_t *path,
    const void *node,
    void *user_data)
{
    idlpy_ctx ctx = (idlpy_ctx) user_data;
    idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;

    if (!revisit)
    {
        struct_decoration(ctx, node);
        idlpy_ctx_enter_entity(ctx, idl_identifier(node));
        idlpy_ctx_printf(ctx, "class %s(idl.IdlStruct, typename=\"%s\"):", absolute_name(node));
        ret = IDL_VISIT_REVISIT;
    }
    else
    {
        idlpy_ctx_exit_entity(ctx);
        ret = IDL_RETCODE_OK;
    }

    (void) pstate;
    (void) path;

    return ret;
}


static void union_decoration(idlpy_ctx ctx, const void *node)
{
    idl_union_t *_union = (idl_union_t*) node;

    switch(_union->extensibility) {
        case IDL_EXTENSIBILITY_FINAL:
        idlpy_ctx_printf(ctx, "@annotate.final\n");
            break;
        case IDL_EXTENSIBILITY_APPENDABLE:
        idlpy_ctx_printf(ctx, "@annotate.appendable\n");
            break;
        case IDL_EXTENSIBILITY_MUTABLE:
        idlpy_ctx_printf(ctx, "@annotate.mutable\n");
            break;
        default:
            break;
    }

    if (_union->nested.value) {
        idlpy_ctx_printf(ctx, "@annotate.nested\n");
    }
}

static idl_retcode_t
emit_union(
    const idl_pstate_t *pstate,
    bool revisit,
    const idl_path_t *path,
    const void *node,
    void *user_data)
{
    idlpy_ctx ctx = (idlpy_ctx) user_data;
    idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;

    if (!revisit)
    {
        char *discriminator = typename(ctx, ((idl_union_t*)node)->switch_type_spec->type_spec);
        if (discriminator == NULL) return ret;

        idlpy_ctx_enter_entity(ctx, idl_identifier(node));
        union_decoration(ctx, node);
        idlpy_ctx_printf(ctx, "class %s(idl.IdlUnion, discriminator=%s):", idl_identifier(node), discriminator);
        ret = IDL_VISIT_REVISIT;
        free(discriminator);
    }
    else
    {
        idlpy_ctx_exit_entity(ctx);
        ret = IDL_RETCODE_OK;
    }

    (void)pstate;
    (void)path;

    return ret;
}

static idl_retcode_t
emit_typedef(
    const idl_pstate_t *pstate,
    bool revisit,
    const idl_path_t *path,
    const void *node,
    void *user_data)
{
    idlpy_ctx ctx = (idlpy_ctx) user_data;

    char *type = typename(ctx, idl_type_spec(node));
    if (type == NULL) return IDL_RETCODE_NO_MEMORY;

    idlpy_ctx_enter_entity(ctx, idl_identifier(node));
    idlpy_ctx_printf(ctx, "%s = %s", idl_identifier(node), type);
    idlpy_ctx_exit_entity(ctx);

    free(type);

    (void)revisit;
    (void)path;
    (void)pstate;

    return IDL_VISIT_DONT_RECURSE;
}

static idl_retcode_t
emit_enum(
    const idl_pstate_t *pstate,
    bool revisit,
    const idl_path_t *path,
    const void *node,
    void *user_data)
{
    idlpy_ctx ctx = (idlpy_ctx) user_data;
    idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
    uint32_t skip = 0, value = 0;

    idlpy_ctx_enter_entity(ctx, idl_identifier(node));
    idlpy_ctx_printf(ctx, "class %s(enum):", idl_identifier(node));

    idl_enumerator_t *enumerator = ((const idl_enum_t *)node)->enumerators;
    for (; enumerator; enumerator = idl_next(enumerator))
    {
        const char* fmt;

        char* name = typename(ctx, enumerator);
        value = enumerator->value;

        /* IDL 3.5 did not support fixed enumerator values */
        if (value == skip) // || (pstate->version == IDL35))
            fmt = "    %s = auto()\n";
        else
            fmt = "    %s = %" PRIu32;

        idlpy_ctx_printf(ctx, fmt, name, value);
        free(name);
        skip = value + 1;
    }

    idlpy_ctx_exit_entity(ctx);

    ret = IDL_VISIT_DONT_RECURSE;

    (void)pstate;
    (void)revisit;
    (void)path;

    return ret;
}

static void
print_literal(
    const idl_pstate_t *pstate,
    idlpy_ctx ctx,
    const idl_literal_t *literal)
{
    idl_type_t type;

    switch ((type = idl_type(literal)))
    {
    case IDL_CHAR:
        idlpy_ctx_printf(ctx, "'%c'", literal->value.chr);
        break;
    case IDL_BOOL:
        idlpy_ctx_printf(ctx, "%s", literal->value.bln ? "True" : "False");
        break;
    case IDL_INT8:
        idlpy_ctx_printf(ctx, "%" PRId8, literal->value.int8);
        break;
    case IDL_OCTET:
    case IDL_UINT8:
        idlpy_ctx_printf(ctx, "%" PRIu8, literal->value.uint8);
        break;
    case IDL_SHORT:
    case IDL_INT16:
        idlpy_ctx_printf(ctx, "%" PRId16, literal->value.int16);
        break;
    case IDL_USHORT:
    case IDL_UINT16:
        idlpy_ctx_printf(ctx, "%" PRIu16, literal->value.uint16);
        break;
    case IDL_LONG:
    case IDL_INT32:
        idlpy_ctx_printf(ctx, "%" PRId32, literal->value.int32);
        break;
    case IDL_ULONG:
    case IDL_UINT32:
        idlpy_ctx_printf(ctx, "%" PRIu32, literal->value.uint32);
        break;
    case IDL_LLONG:
    case IDL_INT64:
        idlpy_ctx_printf(ctx, "%" PRId64, literal->value.int64);
        break;
    case IDL_ULLONG:
    case IDL_UINT64:
        idlpy_ctx_printf(ctx, "%" PRIu64, literal->value.uint64);
        break;
    case IDL_FLOAT:
        idlpy_ctx_printf(ctx, "%.6f", literal->value.flt);
        break;
    case IDL_DOUBLE:
        idlpy_ctx_printf(ctx, "%f", literal->value.dbl);
        break;
    case IDL_LDOUBLE:
        idlpy_ctx_printf(ctx, "%lf", literal->value.ldbl);
        break;
    case IDL_STRING:
        idlpy_ctx_printf(ctx, "\"%s\"", literal->value.str);
        break;
    default:
    {
        char *name;
        assert(type == IDL_ENUM);
        name = typename(ctx, literal);
        idlpy_ctx_printf(ctx, "%s", name);
        free(name);
    }
    }
    (void)pstate;
}

static idl_retcode_t
emit_const(
    const idl_pstate_t *pstate,
    bool revisit,
    const idl_path_t *path,
    const void *node,
    void *user_data)
{
    idlpy_ctx ctx = (idlpy_ctx) user_data;

    char *type = typename(ctx, node);
    if (type == NULL) return IDL_RETCODE_NO_MEMORY;

    const idl_literal_t *literal = ((const idl_const_t *) node)->const_expr;

    idlpy_ctx_enter_entity(ctx, idl_identifier(node));
    idlpy_ctx_printf(ctx, "%s = ", type);
    print_literal(pstate, ctx, literal);
    idlpy_ctx_write(ctx, "\n\n");
    idlpy_ctx_exit_entity(ctx);

    free(type);
    (void)revisit;
    (void)path;

    return IDL_RETCODE_OK;
}


idl_retcode_t generate_types(const idl_pstate_t *pstate, idlpy_ctx ctx)
{
    idl_retcode_t ret;
    idl_visitor_t visitor;

    memset(&visitor, 0, sizeof(visitor));
    visitor.visit = IDL_CONST | IDL_TYPEDEF | IDL_STRUCT | IDL_UNION | IDL_ENUM | IDL_DECLARATOR | IDL_MODULE;
    visitor.accept[IDL_ACCEPT_MODULE] = &emit_module;
    visitor.accept[IDL_ACCEPT_CONST] = &emit_const;
    visitor.accept[IDL_ACCEPT_TYPEDEF] = &emit_typedef;
    visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
    visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
    visitor.accept[IDL_ACCEPT_ENUM] = &emit_enum;
    visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_field;
    visitor.sources = (const char *[]){pstate->sources->path->name, NULL};
    if ((ret = idl_visit(pstate, pstate->root, &visitor, ctx)))
        return ret;
    return IDL_RETCODE_OK;
}
