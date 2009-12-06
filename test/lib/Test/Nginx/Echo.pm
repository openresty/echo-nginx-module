package Test::Nginx::Echo;

use lib 'lib';
use lib 'inc';
use Test::Base -Base;

our $NoNginxManager = 0;
our $RepeatEach = 1;

use Time::HiRes qw(sleep);
use Test::LongString;

#use Smart::Comments::JSON '##';
use POSIX qw( SIGQUIT SIGKILL SIGTERM );
use LWP::UserAgent; # XXX should use a socket level lib here
use Module::Install::Can;
use List::Util qw( shuffle );
use File::Spec ();
use Cwd qw( cwd );

our $UserAgent = LWP::UserAgent->new;
$UserAgent->agent(__PACKAGE__);
#$UserAgent->default_headers(HTTP::Headers->new);

our $Workers                = 1;
our $WorkerConnections      = 1024;
our $LogLevel               = 'debug';
#our $MasterProcessEnabled   = 'on';
#our $DaemonEnabled          = 'on';
our $ServerPort             = 1984;
our $ServerPortForClient    = 1984;
#our $ServerPortForClient    = 1984;

our $NginxVersion;
our $NginxRawVersion;

#our ($PrevRequest, $PrevConfig);

our $ServRoot   = File::Spec->catfile(cwd(), 't/servroot');
our $LogDir     = File::Spec->catfile($ServRoot, 'logs');
our $ErrLogFile = File::Spec->catfile($LogDir, 'error.log');
our $AccLogFile = File::Spec->catfile($LogDir, 'access.log');
our $HtmlDir    = File::Spec->catfile($ServRoot, 'html');
our $ConfDir    = File::Spec->catfile($ServRoot, 'conf');
our $ConfFile   = File::Spec->catfile($ConfDir, 'nginx.conf');
our $PidFile    = File::Spec->catfile($LogDir, 'nginx.pid');

our @EXPORT = qw( plan run_tests run_test );

=begin cmt

sub plan (@) {
    if (@_ == 2 && $_[0] eq 'tests' && defined $RepeatEach) {
        #$_[1] *= $RepeatEach;
    }
    super;
}

=end cmt

=cut

sub trim ($);

sub show_all_chars ($);

sub parse_headers ($);

sub run_test_helper ($$);

sub get_canon_version (@) {
    sprintf "%d.%03d%03d", $_[0], $_[1], $_[2];
}

sub get_nginx_version () {
    my $out = `nginx -V 2>&1`;
    if (!defined $out || $? != 0) {
        warn "Failed to get the version of the Nginx in PATH.\n";
    }
    if ($out =~ m{nginx/(\d+)\.(\d+)\.(\d+)}s) {
        $NginxRawVersion = "$1.$2.$3";
        return get_canon_version($1, $2, $3);
    }
    warn "Failed to parse the output of \"nginx -V\": $out\n";
    return undef;
}

sub run_tests () {
    $NginxVersion = get_nginx_version();

    if (defined $NginxVersion) {
        #warn "[INFO] Using nginx version $NginxVersion ($NginxRawVersion)\n";
    }

    for my $block (shuffle blocks()) {
        #for (1..3) {
            run_test($block);
        #}
    }
}

sub setup_server_root () {
    if (-d $ServRoot) {
        #sleep 0.5;
        #die ".pid file $PidFile exists.\n";
        system("rm -rf t/servroot > /dev/null") == 0 or
            die "Can't remove t/servroot";
        #sleep 0.5;
    }
    mkdir $ServRoot or
        die "Failed to do mkdir $ServRoot\n";
    mkdir $LogDir or
        die "Failed to do mkdir $LogDir\n";
    mkdir $HtmlDir or
        die "Failed to do mkdir $HtmlDir\n";
    mkdir $ConfDir or
        die "Failed to do mkdir $ConfDir\n";
}

sub write_config_file ($) {
    my $rconfig = shift;
    open my $out, ">$ConfFile" or
        die "Can't open $ConfFile for writing: $!\n";
    print $out <<_EOC_;
worker_processes  $Workers;
daemon on;
master_process on;
error_log $ErrLogFile $LogLevel;
pid       $PidFile;

http {
    access_log $AccLogFile;

    default_type text/plain;
    keepalive_timeout  65;
    server {
        listen          $ServerPort;
        server_name     localhost;

        client_max_body_size 30M;
        #client_body_buffer_size 4k;

        # Begin test case config...
$$rconfig
        # End test case config.

        location / {
            root $HtmlDir;
            index index.html index.htm;
        }
    }
}

events {
    worker_connections  $WorkerConnections;
}

_EOC_
    close $out;
}

sub parse_request ($$) {
    my ($name, $rrequest) = @_;
    open my $in, '<', $rrequest;
    my $first = <$in>;
    if (!$first) {
        Test::More::BAIL_OUT("$name - Request line should be non-empty");
        die;
    }
    $first =~ s/^\s+|\s+$//g;
    my ($meth, $rel_url) = split /\s+/, $first, 2;
    my $url = "http://localhost:$ServerPortForClient" . $rel_url;

    my $content = do { local $/; <$in> };
    if ($content) {
        $content =~ s/^\s+|\s+$//s;
    }

    close $in;

    return {
        method  => $meth,
        url     => $url,
        content => $content,
    };
}

sub get_pid_from_pidfile ($) {
    my ($name) = @_;
    open my $in, $PidFile or
        Test::More::BAIL_OUT("$name - Failed to open the pid file $PidFile for reading: $!");
    my $pid = do { local $/; <$in> };
    #warn "Pid: $pid\n";
    close $in;
    $pid;
}

sub chunk_it ($$$) {
    my ($chunks, $start_delay, $middle_delay) = @_;
    my $i = 0;
    return sub {
        if ($i == 0) {
            if ($start_delay) {
                sleep($start_delay);
            }
        } elsif ($middle_delay) {
            sleep($middle_delay);
        }
        return $chunks->[$i++];
    }
}

sub run_test ($) {
    my $block = shift;
    my $name = $block->name;
    my $request = $block->request;
    if (!defined $request) {
        #$request = $PrevRequest;
        #$PrevRequest = $request;
        Test::More::BAIL_OUT("$name - No '--- request' section specified");
        die;
    }

    my $config = $block->config;
    if (!defined $config) {
        Test::More::BAIL_OUT("$name - No '--- config' section specified");
        #$config = $PrevConfig;
        die;
    }

    my $skip_nginx = $block->skip_nginx;
    my ($tests_to_skip, $should_skip, $skip_reason);
    if (defined $skip_nginx) {
        if ($skip_nginx =~ m{
                ^ \s* (\d+) \s* : \s*
                    ([<>]=?) \s* (\d+)\.(\d+)\.(\d+)
                    (?: \s* : \s* (.*) )?
                \s*$}x) {
            $tests_to_skip = $1;
            my ($op, $ver1, $ver2, $ver3) = ($2, $3, $4, $5);
            $skip_reason = $6;
            #warn "$ver1 $ver2 $ver3";
            my $ver = get_canon_version($ver1, $ver2, $ver3);
            if ((!defined $NginxVersion and $op =~ /^</)
                    or eval "$NginxVersion $op $ver")
            {
                $should_skip = 1;
            }
        } else {
            Test::More::BAIL_OUT("$name - Invalid --- skip_nginx spec: " .
                $skip_nginx);
            die;
        }
    }
    if (!defined $skip_reason) {
        $skip_reason = "various reasons";
    }

    my $todo_nginx = $block->todo_nginx;
    my ($should_todo, $todo_reason);
    if (defined $todo_nginx) {
        if ($todo_nginx =~ m{
                ^ \s*
                    ([<>]=?) \s* (\d+)\.(\d+)\.(\d+)
                    (?: \s* : \s* (.*) )?
                \s*$}x) {
            my ($op, $ver1, $ver2, $ver3) = ($1, $2, $3, $4);
            $todo_reason = $5;
            my $ver = get_canon_version($ver1, $ver2, $ver3);
            if ((!defined $NginxVersion and $op =~ /^</)
                    or eval "$NginxVersion $op $ver")
            {
                $should_todo = 1;
            }
        } else {
            Test::More::BAIL_OUT("$name - Invalid --- todo_nginx spec: " .
                $todo_nginx);
            die;
        }
    }

    if (!defined $todo_reason) {
        $todo_reason = "various reasons";
    }

    if (!$NoNginxManager && !$should_skip) {
        my $nginx_is_running = 1;
        if (-f $PidFile) {
            my $pid = get_pid_from_pidfile($name);
            if (system("ps $pid > /dev/null") == 0) {
                write_config_file(\$config);
                if (kill(SIGQUIT, $pid) == 0) { # send quit signal
                    #warn("$name - Failed to send quit signal to the nginx process with PID $pid");
                }
                sleep 0.02;
                if (system("ps $pid > /dev/null") == 0) {
                    #warn "killing with force...\n";
                    kill(SIGKILL, $pid);
                    sleep 0.03;
                }
                undef $nginx_is_running;
            } else {
                unlink $PidFile or
                    die "Failed to remove pid file $PidFile\n";
                undef $nginx_is_running;
            }
        } else {
            undef $nginx_is_running;
        }

        unless ($nginx_is_running) {
            #warn "*** Restarting the nginx server...\n";
            setup_server_root();
            write_config_file(\$config);
            if ( ! Module::Install::Can->can_run('nginx') ) {
                Test::More::BAIL_OUT("$name - Cannot find the nginx executable in the PATH environment");
                die;
            }
        #if (system("nginx -p $ServRoot -c $ConfFile -t") != 0) {
        #Test::More::BAIL_OUT("$name - Invalid config file");
        #}
        #my $cmd = "nginx -p $ServRoot -c $ConfFile > /dev/null";
            my $cmd = "nginx -c $ConfFile > /dev/null";
            if (system($cmd) != 0) {
                Test::More::BAIL_OUT("$name - Cannot start nginx using command \"$cmd\".");
                die;
            }
            sleep 0.1;
        }
    }

    my $i = 0;
    while ($i++ < $RepeatEach) {
        if ($should_skip) {
            SKIP: {
                skip "$name - $skip_reason", $tests_to_skip;

                run_test_helper($block, $request);
            }
        } elsif ($should_todo) {
            TODO: {
                local $TODO = "$name - $todo_reason";

                run_test_helper($block, $request);
            }
        } else {
            run_test_helper($block, $request);
        }
    }
}

sub trim ($) {
    (my $s = shift) =~ s/^\s+|\s+$//g;
    $s =~ s/\n/ /gs;
    $s =~ s/\s{2,}/ /gs;
    $s;
}

sub show_all_chars ($) {
    my $s = shift;
    $s =~ s/\n/\\n/gs;
    $s =~ s/\r/\\r/gs;
    $s =~ s/\t/\\t/gs;
    $s;
}

sub parse_headers ($) {
    my $s = shift;
    my %headers;
    open my $in, '<', \$s;
    while (<$in>) {
        s/^\s+|\s+$//g;
        my ($key, $val) = split /\s*:\s*/, $_, 2;
        $headers{$key} = $val;
    }
    close $in;
    return \%headers;
}

sub run_test_helper ($$) {
    my ($block, $request) = @_;

    my $name = $block->name;
    #if (defined $TODO) {
    #$name .= "# $TODO";
    #}

    my $req_spec = parse_request($name, \$request);
    ## $req_spec
    my $method = $req_spec->{method};
    my $req = HTTP::Request->new($method);
    my $content = $req_spec->{content};

    if (defined ($block->request_headers)) {
        my $headers = parse_headers($block->request_headers);
        while (my ($key, $val) = each %$headers) {
            $req->header($key => $val);
        }
    }

    #$req->header('Accept', '*/*');
    $req->url($req_spec->{url});
    if ($content) {
        if ($method eq 'GET' or $method eq 'HEAD') {
            croak "HTTP 1.0/1.1 $method request should not have content: $content";
        }
        $req->content($content);
    } elsif ($method eq 'POST' or $method eq 'PUT') {
        my $chunks = $block->chunked_body;
        if (defined $chunks) {
            if (!ref $chunks or ref $chunks ne 'ARRAY') {

                Test::More::BAIL_OUT("$name - --- chunked_body should takes a Perl array ref as its value");
            }

            my $start_delay = $block->start_chunk_delay || 0;
            my $middle_delay = $block->middle_chunk_delay || 0;
            $req->content(chunk_it($chunks, $start_delay, $middle_delay));
            if (!defined $req->header('Content-Type')) {
                $req->header('Content-Type' => 'text/plain');
            }
        } else {
            if (!defined $req->header('Content-Type')) {
                $req->header('Content-Type' => 'text/plain');
            }

            $req->header('Content-Length' => 0);
        }
    }

    if ($block->more_headers) {
        my @headers = split /\n+/, $block->more_headers;
        for my $header (@headers) {
            next if $header =~ /^\s*\#/;
            my ($key, $val) = split /:\s*/, $header, 2;
            #warn "[$key, $val]\n";
            $req->header($key => $val);
        }
    }
    #warn "DONE!!!!!!!!!!!!!!!!!!!!";

    my $res = $UserAgent->request($req);

    #warn "res returned!!!";

    if (defined $block->error_code) {
        is($res->code, $block->error_code, "$name - status code ok");
    } else {
        is($res->code, 200, "$name - status code ok");
    }

    if (defined $block->response_headers) {
        my $headers = parse_headers($block->response_headers);
        while (my ($key, $val) = each %$headers) {
            my $expected_val = $res->header($key);
            if (!defined $expected_val) {
                $expected_val = '';
            }
            is $expected_val, $val,
                "$name - header $key ok";
        }
    } elsif (defined $block->response_headers_like) {
        my $headers = parse_headers($block->response_headers_like);
        while (my ($key, $val) = each %$headers) {
            my $expected_val = $res->header($key);
            if (!defined $expected_val) {
                $expected_val = '';
            }
            like $expected_val, qr/^$val$/,
                "$name - header $key like ok";
        }
    }

    if (defined $block->response_body) {
        my $content = $res->content;
        if (defined $content) {
            $content =~ s/^TE: deflate,gzip;q=0\.3\r\n//gms;
        }

        $content =~ s/^Connection: TE, close\r\n//gms;
        my $expected = $block->response_body;
        $expected =~ s/\$ServerPort\b/$ServerPort/g;
        $expected =~ s/\$ServerPortForClient\b/$ServerPortForClient/g;
        #warn show_all_chars($content);

        is_string($content, $expected, "$name - response_body - response is expected");
        #is($content, $expected, "$name - response_body - response is expected");

    } elsif (defined $block->response_body_like) {
        my $content = $res->content;
        if (defined $content) {
            $content =~ s/^TE: deflate,gzip;q=0\.3\r\n//gms;
        }
        $content =~ s/^Connection: TE, close\r\n//gms;
        my $expected_pat = $block->response_body_like;
        $expected_pat =~ s/\$ServerPort\b/$ServerPort/g;
        $expected_pat =~ s/\$ServerPortForClient\b/$ServerPortForClient/g;
        my $summary = trim($content);
        like($content, qr/$expected_pat/s, "$name - response_body_like - response is expected ($summary)");
    }
}

1;
__END__

=head1 NAME

Test::Nginx::Echo - Test scaffold for the ngx_echo module

=head1 AUTHOR

agentzh C<< <agentzh@gmail.com> >>

=head1 COPYRIGHT & LICENSE

Copyright (C) 2009 by agentzh.
Copyright (C) 2009 by Taobao Inc. ( http://www.taobao.com )

This software is licensed under the terms of the BSD License.

