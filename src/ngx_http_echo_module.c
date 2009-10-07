#define DDEBUG 1

#include "ddebug.h"
#include "ngx_http_echo_module.h"

#include <ngx_config.h>
#include <ngx_log.h>

static ngx_http_output_header_filter_pt ngx_http_next_header_filter = NULL;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter = NULL;

static ngx_buf_t ngx_http_echo_space_buf;
static ngx_buf_t ngx_http_echo_newline_buf;

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
static char* ngx_http_echo_echo_sleep(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
static char* ngx_http_echo_echo_client_request_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char*
ngx_http_echo_helper(ngx_http_echo_opcode_t opcode,
        ngx_http_echo_cmd_category_t cat,
        ngx_conf_t *cf, ngx_command_t *cmd, void* conf);

/* main content handler */
static ngx_int_t ngx_http_echo_handler(ngx_http_request_t *r);

static ngx_int_t ngx_http_echo_eval_cmd_args(ngx_http_request_t *r,
        ngx_http_echo_cmd_t *cmd, ngx_array_t *computed_args);

/* write event handler for echo_sleep */
static void ngx_http_echo_req_delay(ngx_http_request_t *r);

static ngx_command_t  ngx_http_echo_commands[] = {

    { ngx_string("echo"),
      NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
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
    /* TODO
    { ngx_string("echo_sleep"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_echo_echo_sleep,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_echo_loc_conf_t, handler_cmds),
      NULL },
    { ngx_string("echo_client_request_body"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_echo_echo_client_request_body,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
      */
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
    static char space_str[]   = " ";
    static char newline_str[] = "\n";

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

    return NGX_OK;
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
    ngx_http_echo_loc_conf_t        *ulcf = conf;
    ngx_array_t                     **args_ptr;
    ngx_http_script_compile_t       sc;
    ngx_str_t                       *raw_args;
    ngx_http_echo_arg_template_t    *arg;
    ngx_array_t                     **cmds_ptr;
    ngx_http_echo_cmd_t             *echo_cmd;
    ngx_int_t                       i, n;

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
ngx_http_echo_echo_sleep(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    DD("in echo_sleep...");
    return ngx_http_echo_helper(echo_opcode_echo_sleep,
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

static ngx_int_t
ngx_http_echo_handler(ngx_http_request_t *r) {
    ngx_http_echo_loc_conf_t    *elcf;
    ngx_http_echo_ctx_t         *ctx;
    ngx_int_t                   rc;
    ngx_array_t                 *cmds, *computed_args;
    ngx_chain_t                 *cl = NULL;
    ngx_chain_t                 *last_cl = NULL;
    ngx_chain_t                 *temp_cl;
    ngx_chain_t                 *temp_first_cl;
    ngx_chain_t                 *temp_last_cl;
    ngx_buf_t                   *buf;
    ngx_http_echo_cmd_t         *cmd;
    ngx_http_echo_cmd_t         *cmd_elts;
    ngx_str_t                   *computed_arg;
    ngx_str_t                   *computed_arg_elts;
    ngx_int_t                   i;
    ngx_buf_t                   *header_in;
    ngx_int_t                   delay;

    /* nginx clears buf flags at the end of each request handling */
    ngx_buf_t                   *space_buf;
    ngx_buf_t                   *newline_buf;

    size_t                      size;
    u_char                      *c;

    elcf = ngx_http_get_module_loc_conf(r, ngx_http_echo_module);
    cmds = elcf->handler_cmds;
    if (cmds == NULL) {
        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_echo_module);
    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_echo_ctx_t));
        if (ctx == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ngx_http_set_ctx(r, ctx, ngx_http_echo_module);
    }

    if (ctx->next_handler_cmd == 0) {
        r->headers_out.status = NGX_HTTP_OK;
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        rc = ngx_http_send_header(r);
        if (r->header_only || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }
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

        //DD("space buf .memory: %d", ngx_http_echo_space_buf->memory);
        //DD("space buf .pos: '%s'", ngx_http_echo_space_buf->pos);

        /* do command dispatch based on the opcode */
        switch (cmd->opcode) {
            case echo_opcode_echo:
                /* XXX moved the following code to a separate
                 * function */
                DD("found echo opcode");
                temp_first_cl = temp_last_cl = NULL;
                if (computed_args->nelts == 0) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "Found 0 evaluated argument for "
                            "the \"echo\" directive.");
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

                    temp_cl = ngx_alloc_chain_link(r->pool);
                    if (temp_cl == NULL) {
                        return NGX_HTTP_INTERNAL_SERVER_ERROR;
                    }
                    temp_cl->buf  = buf;
                    temp_cl->next = NULL;

                    if (temp_first_cl == NULL) {
                        temp_first_cl = temp_last_cl = temp_cl;
                    } else {
                        temp_last_cl->next = ngx_alloc_chain_link(r->pool);
                        if (temp_last_cl->next == NULL) {
                            return NGX_HTTP_INTERNAL_SERVER_ERROR;
                        }
                        space_buf = ngx_calloc_buf(r->pool);
                        if (space_buf == NULL) {
                            return NGX_HTTP_INTERNAL_SERVER_ERROR;
                        }
                        *space_buf = ngx_http_echo_space_buf;
                        temp_last_cl->next->buf = space_buf;
                        temp_last_cl->next->next = temp_cl;
                        temp_last_cl = temp_cl;
                    }
                } /* end for */

                /* append the newline character */
                /* TODO add support for -n option to suppress
                 * the trailing newline */
                if (temp_last_cl == NULL) {
                    temp_last_cl = temp_first_cl = ngx_alloc_chain_link(r->pool);
                    if (temp_last_cl == NULL) {
                        return NGX_HTTP_INTERNAL_SERVER_ERROR;
                    }
                } else {
                    temp_last_cl->next = ngx_alloc_chain_link(r->pool);
                }
                if (temp_last_cl->next == NULL) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                newline_buf = ngx_calloc_buf(r->pool);
                if (newline_buf == NULL) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                *newline_buf = ngx_http_echo_newline_buf;
                temp_last_cl->next->buf = newline_buf;
                temp_last_cl->next->next = NULL;
                temp_last_cl = temp_last_cl->next;

                if (cl == NULL) {
                    DD("found NULL cl, setting cl to temp_first_cl...");
                    cl = temp_first_cl;
                    last_cl = temp_last_cl;
                } else {
                    last_cl->next = temp_first_cl;
                    last_cl = temp_last_cl;
                }
                break;
            case echo_opcode_echo_sleep:
                computed_arg_elts = computed_args->elts;
                computed_arg = &computed_arg_elts[0];
                delay = ngx_atoi(computed_arg->data, computed_arg->len);
                if (delay <= 0) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                               "invalid sleep duration \"%V\"", &computed_arg_elts[0]);
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                DD("DELAY = %d sec", delay);
                if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                r->read_event_handler = ngx_http_block_reading;
                r->write_event_handler = ngx_http_echo_req_delay;
                ngx_add_timer(r->connection->write, (ngx_msec_t) delay * 1000);

                return NGX_AGAIN;
                break;
            case echo_opcode_echo_client_request_headers:
                DD("echo_client_request_headers triggered!");
                /* XXX moved the following code to a separate
                 * function */
                temp_first_cl = ngx_alloc_chain_link(r->pool);
                if (temp_first_cl == NULL) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                temp_first_cl->next = NULL;

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
                temp_first_cl->buf = buf;

                if (cl == NULL) {
                    DD("found NULL cl, setting cl to temp_first_cl...");
                    cl = last_cl = temp_first_cl;
                } else {
                    last_cl->next = temp_first_cl;
                    last_cl = last_cl->next;
                }

                break;
            default:
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "Unknown opcode: %d", cmd->opcode);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
                break;
        }
    }
    if (last_cl) {
        last_cl->buf->last_buf = 1;
    }

    if (cl == NULL) {
        DD("cl is NULL");
        return ngx_http_send_special(r, NGX_HTTP_LAST);
    }
    DD("sending last...");
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
    ngx_int_t                       i;
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

static void
ngx_http_echo_req_delay(ngx_http_request_t *r) {
    ngx_event_t  *wev;
    DD("!! req delaying");
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

    r->read_event_handler = ngx_http_block_reading;
    r->write_event_handler = ngx_http_core_run_phases;

    ngx_http_core_run_phases(r);
}

