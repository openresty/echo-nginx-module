#ifndef ECHO_HANDLER_H
#define ECHO_HANDLER_H

#include "ngx_http_echo_module.h"

ngx_int_t ngx_http_echo_handler_init(ngx_conf_t *cf);

ngx_int_t ngx_http_echo_handler(ngx_http_request_t *r);

#endif /* ECHO_HANDLER_H */

