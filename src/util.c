#define DDEBUG 0

#include "ddebug.h"
#include "util.h"
#include "sleep.h"

ngx_int_t
ngx_http_echo_init_ctx(ngx_http_request_t *r, ngx_http_echo_ctx_t **ctx_ptr) {
    *ctx_ptr = ngx_pcalloc(r->pool, sizeof(ngx_http_echo_ctx_t));
    if (*ctx_ptr == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    (*ctx_ptr)->sleep.handler   = ngx_http_echo_sleep_event_handler;
    (*ctx_ptr)->sleep.data      = r;
    (*ctx_ptr)->sleep.log       = r->connection->log;

    return NGX_OK;
}

ngx_int_t
ngx_http_echo_eval_cmd_args(ngx_http_request_t *r,
        ngx_http_echo_cmd_t *cmd, ngx_array_t *computed_args) {
    ngx_uint_t                      i;
    ngx_array_t                     *args = cmd->args;
    ngx_str_t                       *computed_arg;
    ngx_http_echo_arg_template_t    *arg, *arg_elts;

    arg_elts = args->elts;
    for (i = 0; i < args->nelts; i++) {
        computed_arg = ngx_array_push(computed_args);
        if (computed_arg == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        arg = &arg_elts[i];
        if (arg->lengths == NULL) { /* does not contain vars */
            DD("Using raw value \"%s\"", arg->raw_value.data);
            *computed_arg = arg->raw_value;
        } else {
            if (ngx_http_script_run(r, computed_arg, arg->lengths->elts,
                        0, arg->values->elts) == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
        }
    }
    return NGX_OK;
}

