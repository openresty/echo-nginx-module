#ifndef DDEBUG
#define DDEBUG 0
#endif

#include "ddebug.h"

#include "ngx_http_echo_timer.h"
#include "ngx_http_echo_util.h"

#include <stdlib.h>
#include <ngx_log.h>
#include <nginx.h>


ngx_int_t
ngx_http_echo_timer_elapsed_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_echo_ctx_t     *ctx;
    ngx_msec_int_t           ms;
    u_char                  *p;
#if !(defined freenginx && nginx_version >= 1029000)
    ngx_time_t              *tp;
#endif
    size_t                   size;

    ctx = ngx_http_get_module_ctx(r, ngx_http_echo_module);
    if (ctx == NULL) {
        ctx = ngx_http_echo_create_ctx(r);
        if (ctx == NULL) {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_echo_module);
    }

#if (defined freenginx && nginx_version >= 1029000)
    if (ctx->timer_begin == 0)
        ctx->timer_begin = r->start_time;
#else
    if (ctx->timer_begin.sec == 0) {
        ctx->timer_begin.sec  = r->start_sec;
        ctx->timer_begin.msec = (ngx_msec_t) r->start_msec;
    }
#endif

    /* force the ngx timer to update */

#if (nginx_version >= 8035) || (nginx_version < 8000 && nginx_version >= 7066)
    ngx_time_update();
#else
    ngx_time_update(0, 0);
#endif

#if (defined freenginx && nginx_version >= 1029000)
    ms = (ngx_msec_int_t) (ngx_current_msec - ctx->timer_begin);
#else

    tp = ngx_timeofday();

    dd("old sec msec: %ld %d\n", (long) ctx->timer_begin.sec,
       (int) ctx->timer_begin.msec);

    dd("new sec msec: %ld %d\n", (long) tp->sec, (int) tp->msec);

    ms = (ngx_msec_int_t)
             ((tp->sec - ctx->timer_begin.sec) * 1000 +
              (tp->msec - ctx->timer_begin.msec));
#endif
    ms = (ms >= 0) ? ms : 0;

    size = sizeof("-9223372036854775808.000") - 1;

    p = ngx_palloc(r->pool, size);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_snprintf(p, size, "%T.%03M", ms / 1000, ms % 1000) - p;
    v->data = p;

    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;

    return NGX_OK;
}


ngx_int_t
ngx_http_echo_exec_echo_reset_timer(ngx_http_request_t *r,
    ngx_http_echo_ctx_t *ctx)
{
    dd("Exec timer...");

    /* force the ngx timer to update */

#if (nginx_version >= 8035) || (nginx_version < 8000 && nginx_version >= 7066)
    ngx_time_update();
#else
    ngx_time_update(0, 0);
#endif

#if (defined freenginx && nginx_version >= 1029000)
    ctx->timer_begin = ngx_current_msec;
#else
    ctx->timer_begin = *ngx_timeofday();
#endif
    return NGX_OK;
}

