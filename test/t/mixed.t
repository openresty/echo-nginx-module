# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

run_tests();

__DATA__

=== TEST 1: echo before echo_client_request_headers
--- config
    location /echo {
        echo "headers:";
        echo $echo_client_request_headers;
    }
--- request
    GET /echo
--- response_body eval
"headers:
GET /echo HTTP/1.1\r
Host: localhost:\$ServerPort\r
User-Agent: Test::Nginx::Echo\r

"



=== TEST 2: echo_client_request_headers before echo
--- config
    location /echo {
        echo $echo_client_request_headers;
        echo "...these are the headers";
    }
--- request
    GET /echo
--- response_body eval
"GET /echo HTTP/1.1\r
Host: localhost:\$ServerPort\r
User-Agent: Test::Nginx::Echo\r

...these are the headers
"



=== TEST 3: echo & headers & echo
--- config
    location /echo {
        echo "headers are";
        echo $echo_client_request_headers;
        echo "...these are the headers";
    }
--- request
    GET /echo
--- response_body eval
"headers are
GET /echo HTTP/1.1\r
Host: localhost:\$ServerPort\r
User-Agent: Test::Nginx::Echo\r

...these are the headers
"



=== TEST 4: mixed with echo_duplicate
--- config
    location /mixed {
        echo hello;
        echo_duplicate 2 ---;
        echo_duplicate 1 ' END ';
        echo_duplicate 2 ---;
        echo;
    }
--- request
    GET /mixed
--- response_body
hello
------ END ------

