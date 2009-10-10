use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

run_tests();

__DATA__

=== TEST 1: AUTHOR
--- config
    location /echo {
        echo hello;
    }
--- request
    GET /echo
--- response_body
hello

