#define DDEBUG 0

#include "ddebug.h"
#include "var.h"
#include "timer.h"

static ngx_int_t ngx_http_echo_request_method_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_echo_client_request_method_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_echo_request_body_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data);


static ngx_http_variable_t ngx_http_echo_variables[] = {
    { ngx_string("echo_timer_elapsed"), NULL,
      ngx_http_echo_timer_elapsed_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("echo_request_method"), NULL,
      ngx_http_echo_request_method_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("echo_client_request_method"), NULL,
      ngx_http_echo_client_request_method_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("echo_request_body"), NULL,
      ngx_http_echo_request_body_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string(""), NULL, NULL, 0, 0, 0 }
};

ngx_int_t
ngx_http_echo_add_variables(ngx_conf_t *cf) {
    ngx_http_variable_t *var, *v;
    for (v = ngx_http_echo_variables; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }
        var->get_handler = v->get_handler;
        var->data = v->data;
    }
    return NGX_OK;
}

static ngx_int_t
ngx_http_echo_request_method_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data) {
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

static ngx_int_t
ngx_http_echo_client_request_method_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data) {
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

static ngx_int_t
ngx_http_echo_request_body_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data) {
    u_char       *p;
    size_t        len;
    ngx_buf_t    *buf, *next;
    ngx_chain_t  *cl;

#if 0

    DD("rrr request_body null ? %d", r->request_body == NULL);
    if (r->request_body) {
        DD("rrr request_body bufs null ? %d", r->request_body->bufs == NULL);
        DD("rrr request_body temp file ? %d", r->request_body->temp_file != NULL);
    }
    DD("rrr request_body content length ? %ld", (long) r->headers_in.content_length_n);

#endif


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

