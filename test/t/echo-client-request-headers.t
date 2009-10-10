# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

run_tests();

__DATA__

=== TEST 1: standalone directive
--- config
    location /echo {
        echo_client_request_headers;
    }
--- request
    GET /echo
--- response_body eval
"GET /echo HTTP/1.1\r
Host: localhost:\$ServerPort\r
User-Agent: Test::Nginx::Echo\r
\r
"


=== TEST 2: multiple instances
--- config
    location /echo {
        echo_client_request_headers;
        echo_client_request_headers;
    }
--- request
    GET /echo
--- response_body eval
"GET /echo HTTP/1.1\r
Host: localhost:\$ServerPort\r
User-Agent: Test::Nginx::Echo\r
\r
GET /echo HTTP/1.1\r
Host: localhost:\$ServerPort\r
User-Agent: Test::Nginx::Echo\r
\r
"
