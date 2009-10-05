#define DDEBUG 0

#include "ngx_http_echo_module.h"

#include <ngx_config.h>
#include <ngx_log.h>

#if (DDEBUG)
#include <ddebug.h>
#else
#define DD
#endif

static char *ngx_http_echo_echo(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_echo_handler(ngx_http_request_t *r);

static ngx_command_t  ngx_http_echo_commands[] = {

    { ngx_string("echo"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_NOARGS,
      ngx_http_echo_echo,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};

static ngx_http_module_t  ngx_http_echo_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};

ngx_module_t ngx_http_echo_module = {
    NGX_MODULE_V1,
    &ngx_http_echo_module_ctx, /* module context */
    ngx_http_echo_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_echo_handler(ngx_http_request_t *r) {
    ngx_buf_t     *b;
    ngx_buf_t     *header_in;
    ngx_chain_t   out;
    ngx_int_t     rc;
    size_t        size;
    u_char        *c;

    r->headers_out.content_type.len = sizeof(ngx_http_echo_content_type) - 1;
    r->headers_out.content_type.data = (u_char *) ngx_http_echo_content_type;

    if (r != r->main) {
        header_in = r->main->header_in;
    } else {
        header_in = r->header_in;
    }
    if (NULL == header_in) {
        DD("header_in is NULL");
        return NGX_HTTP_BAD_REQUEST;
    }
    size = header_in->pos - header_in->start;
    //DD("!!! size: %lu", (unsigned long)size);

    b = ngx_create_temp_buf(r->pool, size);
    b->last = ngx_cpymem(b->start, header_in->start, size);
    b->memory = 1;
    b->last_buf = 1;

    /* fix \0 introduced by the nginx header parser */

    for (c = (u_char*)b->start; c != b->last; c++) {
        if (*c == '\0') {
            if (c + 1 != b->last && *(c + 1) == LF) {
                *c = CR;
            } else {
                *c = ':';
            }
        }
    }

    out.buf = b;
    out.next = NULL;

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = size;

    rc = ngx_http_send_header(r);
    if (r->header_only || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}

static char *
ngx_http_echo(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_echo_handler;

    return NGX_CONF_OK;
}

