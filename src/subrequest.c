#define DDEBUG 1

#include "ddebug.h"
#include "util.h"
#include "subrequest.h"
#include "handler.h"

ngx_str_t  ngx_http_echo_content_length_header_key = ngx_string("Content-Length");

ngx_str_t  ngx_http_echo_get_method = { 3, (u_char *) "GET " };
ngx_str_t  ngx_http_echo_put_method = { 3, (u_char *) "PUT " };

ngx_str_t  ngx_http_echo_post_method = { 4, (u_char *) "POST " };
ngx_str_t  ngx_http_echo_head_method = { 4, (u_char *) "HEAD " };
ngx_str_t  ngx_http_echo_copy_method = { 4, (u_char *) "COPY " };
ngx_str_t  ngx_http_echo_move_method = { 4, (u_char *) "MOVE " };
ngx_str_t  ngx_http_echo_lock_method = { 4, (u_char *) "LOCK " };

ngx_str_t  ngx_http_echo_mkcol_method = { 5, (u_char *) "MKCOL " };
ngx_str_t  ngx_http_echo_trace_method = { 5, (u_char *) "TRACE " };

ngx_str_t  ngx_http_echo_delete_method = { 6, (u_char *) "DELETE " };
ngx_str_t  ngx_http_echo_unlock_method = { 6, (u_char *) "UNLOCK " };

ngx_str_t  ngx_http_echo_options_method = { 7, (u_char *) "OPTIONS " };
ngx_str_t  ngx_http_echo_propfind_method = { 8, (u_char *) "PROPFIND " };
ngx_str_t  ngx_http_echo_proppatch_method = { 9, (u_char *) "PROPPATCH " };

typedef struct ngx_http_echo_subrequest_s {
    ngx_uint_t                  method;
    ngx_str_t                   *method_name;
    ngx_str_t                   *location;
    ngx_str_t                   *query_string;
    ssize_t                     content_length_n;
    ngx_http_request_body_t     *request_body;
} ngx_http_echo_subrequest_t;

static ngx_int_t ngx_http_echo_adjust_subrequest(ngx_http_request_t *sr,
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

    rc = ngx_http_echo_adjust_subrequest(sr, &parsed_sr);
    if (rc != NGX_OK) {
        return rc;
    }

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
    if (rc != NGX_OK) {
        return rc;
    }

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
    ngx_str_t                   *request_body = NULL;
    ngx_str_t                   *query_string = NULL;
    ngx_uint_t                  i;
    ngx_int_t                   method;
    ngx_flag_t                  expecting_opt;
    ngx_http_request_body_t     *rb = NULL;
    ngx_buf_t                   *b;

    ngx_memzero(parsed_sr, sizeof(ngx_http_echo_subrequest_t));

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
        } else if (ngx_strncmp("-b", arg->data, arg->len) == 0) {
            to_write = &request_body;
            expecting_opt = 0;
        } else {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "Unknown option for echo_subrequest_async: %V", arg);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    if (request_body != NULL && request_body->len != 0) {
        rb = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
        if (rb == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        parsed_sr->content_length_n = request_body->len;

        b = ngx_calloc_buf(r->pool);
        if (b == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->temporary = 1;
        /* b->memory = 1; */
        b->start = b->pos = request_body->data;
        b->end = b->last = request_body->data + request_body->len;

        rb->bufs = ngx_alloc_chain_link(r->pool);
        if (rb->bufs == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        rb->bufs->buf = b;
        rb->bufs->next = NULL;

        rb->buf = b;
    }

    switch (method_name->len) {
        case 3:
            if (ngx_strncmp("GET", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_GET;
                method_name = &ngx_http_echo_get_method;
                break;
            }
            if (ngx_strncmp("PUT", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_PUT;
                method_name = &ngx_http_echo_put_method;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 4:
            if (ngx_strncmp("POST", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_POST;
                method_name = &ngx_http_echo_post_method;
                break;
            }
            if (ngx_strncmp("HEAD", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_HEAD;
                method_name = &ngx_http_echo_head_method;
                break;
            }
            if (ngx_strncmp("COPY", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_COPY;
                method_name = &ngx_http_echo_copy_method;
                break;
            }
            if (ngx_strncmp("MOVE", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_MOVE;
                method_name = &ngx_http_echo_move_method;
                break;
            }
            if (ngx_strncmp("LOCK", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_LOCK;
                method_name = &ngx_http_echo_lock_method;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 5:
            if (ngx_strncmp("MKCOL", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_MKCOL;
                method_name = &ngx_http_echo_mkcol_method;
                break;
            }
            if (ngx_strncmp("TRACE", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_TRACE;
                method_name = &ngx_http_echo_trace_method;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 6:
            if (ngx_strncmp("DELETE", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_DELETE;
                method_name = &ngx_http_echo_delete_method;
                break;
            }
            if (ngx_strncmp("UNLOCK", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_UNLOCK;
                method_name = &ngx_http_echo_unlock_method;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 7:
            if (ngx_strncmp("OPTIONS", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_OPTIONS;
                method_name = &ngx_http_echo_options_method;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 8:
            if (ngx_strncmp("PROPFIND", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_PROPFIND;
                method_name = &ngx_http_echo_propfind_method;
                break;
            }
            method = NGX_HTTP_UNKNOWN;
            break;

        case 9:
            if (ngx_strncmp("PROPPATCH", method_name->data, method_name->len) == 0) {
                method = NGX_HTTP_PROPPATCH;
                method_name = &ngx_http_echo_proppatch_method;
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
    parsed_sr->request_body = rb;

    return NGX_OK;
}

static ngx_int_t
ngx_http_echo_adjust_subrequest(ngx_http_request_t *sr, ngx_http_echo_subrequest_t *parsed_sr) {
    ngx_table_elt_t            *h;

    sr->method = parsed_sr->method;
    sr->method_name = *(parsed_sr->method_name);
    if (parsed_sr->content_length_n > 0) {
        sr->headers_in.content_length_n = parsed_sr->content_length_n;
        sr->request_body = parsed_sr->request_body;

        sr->headers_in.content_length = ngx_pcalloc(sr->pool,
                sizeof(ngx_table_elt_t));
        sr->headers_in.content_length->value.data =
            ngx_palloc(sr->pool, NGX_OFF_T_LEN);
        if (sr->headers_in.content_length->value.data == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        sr->headers_in.content_length->value.len = ngx_sprintf(
                sr->headers_in.content_length->value.data, "%O",
                sr->headers_in.content_length_n) -
                sr->headers_in.content_length->value.data;

        if (ngx_list_init(&sr->headers_in.headers, sr->pool, 20,
                    sizeof(ngx_table_elt_t)) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        h = ngx_list_push(&sr->headers_in.headers);
        if (h == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        h->hash = sr->header_hash;

        h->key = ngx_http_echo_content_length_header_key;
        h->value = sr->headers_in.content_length->value;

        DD("sr content length: %s", sr->headers_in.content_length->value.data);
    }
    return NGX_OK;
}

