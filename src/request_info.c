#define DDEBUG 0
#include "ddebug.h"

#include "request_info.h"
#include "util.h"

#include <nginx.h>

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

