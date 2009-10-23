# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

#$Test::Nginx::Echo::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: sanity
--- config
    location /main {
        echo_location /sub;
    }
    location /sub {
        echo hello;
    }
--- request
    GET /main
--- response_body
hello


=== TEST 1: sanity with proxy in the middle
--- config
    location /main {
        echo_location /proxy;
    }
    location /proxy {
        proxy_pass $scheme://127.0.0.1:$server_port/sub;
    }
    location /sub {
        echo hello;
    }
--- request
    GET /main
--- response_body
hello



=== TEST 2: trailing echo
--- config
    location /main {
        echo_location /sub;
        echo after subrequest;
    }
    location /sub {
        echo hello;
    }
--- request
    GET /main
--- response_body
hello
after subrequest



=== TEST 3: leading echo
--- config
    location /main {
        echo before subrequest;
        echo_location /sub;
    }
    location /sub {
        echo hello;
    }
--- request
    GET /main
--- response_body
before subrequest
hello



=== TEST 4: leading & trailing echo
--- config
    location /main {
        echo before subrequest;
        echo_location /sub;
        echo after subrequest;
    }
    location /sub {
        echo hello;
    }
--- request
    GET /main
--- response_body
before subrequest
hello
after subrequest



=== TEST 5: multiple subrequests
--- config
    location /main {
        echo before sr 1;
        echo_location /sub;
        echo after sr 1;
        echo before sr 2;
        echo_location /sub;
        echo after sr 2;
    }
    location /sub {
        echo hello;
    }
--- request
    GET /main
--- response_body
before sr 1
hello
after sr 1
before sr 2
hello
after sr 2



=== TEST 6: timed multiple subrequests (blocking sleep)
--- config
    location /main {
        echo_reset_timer;
        echo_location /sub1;
        echo_location /sub2;
        echo "took $echo_timer_elapsed sec for total.";
    }
    location /sub1 {
        echo_blocking_sleep 0.02;
        echo hello;
    }
    location /sub2 {
        echo_blocking_sleep 0.01;
        echo world;
    }

--- request
    GET /main
--- response_body_like
^hello
world
took 0\.0(?:2[5-9]|3[0-5]) sec for total\.$



=== TEST 7: timed multiple subrequests (non-blocking sleep)
--- config
    location /main {
        echo_reset_timer;
        echo_location /sub1;
        echo_location /sub2;
        echo "took $echo_timer_elapsed sec for total.";
    }
    location /sub1 {
        echo_sleep 0.02;
        echo hello;
    }
    location /sub2 {
        echo_sleep 0.01;
        echo world;
    }

--- request
    GET /main
--- response_body_like
^hello
world
took 0\.0(?:2[5-9]|3[0-5]) sec for total\.$



=== TEST 8: location with args
--- config
    location /main {
        echo_location /sub 'foo=Foo&bar=Bar';
    }
    location /sub {
        echo $arg_foo $arg_bar;
    }
--- request
    GET /main
--- response_body
Foo Bar



=== TEST 9: chained subrequests
--- config
    location /main {
        echo 'pre main';
        echo_location /sub;
        echo 'post main';
    }

    location /sub {
        echo 'pre sub';
        echo_location /subsub;
        echo 'post sub';
    }

    location /subsub {
        echo 'subsub';
    }
--- request
    GET /main
--- response_body
pre main
pre sub
subsub
post sub
post main



=== TEST 10: chained subrequests using named locations
as of 0.8.20, ngx_http_subrequest still does not support
named location. sigh. this case is a TODO.
--- config
    location /main {
        echo 'pre main';
        echo_location @sub;
        echo 'post main';
    }

    location @sub {
        echo 'pre sub';
        echo_location @subsub;
        echo 'post sub';
    }

    location @subsub {
        echo 'subsub';
    }
--- request
    GET /main
--- response_body
pre main
pre sub
subsub
post sub
post main
--- SKIP



=== TEST 11: explicit flush in main request
--- config
    location /main {
        echo 'pre main';
        echo_location /sub;
        echo 'post main';
        echo_flush;
    }

    location /sub {
        echo_sleep 0.02;
        echo 'sub';
    }
--- request
    GET /main
--- response_body
pre main
sub
post main

