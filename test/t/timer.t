# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Echo;

plan tests => 1 * blocks();

#$Test::Nginx::Echo::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: timer without explicit reset
--- config
    location /timer {
        echo_sleep 0.03;
        echo "elapsed $echo_timer_elapsed sec.";
    }
--- request
    GET /timer
--- response_body_like
^elapsed 0\.0(3[0-4]|2[6-9]) sec\.$



=== TEST 2: timer without explicit reset and sleep
--- config
    location /timer {
        echo "elapsed $echo_timer_elapsed sec.";
    }
--- request
    GET /timer
--- response_body_like
^elapsed 0\.00[0-4] sec\.$

