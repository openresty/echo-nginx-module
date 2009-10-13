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
        echo_sleep 1;
    }
--- request
    GET /echo
--- response_body



=== TEST 2: fractional delay
--- config
    location /echo {
        echo_sleep 0.01;
    }
--- request
    GET /echo
--- response_body
--- SKIP

