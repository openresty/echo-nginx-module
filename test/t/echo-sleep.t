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


=== TEST 2: leading echo
--- config
    location /echo {
        echo before...;
        echo_sleep 0.01;
    }
--- request
    GET /echo
--- response_body
before...


=== TEST 3: trailing echo
--- config
    location /echo {
        echo_sleep 0.01;
        echo after...;
    }
--- request
    GET /echo
--- response_body
after...


=== TEST 3: two echos around sleep
--- config
    location /echo {
        echo before...;
        echo_sleep 0.01;
        echo after...;
    }
--- request
    GET /echo
--- response_body
before...
after...

