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

ngx_int_t
ngx_http_echo_send_chain_link(ngx_http_request_t* r,
        ngx_http_echo_ctx_t *ctx, ngx_chain_t *cl) {
    ngx_int_t   rc;

    rc = ngx_http_echo_send_header_if_needed(r, ctx);
    if (r->header_only || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    if (cl == NULL) {

#if defined(nginx_version) && nginx_version <= 8004

        /* earlier versions of nginx does not allow subrequests
            to send last_buf themselves */
        if (r != r->main) {
            return NGX_OK;
        }

#endif

        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        return NGX_OK;
    }

    return ngx_http_output_filter(r, cl);
}

ngx_int_t
ngx_http_echo_send_header_if_needed(ngx_http_request_t* r,
        ngx_http_echo_ctx_t *ctx) {
    ngx_int_t   rc;

    if ( ! ctx->headers_sent ) {
        ctx->headers_sent = 1;
        r->headers_out.status = NGX_HTTP_OK;
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        rc = ngx_http_send_header(r);
    }
    return NGX_OK;
}

ssize_t
ngx_http_echo_atosz(u_char *line, size_t n) {
    ssize_t  value;

    if (n == 0) {
        return NGX_ERROR;
    }

    for (value = 0; n--; line++) {
        if (*line == '_') { /* we ignore undercores */
            continue;
        }

        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return NGX_ERROR;
    } else {
        return value;
    }
}

