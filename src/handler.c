#define DDEBUG 0

#include "ddebug.h"

#include "handler.h"
#include "echo.h"
#include "util.h"
#include "sleep.h"
#include "var.h"
#include "timer.h"
#include "location.h"
#include "subrequest.h"
#include "request_info.h"
#include "foreach.h"

#include <nginx.h>
#include <ngx_log.h>

ngx_int_t
ngx_http_echo_handler_init(ngx_conf_t *cf) {
    ngx_int_t         rc;

    rc = ngx_http_echo_echo_init(cf);
    if (rc != NGX_OK) {
        return rc;
    }

    return ngx_http_echo_add_variables(cf);
}

ngx_int_t
ngx_http_echo_handler(ngx_http_request_t *r) {
    ngx_http_echo_loc_conf_t    *elcf;
    ngx_http_echo_ctx_t         *ctx;
    ngx_int_t                   rc;
    ngx_array_t                 *cmds;
    ngx_array_t                 *computed_args = NULL;
    ngx_http_echo_cmd_t         *cmd;
    ngx_http_echo_cmd_t         *cmd_elts;

    elcf = ngx_http_get_module_loc_conf(r, ngx_http_echo_module);
    cmds = elcf->handler_cmds;
    if (cmds == NULL) {
        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_echo_module);
    if (ctx == NULL) {
        rc = ngx_http_echo_init_ctx(r, &ctx);
        if (rc != NGX_OK) {
            return rc;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_echo_module);
    }

    DD("exec handler: %s: %u", r->uri.data, ctx->next_handler_cmd);

    cmd_elts = cmds->elts;
    for (; ctx->next_handler_cmd < cmds->nelts; ctx->next_handler_cmd++) {
        cmd = &cmd_elts[ctx->next_handler_cmd];

        /* evaluate arguments for the current cmd (if any) */
        if (cmd->args) {
            computed_args = ngx_array_create(r->pool, cmd->args->nelts,
                    sizeof(ngx_str_t));
            if (computed_args == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            rc = ngx_http_echo_eval_cmd_args(r, cmd, computed_args);
            if (rc != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "Failed to evaluate arguments for "
                        "the directive.");
                return rc;
            }
        }

        /* do command dispatch based on the opcode */
        switch (cmd->opcode) {
            case echo_opcode_echo:
                /* XXX moved the following code to a separate
                 * function */
                DD("found echo opcode");
                rc = ngx_http_echo_exec_echo(r, ctx, computed_args);
                if (rc != NGX_OK) {
                    return rc;
                }
                break;

            case echo_opcode_echo_request_body:
                rc = ngx_http_echo_exec_echo_request_body(r, ctx);
                if (rc != NGX_OK) {
                    return rc;
                }
                break;

            case echo_opcode_echo_location_async:
                DD("found opcode echo location async...");
                rc = ngx_http_echo_exec_echo_location_async(r, ctx,
                        computed_args);
                if (rc != NGX_OK) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "Failed to issue subrequest for "
                            "echo_location_async");
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                break;

            case echo_opcode_echo_location:
                rc = ngx_http_echo_exec_echo_location(r, ctx, computed_args);
                if (rc != NGX_OK) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                return NGX_OK;
                break;

            case echo_opcode_echo_subrequest_async:
                DD("found opcode echo subrequest async...");
                rc = ngx_http_echo_exec_echo_subrequest_async(r, ctx,
                        computed_args);
                if (rc != NGX_OK) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "Failed to issue subrequest for "
                            "echo_subrequest_async");
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                break;

            case echo_opcode_echo_subrequest:
                rc = ngx_http_echo_exec_echo_subrequest(r, ctx, computed_args);
                if (rc != NGX_OK) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                return NGX_OK;
                break;

            case echo_opcode_echo_sleep:
                return ngx_http_echo_exec_echo_sleep(r, ctx, computed_args);
                break;

            case echo_opcode_echo_flush:
                rc = ngx_http_echo_exec_echo_flush(r, ctx);

                if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    return rc;
                }

                if (rc == NGX_ERROR) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "Failed to flush the output", cmd->opcode);
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                break;

            case echo_opcode_echo_blocking_sleep:
                rc = ngx_http_echo_exec_echo_blocking_sleep(r, ctx,
                        computed_args);
                if (rc == NGX_ERROR) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "Failed to run blocking sleep", cmd->opcode);
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                break;

            case echo_opcode_echo_reset_timer:
                rc = ngx_http_echo_exec_echo_reset_timer(r, ctx);
                if (rc != NGX_OK) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                break;

            case echo_opcode_echo_duplicate:
                rc = ngx_http_echo_exec_echo_duplicate(r, ctx, computed_args);
                if (rc != NGX_OK) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                break;

            case echo_opcode_echo_read_request_body:
                return ngx_http_echo_exec_echo_read_request_body(r, ctx);
                break;

            case echo_opcode_echo_foreach_split:
                rc = ngx_http_echo_exec_echo_foreach_split(r, ctx, computed_args);
                if (rc != NGX_OK) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                break;

            case echo_opcode_echo_end:
                rc = ngx_http_echo_exec_echo_end(r, ctx);
                if (rc != NGX_OK) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                break;

            case echo_opcode_echo_abort_parent:
                return ngx_http_echo_exec_abort_parent(r, ctx);
                break;

            default:
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "Unknown opcode: %d", cmd->opcode);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
                break;
        }
    }

    return ngx_http_echo_send_chain_link(r, ctx, NULL /* indicate LAST */);
}

