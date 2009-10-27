# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

#$Test::Nginx::Echo::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: sanity
--- config
    location /main {
        echo "main pre: $echo_incr";
        echo_location /sub;
        echo_location /sub;
        echo "main post: $echo_incr";
    }
    location /sub {
        echo "sub: $echo_incr";
    }
--- request
    GET /main
--- response_body
main pre: 1
sub: 2
sub: 3
main post: 4

