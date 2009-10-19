# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

$Test::Nginx::Echo::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: sanity
--- config
    location /main {
        echo_location_async /sub;
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
        echo_location_async /sub;
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
        echo_location_async /sub;
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
        echo_location_async /sub;
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
        echo_location_async /sub;
        echo after sr 1;
        echo before sr 2;
        echo_location_async /sub;
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
        echo_location_async /sub1;
        echo_location_async /sub2;
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
took 0\.00[0-5] sec for total\.$



=== TEST 7: timed multiple subrequests (non-blocking sleep)
--- config
    location /main {
        echo_reset_timer;
        echo_location_async /sub1;
        echo_location_async /sub2;
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
took 0\.00[0-5] sec for total\.$
--- SKIP

