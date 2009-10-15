/* Copyright (C) by agentzh */

#define DDEBUG 0

#include "ddebug.h"
#include "ngx_http_echo_module.h"

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_log.h>
#include <stdlib.h>

static ngx_http_output_header_filter_pt ngx_http_next_header_filter = NULL;

static ngx_http_output_body_filter_pt ngx_http_next_body_filter = NULL;

static ngx_buf_t ngx_http_echo_space_buf;

static ngx_buf_t ngx_http_echo_newline_buf;

static ngx_int_t ngx_http_echo_init_ctx(ngx_http_request_t *r,
        ngx_http_echo_ctx_t **ctx_ptr);

/* (optional) filters initialization */
static ngx_int_t ngx_http_echo_filter_init(ngx_conf_t *cf);

static ngx_int_t ngx_http_echo_header_filter(ngx_http_request_t *r);

static ngx_int_t ngx_http_echo_body_filter(ngx_http_request_t *r, ngx_chain_t *in);

/* module init headler */
static ngx_int_t ngx_http_echo_init(ngx_conf_t *cf);

/* config init handler */
static void* ngx_http_echo_create_conf(ngx_conf_t *cf);

/* config directive handlers */
static char* ngx_http_echo_echo(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char* ngx_http_echo_echo_client_request_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static char* ngx_http_echo_echo_sleep(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);

static char* ngx_http_echo_echo_flush(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char* ngx_http_echo_echo_blocking_sleep(ngx_conf_t *cf,
        ngx_command_t *cmd, void *conf);

static char*
ngx_http_echo_helper(ngx_http_echo_opcode_t opcode,
        ngx_http_echo_cmd_category_t cat,
        ngx_conf_t *cf, ngx_command_t *cmd, void* conf);

/* main content handler */
static ngx_int_t ngx_http_echo_handler(ngx_http_request_t *r);

static ngx_int_t ngx_http_echo_exec_echo(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args);

static ngx_int_t ngx_http_echo_exec_echo_client_request_headers(
        ngx_http_request_t* r, ngx_http_echo_ctx_t *ctx);

static ngx_int_t ngx_http_echo_exec_echo_sleep(
        ngx_http_request_t *r, ngx_http_echo_ctx_t *ctx,
        ngx_array_t *computed_args);

static ngx_int_t ngx_http_echo_exec_echo_flush(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx);

static ngx_int_t ngx_http_echo_exec_echo_blocking_sleep(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args);

static ngx_int_t ngx_http_echo_eval_cmd_args(ngx_http_request_t *r,
        ngx_http_echo_cmd_t *cmd, ngx_array_t *computed_args);

static ngx_int_t ngx_http_echo_send_chain_link(ngx_http_request_t* r,
        ngx_http_echo_ctx_t *ctx, ngx_chain_t *cl);

/* write event handler for echo_sleep */
static void ngx_http_echo_post_sleep(ngx_http_request_t *r);

/* variable handler */
static ngx_int_t ngx_http_echo_timer_elapsed_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_echo_add_variables(ngx_conf_t *cf);

static ngx_http_variable_t ngx_http_echo_variables[] = {
    { ngx_string("echo_timer_elapsed"), NULL,
      ngx_http_echo_timer_elapsed_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 }
};

static ngx_command_t  ngx_http_echo_commands[] = {

    { ngx_string("echo"),
      NGX_HTTP_LOC_CONF|NGX_CONF_ANY,
      ngx_http_echo_echo,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_echo_loc_conf_t, handler_cmds),
      NULL },
    /* TODO echo_client_request_headers should take an
     * optional argument to change output format to
     * "html" or other things */
    { ngx_string("echo_client_request_headers"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_echo_echo_client_request_headers,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_echo_loc_conf_t, handler_cmds),
      NULL },
    { ngx_string("echo_sleep"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_echo_echo_sleep,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_echo_loc_conf_t, handler_cmds),
      NULL },
    { ngx_string("echo_flush"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_echo_echo_flush,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_echo_loc_conf_t, handler_cmds),
      NULL },
    { ngx_string("echo_blocking_sleep"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_echo_echo_blocking_sleep,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_echo_loc_conf_t, handler_cmds),
      NULL },
    /* TODO
    { ngx_string("echo_location"),
      NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_echo_echo_location,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    { ngx_string("echo_before_body"),
      NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_echo_echo_before_body,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    { ngx_string("echo_after_body"),
      NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_echo_echo_after_body,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    */
      ngx_null_command
};

static ngx_http_module_t ngx_http_echo_module_ctx = {
    /* TODO we could add our own variables here... */
    ngx_http_echo_init,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    ngx_http_echo_create_conf, /* create location configuration */
    NULL                           /* merge location configuration */
};

ngx_module_t ngx_http_echo_module = {
    NGX_MODULE_V1,
    &ngx_http_echo_module_ctx, /* module context */
    ngx_http_echo_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_echo_init(ngx_conf_t *cf) {
    static u_char space_str[]   = " ";
    static u_char newline_str[] = "\n";

    DD("global init...");

    ngx_memzero(&ngx_http_echo_space_buf, sizeof(ngx_buf_t));
    ngx_http_echo_space_buf.memory = 1;
    ngx_http_echo_space_buf.start =
        ngx_http_echo_space_buf.pos =
            space_str;
    ngx_http_echo_space_buf.end =
        ngx_http_echo_space_buf.last =
            space_str + sizeof(space_str) - 1;

    ngx_memzero(&ngx_http_echo_newline_buf, sizeof(ngx_buf_t));
    ngx_http_echo_newline_buf.memory = 1;
    ngx_http_echo_newline_buf.start =
        ngx_http_echo_newline_buf.pos =
            newline_str;
    ngx_http_echo_newline_buf.end =
        ngx_http_echo_newline_buf.last =
            newline_str + sizeof(newline_str) - 1;

    return ngx_http_echo_add_variables(cf);
}

static ngx_int_t
ngx_http_echo_filter_init (ngx_conf_t *cf) {
    if (ngx_http_next_header_filter == NULL) {
        ngx_http_next_header_filter = ngx_http_top_header_filter;
        ngx_http_top_header_filter  = ngx_http_echo_header_filter;
    }
    if (ngx_http_next_body_filter == NULL) {
        ngx_http_next_body_filter = ngx_http_top_body_filter;
        ngx_http_top_body_filter  = ngx_http_echo_body_filter;
    }
    return NGX_OK;
}

static void*
ngx_http_echo_create_conf(ngx_conf_t *cf) {
    ngx_http_echo_loc_conf_t *conf;
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_echo_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    return conf;
}

static char*
ngx_http_echo_helper(ngx_http_echo_opcode_t opcode,
        ngx_http_echo_cmd_category_t cat,
        ngx_conf_t *cf, ngx_command_t *cmd, void* conf) {
    ngx_http_core_loc_conf_t        *clcf;
    /* ngx_http_echo_loc_conf_t        *ulcf = conf; */
    ngx_array_t                     **args_ptr;
    ngx_http_script_compile_t       sc;
    ngx_str_t                       *raw_args;
    ngx_http_echo_arg_template_t    *arg;
    ngx_array_t                     **cmds_ptr;
    ngx_http_echo_cmd_t             *echo_cmd;
    ngx_uint_t                       i, n;

    /* cmds_ptr points to ngx_http_echo_loc_conf_t's
     * handler_cmds, before_body_cmds, or after_body_cmds
     * array, depending on the actual offset */
    cmds_ptr = (ngx_array_t**)(((u_char*)conf) + cmd->offset);
    if (*cmds_ptr == NULL) {
        *cmds_ptr = ngx_array_create(cf->pool, 1,
                sizeof(ngx_http_echo_cmd_t));
        if (*cmds_ptr == NULL) {
            return NGX_CONF_ERROR;
        }
        if (cat == echo_handler_cmd) {
            DD("registering the content handler");
            /* register the content handler */
            clcf = ngx_http_conf_get_module_loc_conf(cf,
                    ngx_http_core_module);
            if (clcf == NULL) {
                return NGX_CONF_ERROR;
            }
            DD("registering the content handler (2)");
            clcf->handler = ngx_http_echo_handler;
        } else {
            /* the init function itself will ensure
             * it does NOT initialize twice */
            ngx_http_echo_filter_init(cf);
        }
    }
    echo_cmd = ngx_array_push(*cmds_ptr);
    if (echo_cmd == NULL) {
        return NGX_CONF_ERROR;
    }
    echo_cmd->opcode = opcode;
    args_ptr = &echo_cmd->args;
    *args_ptr = ngx_array_create(cf->pool, 1,
            sizeof(ngx_http_echo_arg_template_t));
    if (*args_ptr == NULL) {
        return NGX_CONF_ERROR;
    }
    raw_args = cf->args->elts;
    /* we skip the first arg and start from the second */
    for (i = 1 ; i < cf->args->nelts; i++) {
        arg = ngx_array_push(*args_ptr);
        if (arg == NULL) {
            return NGX_CONF_ERROR;
        }
        arg->raw_value = raw_args[i];
        DD("found raw arg %s", raw_args[i].data);
        arg->lengths = NULL;
        arg->values  = NULL;
        n = ngx_http_script_variables_count(&arg->raw_value);
        if (n > 0) {
            ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
            sc.cf = cf;
            sc.source = &arg->raw_value;
            sc.lengths = &arg->lengths;
            sc.values = &arg->values;
            sc.variables = n;
            sc.complete_lengths = 1;
            sc.complete_values = 1;
            if (ngx_http_script_compile(&sc) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }
    } /* end for */
    return NGX_CONF_OK;
}

static char*
ngx_http_echo_echo(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    DD("in echo_echo...");
    return ngx_http_echo_helper(echo_opcode_echo,
            echo_handler_cmd,
            cf, cmd, conf);
}

static char*
ngx_http_echo_echo_client_request_headers(ngx_conf_t *cf,
        ngx_command_t *cmd, void *conf) {
    return ngx_http_echo_helper(
            echo_opcode_echo_client_request_headers,
            echo_handler_cmd,
            cf, cmd, conf);
}

static char*
ngx_http_echo_echo_sleep(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    DD("in echo_sleep...");
    return ngx_http_echo_helper(echo_opcode_echo_sleep,
            echo_handler_cmd,
            cf, cmd, conf);
}

static char*
ngx_http_echo_echo_flush(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    DD("in echo_flush...");
    return ngx_http_echo_helper(echo_opcode_echo_flush,
            echo_handler_cmd,
            cf, cmd, conf);
}

static char*
ngx_http_echo_echo_blocking_sleep(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    DD("in echo_blocking_sleep...");
    return ngx_http_echo_helper(echo_opcode_echo_blocking_sleep,
            echo_handler_cmd,
            cf, cmd, conf);
}

static ngx_int_t
ngx_http_echo_init_ctx(ngx_http_request_t *r, ngx_http_echo_ctx_t **ctx_ptr) {
    *ctx_ptr = ngx_pcalloc(r->pool, sizeof(ngx_http_echo_ctx_t));
    if (*ctx_ptr == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    return NGX_OK;
}

static ngx_int_t
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
                        "the \"echo\" directive.");
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
            case echo_opcode_echo_client_request_headers:
                rc = ngx_http_echo_exec_echo_client_request_headers(r,
                        ctx);
                if (rc != NGX_OK) {
                    return rc;
                }
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
            default:
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "Unknown opcode: %d", cmd->opcode);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
                break;
        }
    }

    return ngx_http_echo_send_chain_link(r, ctx, NULL /* indicate LAST */);
}

static ngx_int_t
ngx_http_echo_exec_echo(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args) {
    ngx_uint_t                  i;

    ngx_buf_t                   *space_buf;
    ngx_buf_t                   *newline_buf;
    ngx_buf_t                   *buf;

    ngx_str_t                   *computed_arg;
    ngx_str_t                   *computed_arg_elts;

    ngx_chain_t *cl  = NULL; /* the head of the chain link */
    ngx_chain_t **ll = NULL;  /* always point to the address of the last link */

    if (computed_args == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    computed_arg_elts = computed_args->elts;
    for (i = 0; i < computed_args->nelts; i++) {
        computed_arg = &computed_arg_elts[i];
        buf = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        if (buf == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        buf->start = buf->pos = computed_arg->data;
        buf->last = buf->end = computed_arg->data +
            computed_arg->len;
        buf->memory = 1;

        if (cl == NULL) {
            cl = ngx_alloc_chain_link(r->pool);
            if (cl == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            cl->buf  = buf;
            cl->next = NULL;
            ll = &cl->next;
        } else {
            /* append a space first */
            *ll = ngx_alloc_chain_link(r->pool);
            if (*ll == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            space_buf = ngx_calloc_buf(r->pool);
            if (space_buf == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            /* nginx clears buf flags at the end of each request handling,
             * so we have to make a clone here. */
            *space_buf = ngx_http_echo_space_buf;

            (*ll)->buf = space_buf;
            (*ll)->next = NULL;

            ll = &(*ll)->next;

            /* then append the buf */
            *ll = ngx_alloc_chain_link(r->pool);
            if (*ll == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            (*ll)->buf  = buf;
            (*ll)->next = NULL;

            ll = &(*ll)->next;
        }
    } /* end for */

    /* append the newline character */
    /* TODO add support for -n option to suppress
     * the trailing newline */
    newline_buf = ngx_calloc_buf(r->pool);
    if (newline_buf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    *newline_buf = ngx_http_echo_newline_buf;

    if (cl == NULL) {
        cl = ngx_alloc_chain_link(r->pool);
        if (cl == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        cl->buf = newline_buf;
        cl->next = NULL;
        /* ll = &cl->next; */
    } else {
        *ll = ngx_alloc_chain_link(r->pool);
        if (*ll == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        (*ll)->buf  = newline_buf;
        (*ll)->next = NULL;
        /* ll = &(*ll)->next; */
    }

    return ngx_http_echo_send_chain_link(r, ctx, cl);
}

static ngx_int_t
ngx_http_echo_send_chain_link(ngx_http_request_t* r,
        ngx_http_echo_ctx_t *ctx, ngx_chain_t *cl) {
    ngx_int_t   rc;

    if ( ! ctx->headers_sent ) {
        ctx->headers_sent = 1;
        r->headers_out.status = NGX_HTTP_OK;
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        rc = ngx_http_send_header(r);
        if (r->header_only || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }
    }

    if (cl == NULL) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }
        return NGX_OK;
    }

    return ngx_http_output_filter(r, cl);
}

static ngx_int_t
ngx_http_echo_header_filter(ngx_http_request_t *r) {
    /* TODO */
    return NGX_OK;
}

static ngx_int_t
ngx_http_echo_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
    /* TODO */
    return NGX_OK;
}

static ngx_int_t ngx_http_echo_eval_cmd_args(ngx_http_request_t *r,
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

static ngx_int_t
ngx_http_echo_exec_echo_client_request_headers(
        ngx_http_request_t* r, ngx_http_echo_ctx_t *ctx) {
    size_t                      size;
    u_char                      *c;
    ngx_buf_t                   *header_in;
    ngx_chain_t                 *cl  = NULL;
    ngx_buf_t                   *buf;

    DD("echo_client_request_headers triggered!");
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    cl->next = NULL;

    if (r != r->main) {
        header_in = r->main->header_in;
    } else {
        header_in = r->header_in;
    }
    if (header_in == NULL) {
        DD("header_in is NULL");
        return NGX_HTTP_BAD_REQUEST;
    }
    size = header_in->pos - header_in->start;
    //DD("!!! size: %lu", (unsigned long)size);

    buf = ngx_create_temp_buf(r->pool, size);
    buf->last = ngx_cpymem(buf->start, header_in->start, size);
    buf->memory = 1;

    /* fix \0 introduced by the nginx header parser */
    for (c = (u_char*)buf->start; c != buf->last; c++) {
        if (*c == '\0') {
            if (c + 1 != buf->last && *(c + 1) == LF) {
                *c = CR;
            } else {
                *c = ':';
            }
        }
    }

    cl->buf = buf;
    return ngx_http_echo_send_chain_link(r, ctx, cl);
}

static ngx_int_t
ngx_http_echo_exec_echo_sleep(
        ngx_http_request_t *r, ngx_http_echo_ctx_t *ctx,
        ngx_array_t *computed_args) {
    ngx_str_t                   *computed_arg;
    ngx_str_t                   *computed_arg_elts;
    float                       delay; /* in sec */

    computed_arg_elts = computed_args->elts;
    computed_arg = &computed_arg_elts[0];
    delay = atof( (char*) computed_arg->data );
    if (delay < 0.001) { /* should be bigger than 1 msec */
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                   "invalid sleep duration \"%V\"", &computed_arg_elts[0]);
        return NGX_HTTP_BAD_REQUEST;
    }

    DD("DELAY = %d sec", delay);
    if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* r->read_event_handler = ngx_http_test_reading; */
    r->write_event_handler = ngx_http_echo_post_sleep;

#if defined(nginx_version) && nginx_version >= 8011
    r->main->count++;
    DD("request main count : %d", r->main->count);
#endif

    ngx_add_timer(r->connection->write, (ngx_msec_t) (1000 * delay));

    return NGX_DONE;
}

static void
ngx_http_echo_post_sleep(ngx_http_request_t *r) {
    ngx_event_t                 *wev;
    ngx_http_echo_ctx_t         *ctx;

    wev = r->connection->write;

    if (!wev->timedout) {
        if (ngx_handle_write_event(wev, 0) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        }
        return;
    }

    wev->timedout = 0;

    if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_echo_module);
    if (ctx == NULL) {
        return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }

    ctx->next_handler_cmd++;

    r->read_event_handler = ngx_http_block_reading;
    r->write_event_handler = ngx_http_request_empty_handler;

    ngx_http_finalize_request(r, ngx_http_echo_handler(r));
    return;
}

static ngx_int_t
ngx_http_echo_exec_echo_flush(ngx_http_request_t *r, ngx_http_echo_ctx_t *ctx) {
    return ngx_http_send_special(r, NGX_HTTP_FLUSH);
}

static ngx_int_t
ngx_http_echo_exec_echo_blocking_sleep(ngx_http_request_t *r,
        ngx_http_echo_ctx_t *ctx, ngx_array_t *computed_args) {
    ngx_str_t                   *computed_arg;
    ngx_str_t                   *computed_arg_elts;
    float                       delay; /* in sec */

    computed_arg_elts = computed_args->elts;
    computed_arg = &computed_arg_elts[0];
    delay = atof( (char*) computed_arg->data );
    if (delay < 0.001) { /* should be bigger than 1 msec */
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                   "invalid sleep duration \"%V\"", &computed_arg_elts[0]);
        return NGX_HTTP_BAD_REQUEST;
    }

    DD("blocking DELAY = %d sec", delay);

    ngx_msleep((ngx_msec_t) (1000 * delay));
    return NGX_OK;
}

static ngx_int_t
ngx_http_echo_timer_elapsed_variable(ngx_http_request_t *r,
        ngx_http_variable_value_t *v, uintptr_t data) {
    ngx_http_echo_ctx_t     *ctx;
    ngx_msec_int_t          ms;
    u_char                  *p;
    ngx_time_t              *tp;
    size_t                  size;

    ctx = ngx_http_get_module_ctx(r, ngx_http_echo_module);
    if (ctx->timer_begin == NULL) {
        ctx->timer_begin = ngx_palloc(r->pool, sizeof(ngx_time_t));
        if (ctx->timer_begin == NULL) {
            return NGX_ERROR;
        }
        ctx->timer_begin->sec  = r->start_sec;
        ctx->timer_begin->msec = (ngx_msec_t) r->start_msec;
    }

    tp = ngx_timeofday();

    ms = (ngx_msec_int_t)
             ((tp->sec - r->start_sec) * 1000 + (tp->msec - r->start_msec));
    ms = (ms >= 0) ? ms : 0;

    size = sizeof("-9223372036854775808.000") - 1;
    p = ngx_palloc(r->pool, size);
    v->len = ngx_snprintf(p, size, "%T.%03M",
             ms / 1000, ms % 1000) - p;
    v->data = p;

    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;
    return NGX_OK;
}

static ngx_int_t
ngx_http_echo_add_variables(ngx_conf_t *cf) {
    ngx_http_variable_t *var, *v;
    for (v = ngx_http_echo_variables; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }
        var->get_handler = v->get_handler;
        var->data = v->data;
    }
    return NGX_OK;
}

