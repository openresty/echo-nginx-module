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
^elapsed 0\.0(2[6-9]|3[0-5]) sec\.$



=== TEST 2: timer without explicit reset and sleep
--- config
    location /timer {
        echo "elapsed $echo_timer_elapsed sec.";
    }
--- request
    GET /timer
--- response_body_like
^elapsed 0\.00[0-5] sec\.$



=== TEST 3: timing accumulated sleeps
--- config
    location /timer {
        echo_sleep 0.03;
        echo_sleep 0.02;
        echo "elapsed $echo_timer_elapsed sec.";
    }
--- request
    GET /timer
--- response_body_like
^elapsed 0\.0(4[6-9]|5[0-5]) sec\.$



=== TEST 4: timer with explicit reset but without sleep
--- config
    location /timer {
        echo_reset_timer;
        echo "elapsed $echo_timer_elapsed sec.";
    }
--- request
    GET /timer
--- response_body_like
^elapsed 0\.00[0-5] sec\.$



=== TEST 5: reset timer between sleeps
--- config
    location /timer {
        echo_sleep 0.02;
        echo "elapsed $echo_timer_elapsed sec.";
        echo_reset_timer;
        echo_sleep 0.03;
        echo "elapsed $echo_timer_elapsed sec.";
    }
--- request
    GET /timer
--- response_body_like
^elapsed 0\.0(1[6-9]|2[0-5]) sec\.
elapsed 0\.0(2[6-9]|3[0-5]) sec\.$



=== TEST 6: reset timer between blocking sleeps
--- config
    location /timer {
        echo_blocking_sleep 0.02;
        echo "elapsed $echo_timer_elapsed sec.";
        echo_reset_timer;
        echo_blocking_sleep 0.03;
        echo "elapsed $echo_timer_elapsed sec.";
    }
--- request
    GET /timer
--- response_body_like
^elapsed 0\.0(1[6-9]|2[0-5]) sec\.
elapsed 0\.0(2[6-9]|3[0-6]) sec\.$

