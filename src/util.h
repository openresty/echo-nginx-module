#ifndef ECHO_UTIL_H
#define ECHO_UTIL_H

#include "ngx_http_echo_module.h"

ngx_int_t ngx_http_echo_init_ctx(ngx_http_request_t *r, ngx_http_echo_ctx_t **ctx_ptr);

ngx_int_t ngx_http_echo_eval_cmd_args(ngx_http_request_t *r,
        ngx_http_echo_cmd_t *cmd, ngx_array_t *computed_args);

#endif /* ECHO_UTIL_H */

