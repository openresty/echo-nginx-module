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
        echo_after_body hello;
        echo world;
    }
--- request
    GET /echo
--- response_body
world
hello



=== TEST 2: echo after proxy
--- config
    location /echo {
        echo_after_body hello;
        proxy_pass http://127.0.0.1:$server_port$request_uri/more;
    }
    location /echo/more {
        echo world;
    }
--- request
    GET /echo
--- response_body
world
hello



=== TEST 3: with variables
--- config
    location /echo {
        echo_after_body $request_method;
        echo world;
    }
--- request
    GET /echo
--- response_body
world
GET



=== TEST 4: w/o args
--- config
    location /echo {
        echo_after_body;
        echo world;
    }
--- request
    GET /echo
--- response_body eval
"world\n\n"



=== TEST 5: order is not important
--- config
    location /reversed {
        echo world;
        echo_after_body hello;
    }
--- request
    GET /reversed
--- response_body
world
hello



=== TEST 6: multiple echo_after_body instances
--- config
    location /echo {
        echo_after_body hello;
        echo_after_body world;
        echo !;
    }
--- request
    GET /echo
--- response_body
!
hello
world



=== TEST 7: multiple echo_after_body instances with multiple echo cmds
--- config
    location /echo {
        echo_after_body hello;
        echo_after_body world;
        echo i;
        echo say;
    }
--- request
    GET /echo
--- response_body
i
say
hello
world



=== TEST 8: echo-after-body & echo-before-body
--- config
    location /mixed {
        echo_before_body hello;
        echo_after_body world;
        echo_before_body hiya;
        echo_after_body igor;
        echo ////////;
    }
--- request
    GET /mixed
--- response_body
hello
hiya
////////
world
igor



=== TEST 9: echo around proxy
--- config
    location /echo {
        echo_before_body hello;
        echo_before_body world;
        #echo $scheme://$host:$server_port$request_uri/more;
        proxy_pass $scheme://127.0.0.1:$server_port$request_uri/more;
        echo_after_body hiya;
        echo_after_body igor;
    }
    location /echo/more {
        echo blah;
    }
--- request
    GET /echo
--- response_body
hello
world
blah
hiya
igor

