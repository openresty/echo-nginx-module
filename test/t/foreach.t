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



=== TEST 2: split in a url argument
--- config
    location /main {
        echo_foreach ',' $arg_cssfiles;
            echo_location_async $echo_it;
        echo_end;
    }
    location /foo.css {
        echo foo;
    }
    location /bar.css {
        echo bar;
    }
    location /baz.css {
        echo baz;
    }
--- request
    GET /main?cssfiles=/foo.css,/bar.css,/baz.css
--- response_body
foo
bar
baz



=== TEST 3: empty loop
--- config
    location /main {
        echo "start";
        echo_foreach ',' $arg_cssfiles;
        echo_end;
        echo "end";
    }
--- request
    GET /main?cssfiles=/foo.css,/bar.css,/baz.css
--- response_body
start
end



=== TEST 4: trailing delimiter
--- config
    location /main {
        echo_foreach ',' $arg_cssfiles;
            echo_location_async $echo_it;
        echo_end;
    }
    location /foo.css {
        echo foo;
    }
--- request
    GET /main?cssfiles=/foo.css,
--- response_body
foo

