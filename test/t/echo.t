# vi:filetype=perl

use lib 'lib';
use Test::Nginx::LWP;

plan tests => 2 * blocks();

#$Test::Nginx::LWP::LogLevel = 'debug';

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



=== TEST 5: escaped newline
--- config
    location /echo {
        echo "hello\nworld";
    }
--- request
    GET /echo
--- response_body
hello
world



=== TEST 6: escaped tabs and \r and " wihtin "..."
--- config
    location /echo {
        echo "i say \"hello\tworld\"\r";
    }
--- request
    GET /echo
--- response_body eval: "i say \"hello\tworld\"\r\n"



=== TEST 7: escaped tabs and \r and " in single quotes
--- config
    location /echo {
        echo 'i say \"hello\tworld\"\r';
    }
--- request
    GET /echo
--- response_body eval: "i say \"hello\tworld\"\r\n"



=== TEST 8: escaped tabs and \r and " w/o any quotes
--- config
    location /echo {
        echo i say \"hello\tworld\"\r;
    }
--- request
    GET /echo
--- response_body eval: "i say \"hello\tworld\"\r\n"



=== TEST 9: escaping $
As of Nginx 0.8.20, there's still no way to escape the '$' character.
--- config
    location /echo {
        echo \$;
    }
--- request
    GET /echo
--- response_body
$
--- SKIP



=== TEST 10: XSS
--- config
    location /blah {
        echo_duplicate 1 "$arg_callback(";
        echo_location_async "/data?$uri";
        echo_duplicate 1 ")";
    }
    location /data {
        echo_duplicate 1 '{"dog":"$query_string"}';
    }
--- request
    GET /blah/9999999.json?callback=ding1111111
--- response_body chomp
ding1111111({"dog":"/blah/9999999.json"})

