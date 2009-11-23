# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

#$Test::Nginx::Echo::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: exec normal location
--- config
    location /main {
        echo_exec /bar;
        echo end;
    }
    location = /bar {
        echo bar;
    }
--- request
    GET /main
--- response_body
bar



=== TEST 2: location with args (inlined in uri)
--- config
    location /main {
        echo_exec /bar?a=32;
        echo end;
    }
    location /bar {
        echo "a: [$arg_a]";
    }
--- request
    GET /main
--- response_body
a: [32]



=== TEST 3: location with args (in separate arg)
--- config
    location /main {
        echo_exec /bar a=56;
        echo end;
    }
    location /bar {
        echo "a: [$arg_a]";
    }
--- request
    GET /main
--- response_body
a: [56]



=== TEST 4: exec named location
--- config
    location /main {
        echo_exec @bar;
        echo end;
    }
    location @bar {
        echo bar;
    }
--- request
    GET /main
--- response_body
bar



=== TEST 5: query string ignored for named locations
--- config
    location /main {
        echo_exec @bar?a=32;
        echo end;
    }
    location @bar {
        echo "a: [$arg_a]";
    }
--- request
    GET /main
--- response_body
a: []

