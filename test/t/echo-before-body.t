# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

#$Test::Nginx::Echo::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: sanity
--- config
    location /echo {
        echo_before_body hello;
        echo world;
    }
--- request
    GET /echo
--- response_body
hello
world



=== TEST 2: echo before proxy
--- config
    location /echo {
        echo_before_body hello;
        proxy_pass $scheme://127.0.0.1:$server_port$request_uri/more;
    }
    location /echo/more {
        echo world;
    }
--- request
    GET /echo
--- response_body
hello
world



=== TEST 3: with variables
--- config
    location /echo {
        echo_before_body $request_method;
        echo world;
    }
--- request
    GET /echo
--- response_body
GET
world



=== TEST 4: w/o args
--- config
    location /echo {
        echo_before_body;
        echo world;
    }
--- request
    GET /echo
--- response_body eval
"\nworld\n"



=== TEST 5: order is not important
--- config
    location /reversed {
        echo world;
        echo_before_body hello;
    }
--- request
    GET /reversed
--- response_body
hello
world



=== TEST 6: multiple echo_before_body instances
--- config
    location /echo {
        echo_before_body hello;
        echo_before_body world;
        echo !;
    }
--- request
    GET /echo
--- response_body
hello
world
!



=== TEST 7: multiple echo_before_body instances with multiple echo cmds
--- config
    location /echo {
        echo_before_body hello;
        echo_before_body world;
        echo i;
        echo say;
    }
--- request
    GET /echo
--- response_body
hello
world
i
say

