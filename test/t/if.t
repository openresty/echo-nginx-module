# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

#$Test::Nginx::Echo::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: sanity (hit)
--- config
    location ^~ /if {
        set $res miss;
        if ($arg_val ~* '^a') {
            set $res hit;
            echo $res;
        }
        echo $res;
    }
--- request
    GET /if?val=abc
--- response_body
hit



=== TEST 2: sanity (miss)
--- config
    location ^~ /if {
        set $res miss;
        if ($arg_val ~* '^a') {
            set $res hit;
            echo $res;
        }
        echo $res;
    }
--- request
    GET /if?val=bcd
--- response_body
miss



=== TEST 3: proxy in if (hit)
--- config
    location ^~ /if {
        set $res miss;
        if ($arg_val ~* '^a') {
            set $res hit;
            proxy_pass $scheme://127.0.0.1:$server_port/foo?res=$res;
        }
        proxy_pass $scheme://127.0.0.1:$server_port/foo?res=$res;
    }
    location /foo {
        echo "res = $arg_res";
    }
--- request
    GET /if?val=abc
--- response_body
res = hit



=== TEST 4: proxy in if (miss)
--- config
    location ^~ /if {
        set $res miss;
        if ($arg_val ~* '^a') {
            set $res hit;
            proxy_pass $scheme://127.0.0.1:$server_port/foo?res=$res;
        }
        proxy_pass $scheme://127.0.0.1:$server_port/foo?res=$res;
    }
    location /foo {
        echo "res = $arg_res";
    }
--- request
    GET /if?val=bcd
--- response_body
res = miss

