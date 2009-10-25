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
        echo_foreach '&' $query_string;
            echo_location_async $echo_it;
            echo '/* end */';
        echo_end;
    }
    location /sub/1.css {
        echo "body { font-size: 12pt; }";
    }
    location /sub/2.css {
        echo "table { color: 'red'; }";
    }
--- request
    GET /main?/sub/1.css&/sub/2.css
--- response_body
body { font-size: 12pt; }
/* end */
table { color: 'red'; }
/* end */

