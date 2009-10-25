#define DDEBUG 1
#include "ddebug.h"

#include "foreach.h"

#include <nginx.h>

ngx_int_t
ngx_http_echo_it_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data) {
    ngx_http_echo_ctx_t         *ctx;
    ngx_uint_t                  i;
    ngx_array_t                 *choices;
    ngx_str_t                   *choice_elts, *choice;

    ctx = ngx_http_get_module_ctx(r, ngx_http_echo_module);

    if (ctx->foreach != NULL) {
        choices = ctx->foreach->choices;
        i = ctx->foreach->next_choice;
        if (i < choices->nelts) {
            choice_elts = choices->elts;
            choice = &choice_elts[i];

            v->len = choice->len;
            v->data = choice->data;
            v->valid = 1;
            v->no_cacheable = 1;
            v->not_found = 0;
        }

        return NGX_OK;
    }

    v->not_found = 1;

    return NGX_OK;
}

ngx_int_t
ngx_http_echo_exec_echo_foreach(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args) {
    ngx_str_t                   *delimiter, *compound;
    u_char                      *pos, *last, *end;
    ngx_str_t                   *choice;
    ngx_str_t                   *computed_arg_elts;

    if (ctx->foreach != NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "Nested echo_foreach not supported yet.");
        return NGX_ERROR;
    }

    ctx->foreach = ngx_palloc(r->pool, sizeof(ngx_http_echo_foreach_ctx_t));
    if (ctx->foreach == NULL) {
        return NGX_ERROR;
    }

    ctx->foreach->cmd_index = ctx->next_handler_cmd;

    ctx->foreach->next_choice = 0;

    ctx->foreach->choices = ngx_array_create(r->pool, 10, sizeof(ngx_str_t));
    if (ctx->foreach->choices == NULL) {
        return NGX_ERROR;
    }

    computed_arg_elts = computed_args->elts;

    delimiter = &computed_arg_elts[0];
    compound  = &computed_arg_elts[1];

    pos = compound->data;
    end = compound->data + compound->len;
    while ((last = ngx_strlcasestrn(pos, end, delimiter->data, delimiter->len - 1))
                != NULL) {
        choice = ngx_array_push(ctx->foreach->choices);
        if (choice == NULL) {
            return NGX_ERROR;
        }

        choice->data = pos;
        choice->len  = last - pos;
        pos = last + delimiter->len;
    }

    if (pos < end) {
        choice = ngx_array_push(ctx->foreach->choices);
        if (choice == NULL) {
            return NGX_ERROR;
        }

        choice->data = pos;
        choice->len  = end - pos;
    }

    return NGX_OK;
}

ngx_int_t
ngx_http_echo_exec_echo_end(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx) {
    if (ctx->foreach == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "Found a echo_end that has no corresponding echo_foreach "
                "before it.");
        return NGX_ERROR;
    }

    ctx->foreach->next_choice++;

    if (ctx->foreach->next_choice >= ctx->foreach->choices->nelts) {
        return NGX_OK;
    }

    DD("echo_end: ++ next_choice (total: %u): %u", ctx->foreach->choices->nelts, ctx->foreach->next_choice);

    /* the main handler dispatcher loop will increment
     *   ctx->next_handler_cmd for us anyway. */
    ctx->next_handler_cmd = ctx->foreach->cmd_index;

    return NGX_OK;
}

