#define DDEBUG 0
#include "ddebug.h"

#include "echo.h"
#include "util.h"

#include <nginx.h>

static ngx_buf_t ngx_http_echo_space_buf;

static ngx_buf_t ngx_http_echo_newline_buf;

ngx_int_t
ngx_http_echo_echo_init(ngx_conf_t *cf) {
    static u_char space_str[]   = " ";
    static u_char newline_str[] = "\n";

    DD("global init...");

    ngx_memzero(&ngx_http_echo_space_buf, sizeof(ngx_buf_t));
    ngx_http_echo_space_buf.memory = 1;
    ngx_http_echo_space_buf.start =
        ngx_http_echo_space_buf.pos =
            space_str;
    ngx_http_echo_space_buf.end =
        ngx_http_echo_space_buf.last =
            space_str + sizeof(space_str) - 1;

    ngx_memzero(&ngx_http_echo_newline_buf, sizeof(ngx_buf_t));
    ngx_http_echo_newline_buf.memory = 1;
    ngx_http_echo_newline_buf.start =
        ngx_http_echo_newline_buf.pos =
            newline_str;
    ngx_http_echo_newline_buf.end =
        ngx_http_echo_newline_buf.last =
            newline_str + sizeof(newline_str) - 1;

    return NGX_OK;
}

ngx_int_t
ngx_http_echo_exec_echo(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args) {
    ngx_uint_t                  i;

    ngx_buf_t                   *space_buf;
    ngx_buf_t                   *newline_buf;
    ngx_buf_t                   *buf;

    ngx_str_t                   *computed_arg;
    ngx_str_t                   *computed_arg_elts;

    ngx_chain_t *cl  = NULL; /* the head of the chain link */
    ngx_chain_t **ll = NULL;  /* always point to the address of the last link */

    DD("now exec echo...");

    if (computed_args == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    computed_arg_elts = computed_args->elts;
    for (i = 0; i < computed_args->nelts; i++) {
        computed_arg = &computed_arg_elts[i];
        buf = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        if (buf == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        buf->start = buf->pos = computed_arg->data;
        buf->last = buf->end = computed_arg->data +
            computed_arg->len;
        buf->memory = 1;

        if (cl == NULL) {
            cl = ngx_alloc_chain_link(r->pool);
            if (cl == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            cl->buf  = buf;
            cl->next = NULL;
            ll = &cl->next;
        } else {
            /* append a space first */
            *ll = ngx_alloc_chain_link(r->pool);
            if (*ll == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            space_buf = ngx_calloc_buf(r->pool);
            if (space_buf == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            /* nginx clears buf flags at the end of each request handling,
             * so we have to make a clone here. */
            *space_buf = ngx_http_echo_space_buf;

            (*ll)->buf = space_buf;
            (*ll)->next = NULL;

            ll = &(*ll)->next;

            /* then append the buf */
            *ll = ngx_alloc_chain_link(r->pool);
            if (*ll == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            (*ll)->buf  = buf;
            (*ll)->next = NULL;

            ll = &(*ll)->next;
        }
    } /* end for */

    /* append the newline character */
    /* TODO add support for -n option to suppress
     * the trailing newline */
    newline_buf = ngx_calloc_buf(r->pool);
    if (newline_buf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    *newline_buf = ngx_http_echo_newline_buf;

    if (cl == NULL) {
        cl = ngx_alloc_chain_link(r->pool);
        if (cl == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        cl->buf = newline_buf;
        cl->next = NULL;
        /* ll = &cl->next; */
    } else {
        *ll = ngx_alloc_chain_link(r->pool);
        if (*ll == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        (*ll)->buf  = newline_buf;
        (*ll)->next = NULL;
        /* ll = &(*ll)->next; */
    }

    return ngx_http_echo_send_chain_link(r, ctx, cl);
}

ngx_int_t
ngx_http_echo_exec_echo_client_request_headers(
        ngx_http_request_t* r, ngx_http_echo_ctx_t *ctx) {
    size_t                      size;
    u_char                      *c;
    ngx_buf_t                   *header_in;
    ngx_chain_t                 *cl  = NULL;
    ngx_buf_t                   *buf;

    DD("echo_client_request_headers triggered!");
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    cl->next = NULL;

    if (r != r->main) {
        header_in = r->main->header_in;
    } else {
        header_in = r->header_in;
    }
    if (header_in == NULL) {
        DD("header_in is NULL");
        return NGX_HTTP_BAD_REQUEST;
    }
    size = header_in->pos - header_in->start;
    //DD("!!! size: %lu", (unsigned long)size);

    buf = ngx_create_temp_buf(r->pool, size);
    buf->last = ngx_cpymem(buf->start, header_in->start, size);
    buf->memory = 1;

    /* fix \0 introduced by the nginx header parser */
    for (c = (u_char*)buf->start; c != buf->last; c++) {
        if (*c == '\0') {
            if (c + 1 != buf->last && *(c + 1) == LF) {
                *c = CR;
            } else {
                *c = ':';
            }
        }
    }

    cl->buf = buf;
    return ngx_http_echo_send_chain_link(r, ctx, cl);
}

ngx_int_t
ngx_http_echo_exec_echo_flush(ngx_http_request_t *r, ngx_http_echo_ctx_t *ctx) {
    return ngx_http_send_special(r, NGX_HTTP_FLUSH);
}

ngx_int_t
ngx_http_echo_exec_echo_duplicate(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args) {
    ngx_str_t                   *computed_arg;
    ngx_str_t                   *computed_arg_elts;
    ssize_t                     i, count;
    ngx_str_t                   *str;
    u_char                      *p;

    ngx_buf_t                   *buf;
    ngx_chain_t                 *cl;

    computed_arg_elts = computed_args->elts;

    computed_arg = &computed_arg_elts[0];
    count = ngx_http_echo_atosz(computed_arg->data, computed_arg->len);
    if (count == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                   "invalid size specified: \"%V\"", computed_arg);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    str = &computed_arg_elts[1];

    buf = ngx_create_temp_buf(r->pool, count * str->len);
    if (buf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    p = buf->pos = buf->start;
    for (i = 0; i < count; i++) {
        p = ngx_cpymem(p, str->data, str->len);
    }
    buf->last = p;

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    cl->next = NULL;
    cl->buf = buf;

    return ngx_http_echo_send_chain_link(r, ctx, cl);
}

