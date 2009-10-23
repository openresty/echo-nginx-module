#define DDEBUG 1

#include "ddebug.h"
#include "util.h"
#include "subrequest.h"
#include "handler.h"

typedef struct ngx_http_echo_subrequest_s {
    ngx_uint_t      method;
    ngx_str_t       *method_name;
    ngx_str_t       *location;
    ngx_str_t       *query_string;
} ngx_http_echo_subrequest_t;

static void ngx_http_echo_adjust_subrequest(ngx_http_request_t *sr,
        ngx_http_echo_subrequest_t *parsed_sr);

static ngx_int_t ngx_http_echo_post_subrequest(ngx_http_request_t *r,
        void *data, ngx_int_t rc);

static ngx_int_t ngx_http_echo_parse_subrequest_spec(ngx_http_request_t *r,
        ngx_array_t *computed_args, ngx_http_echo_subrequest_t *parsed_sr);

ngx_int_t
ngx_http_echo_exec_echo_subrequest_async(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args) {
    ngx_int_t                       rc;
    ngx_http_echo_subrequest_t      parsed_sr;
    ngx_http_request_t              *sr; /* subrequest object */

    rc = ngx_http_echo_parse_subrequest_spec(r, computed_args, &parsed_sr);
    if (rc != NGX_OK) {
        return rc;
    }

    DD("location: %s", parsed_sr.location->data);
    DD("location args: %s", (char*) (parsed_sr.query_string ?
                parsed_sr.query_string->data : (u_char*)"NULL"));

    rc = ngx_http_echo_send_header_if_needed(r, ctx);
    if (r->header_only || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    rc = ngx_http_subrequest(r, parsed_sr.location, parsed_sr.query_string,
            &sr, NULL, 0);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_http_echo_adjust_subrequest(sr, &parsed_sr);

    return NGX_OK;
}

ngx_int_t
ngx_http_echo_exec_echo_subrequest(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args) {
    ngx_int_t                           rc;
    ngx_http_request_t                  *sr; /* subrequest object */
    ngx_http_post_subrequest_t          *psr;
    ngx_http_echo_subrequest_t          parsed_sr;

    rc = ngx_http_echo_parse_subrequest_spec(r, computed_args, &parsed_sr);
    if (rc != NGX_OK) {
        return rc;
    }

    rc = ngx_http_echo_send_header_if_needed(r, ctx);
    if (r->header_only || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    psr->handler = ngx_http_echo_post_subrequest;
    psr->data = ctx;


    rc = ngx_http_subrequest(r, parsed_sr.location, parsed_sr.query_string,
            &sr, psr, 0);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_http_echo_adjust_subrequest(sr, &parsed_sr);

    return NGX_OK;
}

static ngx_int_t
ngx_http_echo_post_subrequest(ngx_http_request_t *r,
        void *data, ngx_int_t rc) {
    ngx_http_echo_ctx_t         *ctx;

    ctx = data;
    ctx->next_handler_cmd++;

    return ngx_http_echo_handler(r->parent);
}

static ngx_int_t
ngx_http_echo_parse_subrequest_spec(ngx_http_request_t *r,
        ngx_array_t *computed_args, ngx_http_echo_subrequest_t *parsed_sr) {
    ngx_str_t                   *computed_arg_elts, *arg;
    ngx_str_t                   **to_write = NULL;
    ngx_str_t                   *location, *method_name;
    ngx_str_t                   *query_string = NULL;
    ngx_uint_t                  i;
    ngx_int_t                   method;
    ngx_flag_t                  expecting_opt;

    computed_arg_elts = computed_args->elts;

    method_name = &computed_arg_elts[0];
    location    = &computed_arg_elts[1];

    expecting_opt = 1;
    for (i = 2; i < computed_args->nelts; i++) {
        arg = &computed_arg_elts[i];
        if (!expecting_opt) {
            if (to_write == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "echo_subrequest_async: to_write should NOT be NULL");
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            *to_write = arg;
            to_write = NULL;
            expecting_opt = 0;
            continue;
        }
        if (ngx_strncmp("-q", arg->data, arg->len) == 0) {
            to_write = &query_string;
            expecting_opt = 0;
        } else {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "Unknown option for echo_subrequest_async: %V", arg);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    switch (method_name->len) {
        case 3:
            if (ngx_strncmp("GET", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_GET;
                break;
            }
            if (ngx_strncmp("PUT", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_PUT;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 4:
            if (ngx_strncmp("POST", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_POST;
                break;
            }
            if (ngx_strncmp("HEAD", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_HEAD;
                break;
            }
            if (ngx_strncmp("COPY", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_COPY;
                break;
            }
            if (ngx_strncmp("MOVE", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_MOVE;
                break;
            }
            if (ngx_strncmp("LOCK", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_LOCK;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 5:
            if (ngx_strncmp("MKCOL", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_MKCOL;
                break;
            }
            if (ngx_strncmp("TRACE", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_TRACE;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 6:
            if (ngx_strncmp("DELETE", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_DELETE;
                break;
            }
            if (ngx_strncmp("UNLOCK", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_UNLOCK;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 7:
            if (ngx_strncmp("OPTIONS", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_OPTIONS;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 8:
            if (ngx_strncmp("PROPFIND", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_PROPFIND;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 9:
            if (ngx_strncmp("PROPPATCH", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_PROPPATCH;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        default:
            method = NGX_HTTP_UNKNOWN;
            break;
    }

    parsed_sr->method       = method;
    parsed_sr->method_name  = method_name;
    parsed_sr->location     = location;
    parsed_sr->query_string = query_string;

    return NGX_OK;
}

static void
ngx_http_echo_adjust_subrequest(ngx_http_request_t *sr, ngx_http_echo_subrequest_t *parsed_sr) {
    sr->method = parsed_sr->method;
    sr->method_name = *(parsed_sr->method_name);
}

