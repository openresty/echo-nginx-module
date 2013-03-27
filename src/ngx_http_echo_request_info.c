#ifndef DDEBUG
#define DDEBUG 0
#endif
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
    ngx_buf_t    *b;
    ngx_chain_t  *cl;
    ngx_chain_t  *in;

    if (r->request_body == NULL
        || r->request_body->bufs == NULL
        || r->request_body->temp_file)
    {
        v->not_found = 1;

        return NGX_OK;
    }

    in = r->request_body->bufs;

    len = 0;
    for (cl = in; cl; cl = cl->next) {
        b = cl->buf;

        if (!ngx_buf_in_memory(b)) {
            if (b->in_file) {
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "variable echo_request_body sees in-file only "
                               "buffers and discard the whole body data");

                v->not_found = 1;

                return NGX_OK;
            }

        } else {
            len += b->last - b->pos;
        }
    }

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    for (cl = in; cl; cl = cl->next) {
        b = cl->buf;

        if (ngx_buf_in_memory(b)) {
            p = ngx_copy(p, b->pos, b->last - b->pos);
        }
    }

    if (p - v->data != (ssize_t) len) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "variable echo_request_body: buffer error");

        v->not_found = 1;

        return NGX_OK;
    }

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
    size_t                       size;
    u_char                      *p, *last, *pos;
    ngx_int_t                    i;
    ngx_buf_t                   *b, *first = NULL;
    unsigned                     found;
    ngx_http_connection_t       *hc;

    hc = r->main->http_connection;

    if (hc->nbusy) {
        b = NULL;  /* to suppress a gcc warning */
        size = 0;
        for (i = 0; i < hc->nbusy; i++) {
            b = hc->busy[i];

            if (first == NULL) {
                if (r->main->request_line.data >= b->pos
                    || r->main->request_line.data
                       + r->main->request_line.len + 2
                       <= b->start)
                {
                    continue;
                }

                dd("found first at %d", (int) i);
                first = b;
            }

            if (b == r->main->header_in) {
                size += r->main->header_end + 2 - b->start;
                break;
            }

            size += b->pos - b->start;
        }

    } else {
        b = r->main->header_in;

        if (b == NULL) {
            v->not_found = 1;
            return NGX_OK;
        }

        size = r->main->header_end + 2 - r->main->request_line.data;
    }

    v->data = ngx_palloc(r->pool, size);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    if (hc->nbusy) {
        last = v->data;
        found = 0;
        for (i = 0; i < hc->nbusy; i++) {
            b = hc->busy[i];

            if (!found) {
                if (b != first) {
                    continue;
                }

                dd("found first");
                found = 1;
            }

            p = last;

            if (b == r->main->header_in) {
                pos = r->main->header_end + 2;

            } else {
                pos = b->pos;
            }

            if (b == first) {
                dd("request line: %.*s", (int) r->main->request_line.len,
                   r->main->request_line.data);

                last = ngx_copy(last,
                                r->main->request_line.data,
                                pos - r->main->request_line.data);

            } else {
                last = ngx_copy(last, b->start, pos - b->start);
            }

#if 1
            /* skip truncated header entries (if any) */
            while (last > p && last[-1] != LF) {
                last--;
            }
#endif

            for (; p != last; p++) {
                if (*p == '\0') {
                    if (p + 1 == last) {
                        /* XXX this should not happen */
                        dd("found string end!!");

                    } else if (*(p + 1) == LF) {
                        *p = CR;

                    } else {
                        *p = ':';
                    }
                }
            }

            if (b == r->main->header_in) {
                break;
            }
        }

    } else {
        last = ngx_copy(v->data, r->main->request_line.data, size);

        for (p = v->data; p != last; p++) {
            if (*p == '\0') {
                if (p + 1 != last && *(p + 1) == LF) {
                    *p = CR;

                } else {
                    *p = ':';
                }
            }
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

