#include "var.h"
#include "timer.h"

static ngx_int_t ngx_http_echo_request_method_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_echo_client_request_method_variable(ngx_http_request_t *r,
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

