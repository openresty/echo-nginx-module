#define DDEBUG 0
#include "ddebug.h"

#include "ngx_http_echo_request_info.h"
#include "ngx_http_echo_util.h"
#include "ngx_http_echo_handler.h"

#include <nginx.h>


static void ngx_http_echo_post_read_request_body(ngx_http_request_t *r);

ngx_int_t
ngx_http_echo_exec_echo_read_request_body(
        ngx_http_request_t* r, ngx_http_echo_ctx_t *ctx)
{
    return ngx_http_read_client_request_body(r,
            ngx_http_echo_post_read_request_body);
}


static void
ngx_http_echo_post_read_request_body(ngx_http_request_t *r)
{
    ngx_http_echo_ctx_t         *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_echo_module);

    dd("wait read request body %d", (int) ctx->wait_read_request_body);

    if (ctx->wait_read_request_body) {
        ctx->waiting = 0;
        ctx->done = 1;

        r->write_event_handler = ngx_http_echo_wev_handler;

        ngx_http_echo_wev_handler(r);
    }
}


/* this function's implementation is borrowed from nginx 0.8.20
 * and modified a bit to work with subrequests.
 * Copyrighted (C) by Igor Sysoev */
ngx_int_t
ngx_http_echo_request_method_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->method_name.data) {
        v->len = r->method_name.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->method_name.data;
    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


/* this function's implementation is borrowed from nginx 0.8.20
 * and modified a bit to work with subrequests.
 * Copyrighted (C) by Igor Sysoev */
ngx_int_t
ngx_http_echo_client_request_method_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->main->method_name.data) {
        v->len = r->main->method_name.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->main->method_name.data;
    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


/* this function's implementation is borrowed from nginx 0.8.20
 * and modified a bit to work with subrequests.
 * Copyrighted (C) by Igor Sysoev */
ngx_int_t
ngx_http_echo_request_body_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char       *p;
    size_t        len;
    ngx_buf_t    *buf, *next;
    ngx_chain_t  *cl;

    if (r->request_body == NULL
        || r->request_body->bufs == NULL
        || r->request_body->temp_file)
    {
        v->not_found = 1;

        return NGX_OK;
    }

    cl = r->request_body->bufs;
    buf = cl->buf;

    if (cl->next == NULL) {
        v->len = buf->last - buf->pos;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = buf->pos;

        return NGX_OK;
    }

    next = cl->next->buf;
    len = (buf->last - buf->pos) + (next->last - next->pos);

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    p = ngx_cpymem(p, buf->pos, buf->last - buf->pos);
    ngx_memcpy(p, next->pos, next->last - next->pos);

    v->len = len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


ngx_int_t
ngx_http_echo_client_request_headers_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data)
{
    size_t                      size;
    u_char                      *p, *last;
    ngx_buf_t                   *header_in;
    ngx_flag_t                  just_seen_crlf;

    if (r != r->main) {
        header_in = r->main->header_in;
    } else {
        header_in = r->header_in;
    }

    if (header_in == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    size = header_in->pos - header_in->start;

    v->data = ngx_palloc(r->pool, size);
    last = ngx_cpymem(v->data, header_in->start, size);

    /* fix \0 introduced by the nginx header parser and
     * locate the end of the header */
    just_seen_crlf = 0;
    for (p = (u_char*)v->data; p != last; p++) {
        if (*p == '\0') {
            if (p + 1 != last && *(p + 1) == LF) {
                just_seen_crlf = 1;
                *p = CR;
            } else {
                *p = ':';
                just_seen_crlf = 0;
            }
        } else if (*p == CR) {
            if (just_seen_crlf) {
                *p = '\0';
                last = p;
                break;
            }
        } else if (*p != LF) {
            just_seen_crlf = 0;
        }
    }

    v->len = last - v->data;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


ngx_int_t
ngx_http_echo_cacheable_request_uri_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->uri.len) {
        v->len = r->uri.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->uri.data;
    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_echo_request_uri_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->uri.len) {
        v->len = r->uri.len;
        v->valid = 1;
        v->no_cacheable = 1;
        v->not_found = 0;
        v->data = r->uri.data;
    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_echo_response_status_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                      *p;

    if (r->headers_out.status) {
        dd("headers out status: %d", (int) r->headers_out.status);

        p = ngx_palloc(r->pool, NGX_INT_T_LEN);
        if (p == NULL) {
            return NGX_ERROR;
        }

        v->len = ngx_sprintf(p, "%ui", r->headers_out.status) - p;
        v->data = p;

        v->valid = 1;
        v->no_cacheable = 1;
        v->not_found = 0;
    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}

