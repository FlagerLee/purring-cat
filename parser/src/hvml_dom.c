// This file is a part of Purring Cat, a reference implementation of HVML.
//
// Copyright (C) 2020, <freemine@yeah.net>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "hvml/hvml_dom.h"

#include "hvml/hvml_jo.h"
#include "hvml/hvml_json_parser.h"
#include "hvml/hvml_list.h"
#include "hvml/hvml_parser.h"
#include "hvml/hvml_string.h"

#include <ctype.h>
#include <string.h>

// for easy coding
#define DOM_MEMBERS() \
    HLIST_MEMBERS(hvml_dom_t, hvml_dom_t, _dom_); \
    HNODE_MEMBERS(hvml_dom_t, hvml_dom_t, _dom_)
#define DOM_NEXT(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _dom_, next)
#define DOM_PREV(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _dom_, prev)
#define DOM_OWNER(v)         (v)->MKM(hvml_dom_t, hvml_dom_t, _dom_, owner)
#define DOM_HEAD(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _dom_, head)
#define DOM_TAIL(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _dom_, tail)
#define DOM_COUNT(v)         (v)->MKM(hvml_dom_t, hvml_dom_t, _dom_, count)
#define DOM_IS_ORPHAN(v)     HNODE_IS_ORPHAN(hvml_dom_t, hvml_dom_t, _dom_, v)
#define DOM_IS_EMPTY(v)      HLIST_IS_EMPTY(hvml_dom_t, hvml_dom_t, _dom_, v)
#define DOM_APPEND(ov,v)     HLIST_APPEND(hvml_dom_t, hvml_dom_t, _dom_, ov, v)
#define DOM_REMOVE(v)        HLIST_REMOVE(hvml_dom_t, hvml_dom_t, _dom_, v)

#define DOM_ATTR_MEMBERS() \
    HLIST_MEMBERS(hvml_dom_t, hvml_dom_t, _attr_); \
    HNODE_MEMBERS(hvml_dom_t, hvml_dom_t, _attr_)
#define DOM_ATTR_NEXT(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _attr_, next)
#define DOM_ATTR_PREV(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _attr_, prev)
#define DOM_ATTR_OWNER(v)         (v)->MKM(hvml_dom_t, hvml_dom_t, _attr_, owner)
#define DOM_ATTR_HEAD(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _attr_, head)
#define DOM_ATTR_TAIL(v)          (v)->MKM(hvml_dom_t, hvml_dom_t, _attr_, tail)
#define DOM_ATTR_COUNT(v)         (v)->MKM(hvml_dom_t, hvml_dom_t, _attr_, count)
#define DOM_ATTR_IS_ORPHAN(v)     HNODE_IS_ORPHAN(hvml_dom_t, hvml_dom_t, _attr_, v)
#define DOM_ATTR_IS_EMPTY(v)      HLIST_IS_EMPTY(hvml_dom_t, hvml_dom_t, _attr_, v)
#define DOM_ATTR_APPEND(ov,v)     HLIST_APPEND(hvml_dom_t, hvml_dom_t, _attr_, ov, v)
#define DOM_ATTR_REMOVE(v)        HLIST_REMOVE(hvml_dom_t, hvml_dom_t, _attr_, v)

typedef struct hvml_dom_tag_s               hvml_dom_tag_t;
typedef struct hvml_dom_attr_s              hvml_dom_attr_t;
typedef struct hvml_dom_text_s              hvml_dom_text_t;

struct hvml_dom_tag_s {
    hvml_string_t       name;
};

struct hvml_dom_attr_s {
    hvml_string_t       key;
    hvml_string_t       val;
};

struct hvml_dom_text_s {
    hvml_string_t       txt;
};

struct hvml_dom_s {
    HVML_DOM_TYPE       dt;

    union {
        hvml_dom_tag_t    tag;
        hvml_dom_attr_t   attr;
        hvml_dom_text_t   txt;
        hvml_jo_value_t  *jo;
    };

    DOM_ATTR_MEMBERS();
    DOM_MEMBERS();
};

struct hvml_dom_gen_s {
    hvml_dom_t          *dom;
    hvml_dom_t          *root;
    hvml_parser_t       *parser;
    hvml_jo_value_t     *jo;
};

hvml_dom_t* hvml_dom_create() {
    hvml_dom_t *dom = (hvml_dom_t*)calloc(1, sizeof(*dom));
    if (!dom) return NULL;

    return dom;
}

void hvml_dom_destroy(hvml_dom_t *dom) {
    hvml_dom_detach(dom);

    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            hvml_string_clear(&dom->tag.name);
        } break;
        case MKDOT(D_ATTR):
        {
            hvml_string_clear(&dom->attr.key);
            hvml_string_clear(&dom->attr.val);
        } break;
        case MKDOT(D_TEXT):
        {
            hvml_string_clear(&dom->txt.txt);
        } break;
        case MKDOT(D_JSON):
        {
            hvml_jo_value_free(dom->jo);
            dom->jo = NULL;
        } break;
        default:
        {
            A(0, "internal logic error");
        } break;
    }

    hvml_dom_t *d = DOM_HEAD(dom);
    while (d) {
        hvml_dom_destroy(d);
        d = DOM_HEAD(dom);
    }

    d = DOM_ATTR_HEAD(dom);
    while (d) {
        hvml_dom_destroy(d);
        d = DOM_ATTR_HEAD(dom);
    }

    free(dom);
}

hvml_dom_t* hvml_dom_append_attr(hvml_dom_t *dom, const char *key, size_t key_len, const char *val, size_t val_len) {
    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            hvml_dom_t *v      = hvml_dom_create();
            if (!v) return NULL;
            v->dt              = MKDOT(D_ATTR);
            do {
                int ret = hvml_string_set(&v->attr.key, key, key_len);
                if (ret) break;
                if (val) {
                    ret = hvml_string_set(&v->attr.val, val, val_len);
                    if (ret) break;
                }
                DOM_APPEND(dom, v);
                return v;
            } while (0);
            hvml_dom_destroy(v);
            return NULL;
        } break;
        case MKDOT(D_ATTR):
        {
            E("not allowed for attribute node");
            return NULL;
        } break;
        case MKDOT(D_TEXT):
        {
            E("not allowed for content node");
            return NULL;
        } break;
        default:
        {
            A(0, "internal logic error");
            return NULL;
        } break;
    }
}

hvml_dom_t* hvml_dom_set_val(hvml_dom_t *dom, const char *val, size_t val_len) {
    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            E("not allowed for tag node");
            return NULL;
        } break;
        case MKDOT(D_ATTR):
        {
            do {
                int ret = hvml_string_set(&dom->attr.val, val, val_len);
                if (ret) break;
                return dom;
            } while (0);
            return NULL;
        } break;
        case MKDOT(D_TEXT):
        {
            E("not allowed for content node");
            return NULL;
        } break;
        default:
        {
            A(0, "internal logic error");
            return NULL;
        } break;
    }
}

hvml_dom_t* hvml_dom_append_content(hvml_dom_t *dom, const char *txt, size_t len) {
    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            hvml_dom_t *v      = hvml_dom_create();
            if (!v) return NULL;
            v->dt              = MKDOT(D_TEXT);
            do {
                int ret = hvml_string_set(&v->txt.txt, txt, len);
                if (ret) break;
                DOM_APPEND(dom, v);
                return v;
            } while (0);
            hvml_dom_destroy(v);
            return NULL;
        } break;
        case MKDOT(D_ATTR):
        {
            E("not allowed for attribute node");
            return NULL;
        } break;
        case MKDOT(D_TEXT):
        {
            E("not allowed for content node");
            return NULL;
        } break;
        default:
        {
            A(0, "internal logic error");
            return NULL;
        } break;
    }
}

hvml_dom_t* hvml_dom_add_tag(hvml_dom_t *dom, const char *tag, size_t len) {
    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            hvml_dom_t *v      = hvml_dom_create();
            if (!v) return NULL;
            v->dt              = MKDOT(D_TAG);
            do {
                int ret = hvml_string_set(&v->tag.name, tag, len);
                if (ret) break;
                DOM_APPEND(dom, v);
                return v;
            } while (0);
            hvml_dom_destroy(v);
            return NULL;
        } break;
        case MKDOT(D_ATTR):
        {
            E("not allowed for attribute node");
            return NULL;
        } break;
        case MKDOT(D_TEXT):
        {
            E("not allowed for content node");
            return NULL;
        } break;
        default:
        {
            A(0, "internal logic error");
            return NULL;
        } break;
    }
}

hvml_dom_t* hvml_dom_append_json(hvml_dom_t *dom, hvml_jo_value_t *jo) {
    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            A(hvml_jo_value_parent(jo)==NULL, "internal logic error");
            hvml_dom_t *v      = hvml_dom_create();
            if (!v) return NULL;
            v->dt              = MKDOT(D_JSON);
            v->jo              = jo;
            DOM_APPEND(dom, v);
            return NULL;
        } break;
        case MKDOT(D_ATTR):
        {
            E("not allowed for attribute node");
            A(0, ".");
            return NULL;
        } break;
        case MKDOT(D_TEXT):
        {
            E("not allowed for content node");
            A(0, ".");
            return NULL;
        } break;
        default:
        {
            A(0, "internal logic error");
            return NULL;
        } break;
    }
}

hvml_dom_t* hvml_dom_root(hvml_dom_t *dom) {
    while (DOM_OWNER(dom)) {
        dom = DOM_OWNER(dom);
    }
    return dom;
}

hvml_dom_t* hvml_dom_parent(hvml_dom_t *dom) {
    return DOM_OWNER(dom);
}

hvml_dom_t* hvml_dom_next(hvml_dom_t *dom) {
    return DOM_NEXT(dom);
}

hvml_dom_t* hvml_dom_prev(hvml_dom_t *dom) {
    return DOM_PREV(dom);
}

void hvml_dom_detach(hvml_dom_t *dom) {
    if (DOM_OWNER(dom)) {
        DOM_REMOVE(dom);
    }
    if (DOM_ATTR_OWNER(dom)) {
        DOM_ATTR_REMOVE(dom);
    }
}

hvml_dom_t* hvml_dom_select(hvml_dom_t *dom, const char *selector) {
    A(0, "not implemented yet");
}

void hvml_dom_str_serialize(const char *str, size_t len, FILE *out) {
    const char *p = str;
    for (size_t i=0; i<len; ++i, ++p) {
        const char c = *p;
        switch (c) {
            case '&':  { fprintf(out, "&amp;");   break; }
            case '<':  {
                if (i<len-1 && !isspace(p[1])) {
                    fprintf(out, "&lt;");
                } else {
                    fprintf(out, "%c", c);
                }
            } break;
            default:   { fprintf(out, "%c", c);   break; }
        }
    }
}

void hvml_dom_attr_val_serialize(const char *str, size_t len, FILE *out) {
    const char *p = str;
    for (size_t i=0; i<len; ++i, ++p) {
        const char c = *p;
        switch (c) {
            case '&':  { fprintf(out, "&amp;");   break; }
            case '"':  { fprintf(out, "&quot;");  break; }
            default:   { fprintf(out, "%c", c);   break; }
        }
    }
}

void hvml_dom_printf(hvml_dom_t *dom, FILE *out) {
    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            fprintf(out, "<");
            fprintf(out, "%s", dom->tag.name.str);
            hvml_dom_t *attr = DOM_ATTR_HEAD(dom);
            while (attr) {
                fprintf(out, " ");
                hvml_dom_printf(attr, out);
                attr = DOM_ATTR_NEXT(attr);
            }
            hvml_dom_t *child = DOM_HEAD(dom);
            if (!child) {
                fprintf(out, "/>");
            } else {
                fprintf(out, ">");
                while (child) {
                    hvml_dom_printf(child, out);
                    child = DOM_NEXT(child);
                }
                fprintf(out, "</");
                fprintf(out, "%s", dom->tag.name.str);
                fprintf(out, ">");
            }
        } break;
        case MKDOT(D_ATTR):
        {
            fprintf(out, "%s", dom->attr.key.str);
            if (dom->attr.val.str) {
                fprintf(out, ":\"");
                hvml_dom_attr_val_serialize(dom->attr.val.str, dom->attr.val.len, out);
                fprintf(out, "\"");
            }
        } break;
        case MKDOT(D_TEXT):
        {
            hvml_dom_str_serialize(dom->txt.txt.str, dom->txt.txt.len, out);
        } break;
        case MKDOT(D_JSON):
        {
            hvml_jo_value_printf(dom->jo, out);
        } break;
        default:
        {
            A(0, "internal logic error");
        } break;
    }
}

void hvml_dom_traverse(hvml_dom_t *dom, FILE *out, traverse_callback *out_funcs) {
    switch (dom->dt) {
        case MKDOT(D_TAG):
        {
            //fprintf(out, "<");
            //fprintf(out, "%s", dom->tag.name.str);
            out_funcs->out_dom_head(out, dom->tag.name.str);
            hvml_dom_t *attr = DOM_ATTR_HEAD(dom);
            while (attr) {
                //fprintf(out, " ");
                out_funcs->out_attr_separator(out);
                hvml_dom_printf(attr, out);
                attr = DOM_ATTR_NEXT(attr);
            }
            hvml_dom_t *child = DOM_HEAD(dom);
            if (!child) {
                //fprintf(out, "/>");
                out_funcs->out_simple_close(out);
            } else {
                //fprintf(out, ">");
                out_funcs->out_tag_close(out);
                while (child) {
                    hvml_dom_close(child, out);
                    child = DOM_NEXT(child);
                }
                //fprintf(out, "</");
                //fprintf(out, "%s", dom->tag.name.str);
                //fprintf(out, ">");
                out_funcs->out_dom_close(out, dom->tag.name.str);
            }
        } break;
        case MKDOT(D_ATTR):
        {
            //fprintf(out, "%s", dom->attr.key.str);
            out_funcs->out_attr_key(out, dom->attr.key.str);
            if (dom->attr.val.str) {
                //fprintf(out, ":\"");
                //hvml_dom_attr_val_serialize(dom->attr.val.str, dom->attr.val.len, out);
                //fprintf(out, "\"");
                out_funcs->out_attr_val(dom->attr.val.str, dom->attr.val.len, out);
            }
        } break;
        case MKDOT(D_TEXT):
        {
            //hvml_dom_str_serialize(dom->txt.txt.str, dom->txt.txt.len, out);
            out_funcs->out_dom_string(dom->txt.txt.str, dom->txt.txt.len, out);
        } break;
        case MKDOT(D_JSON):
        {
            //hvml_jo_value_printf(dom->jo, out);
            out_funcs->out_json_value(dom->jo, out);
        } break;
        default:
        {
            A(0, "internal logic error");
        } break;
    }
}

static int on_open_tag(void *arg, const char *tag);
static int on_attr_key(void *arg, const char *key);
static int on_attr_val(void *arg, const char *val);
static int on_close_tag(void *arg);
static int on_text(void *arg, const char *txt);

static int on_begin(void *arg);
static int on_open_array(void *arg);
static int on_close_array(void *arg);
static int on_open_obj(void *arg);
static int on_close_obj(void *arg);
static int on_key(void *arg, const char *key, size_t len);
static int on_true(void *arg);
static int on_false(void *arg);
static int on_null(void *arg);
static int on_string(void *arg, const char *val, size_t len);
static int on_integer(void *arg, const char *origin, int64_t val);
static int on_double(void *arg, const char *origin, double val);
static int on_end(void *arg);

hvml_dom_gen_t* hvml_dom_gen_create() {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)calloc(1, sizeof(*gen));
    if (!gen) return NULL;

    hvml_parser_conf_t conf = {0};

    conf.on_open_tag      = on_open_tag;
    conf.on_attr_key      = on_attr_key;
    conf.on_attr_val      = on_attr_val;
    conf.on_close_tag     = on_close_tag;
    conf.on_text          = on_text;

    conf.on_begin         = on_begin;
    conf.on_open_array    = on_open_array;
    conf.on_close_array   = on_close_array;
    conf.on_open_obj      = on_open_obj;
    conf.on_close_obj     = on_close_obj;
    conf.on_key           = on_key;
    conf.on_true          = on_true;
    conf.on_false         = on_false;
    conf.on_null          = on_null;
    conf.on_string        = on_string;
    conf.on_integer       = on_integer;
    conf.on_double        = on_double;
    conf.on_end           = on_end;

    conf.arg                    = gen;


    gen->parser = hvml_parser_create(conf);
    if (!gen->parser) {
        hvml_dom_gen_destroy(gen);
        return NULL;
    }

    return gen;
}

void hvml_dom_gen_destroy(hvml_dom_gen_t *gen) {
    if (gen->dom) {
        hvml_dom_t *root = hvml_dom_root(gen->dom);
        hvml_dom_destroy(root);
        gen->dom = NULL;
    }

    if (gen->parser) {
        hvml_parser_destroy(gen->parser);
        gen->parser = NULL;
    }

    if (gen->jo) {
        hvml_jo_value_free(gen->jo);
        gen->jo = NULL;
    }

    free(gen);
}

int hvml_dom_gen_parse_char(hvml_dom_gen_t *gen, const char c) {
    return hvml_parser_parse_char(gen->parser, c);
}

int hvml_dom_gen_parse(hvml_dom_gen_t *gen, const char *buf, size_t len) {
    return hvml_parser_parse(gen->parser, buf, len);
}

int hvml_dom_gen_parse_string(hvml_dom_gen_t *gen, const char *str) {
    return hvml_parser_parse_string(gen->parser, str);
}

hvml_dom_t* hvml_dom_gen_parse_end(hvml_dom_gen_t *gen) {
    if (hvml_parser_parse_end(gen->parser)) {
        return NULL;
    }
    hvml_dom_t *dom   = gen->dom;
    gen->dom          = NULL;

    return dom;
}

hvml_dom_t* hvml_dom_load_from_stream(FILE *in) {
    hvml_dom_gen_t *gen = hvml_dom_gen_create();
    if (!gen) return NULL;

    char buf[4096] = {0};
    int  n         = 0;
    int  ret       = 0;

    while ( (n=fread(buf, 1, sizeof(buf), in))>0) {
        ret = hvml_dom_gen_parse(gen, buf, n);
        if (ret) break;
    }
    hvml_dom_t *dom = hvml_dom_gen_parse_end(gen);
    hvml_dom_gen_destroy(gen);

    if (ret==0) {
        return dom;
    }
    if (dom) hvml_dom_destroy(dom);
    return NULL;
}




static int on_open_tag(void *arg, const char *tag) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    hvml_dom_t *v       = hvml_dom_create();
    if (!v) return -1;
    v->dt      = MKDOT(D_TAG);
    if (hvml_string_set(&v->tag.name, tag, strlen(tag))) {
        hvml_dom_destroy(v);
        return -1;
    }
    if (!gen->dom) {
        gen->dom = v;
        return 0;
    }
    if (gen->dom->dt == MKDOT(D_ATTR)) {
        DOM_APPEND(DOM_ATTR_OWNER(gen->dom), v);
    } else {
        A(gen->dom->dt == MKDOT(D_TAG), "internal logic error");
        DOM_APPEND(gen->dom, v);
    }
    gen->dom = v;
    return 0;
}

static int on_attr_key(void *arg, const char *key) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    hvml_dom_t *v       = hvml_dom_create();
    if (!v) return -1;
    v->dt      = MKDOT(D_ATTR);
    if (hvml_string_set(&v->attr.key, key, strlen(key))) {
        hvml_dom_destroy(v);
        return -1;
    }
    A(gen->dom, "internal logic error");
    if (gen->dom->dt == MKDOT(D_ATTR)) {
        DOM_APPEND(DOM_ATTR_OWNER(gen->dom), v);
        DOM_ATTR_APPEND(DOM_ATTR_OWNER(gen->dom), v);
    } else {
        A(gen->dom->dt == MKDOT(D_TAG), "internal logic error");
        DOM_ATTR_APPEND(gen->dom, v);
    }
    gen->dom = v;
    A(gen->dom->dt == MKDOT(D_ATTR), "internal logic error");
    return 0;
}

static int on_attr_val(void *arg, const char *val) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    A(gen->dom, "internal logic error");
    A(gen->dom->dt == MKDOT(D_ATTR), "internal logic error");
    if (hvml_string_set(&gen->dom->attr.val, val, strlen(val))) {
        return -1;
    }
    gen->dom = DOM_ATTR_OWNER(gen->dom);
    A(gen->dom, "internal logic error");
    A(gen->dom->dt == MKDOT(D_TAG), "internal logic error");
    return 0;
}

static int on_close_tag(void *arg) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    if (gen->dom->dt == MKDOT(D_ATTR)) {
        gen->dom = DOM_ATTR_OWNER(gen->dom);
        A(gen->dom->dt == MKDOT(D_TAG), "internal logic error");
    }
    A(gen->dom->dt == MKDOT(D_TAG), "internal logic error");
    if (DOM_OWNER(gen->dom)) {
        gen->dom = DOM_OWNER(gen->dom);
        A(gen->dom->dt == MKDOT(D_TAG), "internal logic error");
        return 0;
    }
    gen->root = gen->dom;
    return 0;
}

static int on_text(void *arg, const char *txt) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    A(gen->root == NULL, "internal logic error");
    hvml_dom_t *v       = hvml_dom_create();
    if (!v) return -1;
    v->dt      = MKDOT(D_TEXT);
    if (hvml_string_set(&v->txt.txt, txt, strlen(txt))) {
        hvml_dom_destroy(v);
        return -1;
    }
    if (gen->dom->dt == MKDOT(D_ATTR)) {
        DOM_APPEND(DOM_ATTR_OWNER(gen->dom), v);
    } else {
        A(gen->dom->dt == MKDOT(D_TAG), "internal logic error");
        DOM_APPEND(gen->dom, v);
    }
    gen->dom = DOM_OWNER(v);
    return 0;
}

static int on_begin(void *arg) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    return 0;
}

static int on_open_array(void *arg) {
    hvml_jo_value_t *jo = hvml_jo_array();
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    A(gen, "internal logic error");

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    return 0;
}

static int on_close_array(void *arg) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_open_obj(void *arg) {
    hvml_jo_value_t *jo = hvml_jo_object();
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    A(gen, "internal logic error");

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    return 0;
}

static int on_close_obj(void *arg) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_key(void *arg, const char *key, size_t len) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    A(hvml_jo_value_type(gen->jo) == MKJOT(J_OBJECT), "internal logic error");

    hvml_jo_value_t *jo = hvml_jo_object_kv(key, len);
    if (!jo) return -1;

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    return 0;
}

static int on_true(void *arg) {
    hvml_jo_value_t *jo = hvml_jo_true();
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_false(void *arg) {
    hvml_jo_value_t *jo = hvml_jo_false();
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_null(void *arg) {
    hvml_jo_value_t *jo = hvml_jo_null();
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_string(void *arg, const char *val, size_t len) {
    hvml_jo_value_t *jo = hvml_jo_string(val, len);
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_integer(void *arg, const char *origin, int64_t val) {
    hvml_jo_value_t *jo = hvml_jo_integer(val, origin);
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_double(void *arg, const char *origin, double val) {
    hvml_jo_value_t *jo = hvml_jo_double(val, origin);
    if (!jo) return -1;

    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;

    if (hvml_jo_value_push(gen->jo, jo)) {
        hvml_jo_value_free(jo);
        return -1;
    }

    gen->jo = jo;

    hvml_jo_value_t *parent = hvml_jo_value_parent(gen->jo);
    if (!parent) return 0;

    gen->jo = parent;

    return 0;
}

static int on_end(void *arg) {
    hvml_dom_gen_t *gen = (hvml_dom_gen_t*)arg;
    hvml_jo_value_t *val = hvml_jo_value_root(gen->jo);
    gen->jo = NULL;
    // hvml_jo_value_printf(val, stdout);
    // fprintf(stdout, "\n");
    // hvml_jo_value_free(val);
    hvml_dom_append_json(gen->dom, val);
    return 0;
}

