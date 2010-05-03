#ifndef ECHO_UTIL_H
#define ECHO_UTIL_H

#include "ngx_http_echo_module.h"

#ifndef ngx_str4cmp

#  if (NGX_HAVE_LITTLE_ENDIAN && NGX_HAVE_NONALIGNED)

#    define ngx_str4cmp(m, c0, c1, c2, c3)                                        \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)

#  else

#    define ngx_str4cmp(m, c0, c1, c2, c3)                                        \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3

#  endif

#endif /* ngx_str4cmp */

#ifndef ngx_str3cmp

#  define ngx_str3cmp(m, c0, c1, c2)                                       \
    m[0] == c0 && m[1] == c1 && m[2] == c2

#endif /* ngx_str3cmp */

#ifndef ngx_str6cmp

#  if (NGX_HAVE_LITTLE_ENDIAN && NGX_HAVE_NONALIGNED)

#    define ngx_str6cmp(m, c0, c1, c2, c3, c4, c5)                                \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && (((uint32_t *) m)[1] & 0xffff) == ((c5 << 8) | c4)

#  else

#    define ngx_str6cmp(m, c0, c1, c2, c3, c4, c5)                                \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3                      \
        && m[4] == c4 && m[5] == c5

#  endif

#endif /* ngx_str6cmp */

ngx_int_t ngx_http_echo_init_ctx(ngx_http_request_t *r, ngx_http_echo_ctx_t **ctx_ptr);

ngx_int_t ngx_http_echo_eval_cmd_args(ngx_http_request_t *r,
        ngx_http_echo_cmd_t *cmd, ngx_array_t *computed_args,
        ngx_array_t *opts);

ngx_int_t ngx_http_echo_send_header_if_needed(ngx_http_request_t* r,
        ngx_http_echo_ctx_t *ctx);

ngx_int_t ngx_http_echo_send_chain_link(ngx_http_request_t* r,
        ngx_http_echo_ctx_t *ctx, ngx_chain_t *cl);

ssize_t ngx_http_echo_atosz(u_char *line, size_t n);

u_char * ngx_http_echo_strlstrn(u_char *s1, u_char *last, u_char *s2, size_t n);

#endif /* ECHO_UTIL_H */

