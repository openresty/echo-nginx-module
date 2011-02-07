# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: big client body buffered into temp files
--- config
    location /echo {
        client_body_buffer_size 1k;
        echo_read_request_body;
        echo_request_body;
    }
--- request eval
"POST /echo
" . 'a' x 4096 . 'end';
--- response_body eval
'a' x 4096 . 'end'

