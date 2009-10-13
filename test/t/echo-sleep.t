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



=== TEST 3: leading echo
--- config
    location /echo {
        echo before...;
        echo_sleep 0.01;
    }
--- request
    GET /echo
--- response_body
before...



=== TEST 4: trailing echo
--- config
    location /echo {
        echo_sleep 0.01;
        echo after...;
    }
--- request
    GET /echo
--- response_body
after...



=== TEST 5: two echos around sleep
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



=== TEST 6: interleaving sleep and echo
--- config
    location /echo {
        echo 1;
        echo_sleep 0.01;
        echo 2;
        echo_sleep 0.01;
    }
--- request
    GET /echo
--- response_body
1
2



=== TEST 7: interleaving sleep and echo with echo at the end...
--- config
    location /echo {
        echo 1;
        echo_sleep 0.01;
        echo 2;
        echo_sleep 0.01;
        echo 3;
    }
--- request
    GET /echo
--- response_body
1
2
3

