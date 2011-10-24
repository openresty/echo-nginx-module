# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

plan tests => 2 * blocks();

run_tests();

__DATA__

=== TEST 1: standalone directive
--- config
    location /echo {
        echo $echo_client_request_headers;
    }
--- request
    GET /echo
--- response_body eval
"GET /echo HTTP/1.1\r
Host: localhost\r
Connection: Close\r

"



=== TEST 2: multiple instances
--- config
    location /echo {
        echo $echo_client_request_headers;
        echo $echo_client_request_headers;
    }
--- request
    GET /echo
--- response_body eval
"GET /echo HTTP/1.1\r
Host: localhost\r
Connection: Close\r

GET /echo HTTP/1.1\r
Host: localhost\r
Connection: Close\r

"



=== TEST 3: does not explicitly request_body
--- config
    location /echo {
        echo [$echo_request_body];
    }
--- request
POST /echo
body here
heh
--- response_body
[]



=== TEST 4: let proxy read request_body
--- config
    location /echo {
        echo_before_body [$echo_request_body];
        proxy_pass $scheme://127.0.0.1:$server_port/blah;
    }
    location /blah { echo_duplicate 0 ''; }
--- request
POST /echo
body here
heh
--- response_body
[body here
heh]



=== TEST 5: use echo_read_request_body to read it!
--- config
    location /echo {
        echo_read_request_body;
        echo [$echo_request_body];
    }
--- request
POST /echo
body here
heh
--- response_body
[body here
heh]



=== TEST 6: how about sleep after that?
--- config
    location /echo {
        echo_read_request_body;
        echo_sleep 0.002;
        echo [$echo_request_body];
    }
--- request
POST /echo
body here
heh
--- response_body
[body here
heh]



=== TEST 7: echo back the whole client request
--- config
  # echo back the client request
  location /echoback {
    echo $echo_client_request_headers;
    echo_read_request_body;
    echo $echo_request_body;
  }
--- request
POST /echoback
body here
haha
--- response_body eval
"POST /echoback HTTP/1.1\r
Host: localhost\r
Connection: Close\r
Content-Length: 14\r

body here
haha
"



=== TEST 8: echo_request_body
--- config
    location /body {
      client_body_buffer_size    5;
      echo_read_request_body;
      echo "[$echo_request_body]";
      echo_request_body;
    }
--- request eval
"POST /body
" . ('a' x 2048) . "b"
--- response_body eval
"[]\n" .
('a' x 2048) . "b"



=== TEST 9: $echo_response_status in content handler
--- config
    location /status {
        echo "status: $echo_response_status";
    }
--- request
    GET /status
--- response_body
status: 



=== TEST 10: echo_request_body (empty body)
--- config
    location /body {
      echo_read_request_body;
      echo_request_body;
    }
    location /main {
        proxy_pass http://127.0.0.1:$server_port/body;
    }
--- request eval
"POST /main"
--- response_body eval
""

