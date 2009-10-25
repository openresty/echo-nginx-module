#define DDEBUG 0

#include "ddebug.h"
#include "var.h"
#include "timer.h"
#include "request_info.h"

static ngx_http_variable_t ngx_http_echo_variables[] = {
    { ngx_string("echo_timer_elapsed"), NULL,
      ngx_http_echo_timer_elapsed_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("echo_request_method"), NULL,
      ngx_http_echo_request_method_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("echo_cached_request_uri"), NULL,
      ngx_http_echo_cached_request_uri_variable, 0,
      0, 0 },

    { ngx_string("echo_client_request_method"), NULL,
      ngx_http_echo_client_request_method_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("echo_request_body"), NULL,
      ngx_http_echo_request_body_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("echo_client_request_headers"), NULL,
      ngx_http_echo_client_request_headers_variable, 0,
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

