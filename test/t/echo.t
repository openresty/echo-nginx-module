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
        echo hello;
    }
--- request
    GET /echo
--- response_body
hello



=== TEST 2: multiple args
--- config
    location /echo {
        echo say hello world;
    }
--- request
    GET /echo
--- response_body
say hello world



=== TEST 3: multiple directive instances
--- config
    location /echo {
        echo say that;
        echo hello;
        echo world !;
    }
--- request
    GET /echo
--- response_body
say that
hello
world !


=== TEST 4: echo without arguments
--- config
    location /echo {
        echo;
        echo;
    }
--- request
    GET /echo
--- response_body eval
"\n\n"

