# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;
use Config;

our $MaxSize = $Config{sizesize} == 8 ? '9223372036854775807' : '2147483647';
our $OverflowSize = $Config{sizesize} == 8 ? '9223372036854775808' : '2147483648';

plan tests => 2 * blocks() + 8;

#$Test::Nginx::LWP::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: sanity
--- config
    location /dup {
        echo_duplicate 3 a;
    }
--- request
    GET /dup
--- response_body: aaa



=== TEST 2: abc abc
--- config
    location /dup {
        echo_duplicate 2 abc;
    }
--- request
    GET /dup
--- response_body: abcabc



=== TEST 3: big size with underscores
--- config
    location /dup {
        echo_duplicate 10_000 A;
    }
--- request
    GET /dup
--- response_body eval
'A' x 10_000



=== TEST 4: 0 duplicate 0 empty strings
--- config
    location /dup {
        echo_duplicate 0 "";
    }
--- request
    GET /dup
--- response_body



=== TEST 5: 0 duplicate non-empty strings
--- config
    location /dup {
        echo_duplicate 0 "abc";
    }
--- request
    GET /dup
--- response_body



=== TEST 6: duplication of empty strings
--- config
    location /dup {
        echo_duplicate 2 "";
    }
--- request
    GET /dup
--- response_body



=== TEST 7: sanity (HEAD)
--- config
    location /dup {
        echo_duplicate 3 a;
    }
--- request
    HEAD /dup
--- response_body



=== TEST 8: reject output size overflow
--- config
    location /dup {
        echo_duplicate $arg_n ABCD;
    }
--- request
    GET /dup?n=4611686018427387905
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log eval
$Config::Config{sizesize} == 8 ? 'echo_duplicate output size overflows'
                               : 'invalid size specified'



=== TEST 9: maximum count with an empty string
--- config
    location /dup {
        echo_duplicate $arg_n "";
    }
--- request eval
"GET /dup?n=$::MaxSize"
--- response_body



=== TEST 10: reject count just above the maximum even with an empty string
--- config
    location /dup {
        echo_duplicate $arg_n "";
    }
--- request eval
"GET /dup?n=$::OverflowSize"
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
invalid size specified



=== TEST 11: reject a count that would wrap to a small positive number
--- config
    location /dup {
        echo_duplicate $arg_n "";
    }
--- request
    GET /dup?n=18446744073709551617
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
invalid size specified



=== TEST 12: reject count overflow with underscores
--- config
    location /dup {
        echo_duplicate $arg_n "";
    }
--- request
    GET /dup?n=18_446_744_073_709_551_617
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
invalid size specified



=== TEST 13: reject output exceeding the supported signed size
--- config
    location /dup {
        echo_duplicate $arg_n ab;
    }
--- request eval
"GET /dup?n=$::MaxSize"
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
echo_duplicate output size overflows



=== TEST 14: request-derived count with underscores
--- config
    location /dup {
        echo_duplicate $arg_n ab;
    }
--- request
    GET /dup?n=0_0_3
--- response_body: ababab



=== TEST 15: reject a negative count
--- config
    location /dup {
        echo_duplicate $arg_n ab;
    }
--- request
    GET /dup?n=-1
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
invalid size specified



=== TEST 16: reject non-decimal count
--- config
    location /dup {
        echo_duplicate $arg_n ab;
    }
--- request
    GET /dup?n=3x
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
invalid size specified



=== TEST 17: reject an empty count
--- config
    location /dup {
        echo_duplicate $arg_n ab;
    }
--- request
    GET /dup?n=
--- error_code: 500
--- response_body_like: 500 Internal Server Error
--- error_log
invalid size specified
