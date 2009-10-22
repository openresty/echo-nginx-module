#ifndef ECHO_REQUEST_INFO_H
#define ECHO_REQUEST_INFO_H

#include "ngx_http_echo_module.h"

ngx_int_t ngx_http_echo_exec_echo_client_request_headers(
        ngx_http_request_t* r, ngx_http_echo_ctx_t *ctx);

#endif /* ECHO_REQUEST_INFO_H */

