package Test::Nginx::Echo;

use lib 'lib';
use lib 'inc';

use Smart::Comments::JSON '##';
use LWP::UserAgent; # XXX should use a socket level lib here
use Test::Base -Base;
use Module::Install::Can;
use File::Spec ();
use Cwd qw( cwd );

our $UserAgent = LWP::UserAgent->new;

our $Workers                = 1;
our $WorkerConnections      = 3;
our $LogLevel               = 'debug';
#our $MasterProcessEnabled   = 'on';
#our $DaemonEnabled          = 'on';
our $ListenPort             = 1984;

our ($ServerRoot, $PrevRequest, $PrevConfig);

our $ServRoot   = File::Spec->catfile(cwd(), 't/servroot');
our $LogDir     = File::Spec->catfile($ServRoot, 'logs');
our $ErrLogFile = File::Spec->catfile($LogDir, 'error.log');
our $AccLogFile = File::Spec->catfile($LogDir, 'access.log');
our $HtmlDir    = File::Spec->catfile($ServRoot, 'html');
our $ConfDir    = File::Spec->catfile($ServRoot, 'conf');
our $ConfFile   = File::Spec->catfile($ConfDir, 'nginx.conf');
our $PidFile    = File::Spec->catfile($LogDir, 'nginx.pid');

our @EXPORT = qw( run_tests run_test );

sub run_tests () {
    for my $block (blocks()) {
        run_test($block);
    }
}

sub setup_server_root () {
    if (-d 't/servroot') {
        system("rm -rf t/servroot") == 0 or
            die "Can't remove t/servroot";
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
master_process off;
error_log $ErrLogFile $LogLevel;
pid       $PidFile;

http {
    access_log $AccLogFile;

    default_type text/plain;
    keepalive_timeout  65;
    server {
        listen          $ListenPort;
        server_name     localhost;

        client_max_body_size 30M;
        client_body_buffer_size 4k;

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
    }
    $first =~ s/^\s+|\s+$//g;
    my ($meth, $rel_url) = split /\s+/, $first, 2;
    my $url = "http://localhost:$ListenPort" . $rel_url;
    close $in;
    return {
        method  => $meth,
        url     => $url,
    };
}

sub run_test ($) {
    my $block = shift;
    my $name = $block->name;
    my $request = $block->request;
    if (!defined $request) {
        $request = $PrevRequest;
        $PrevRequest = $request;
    }
    my $config = $block->config;
    if (!defined $config) {
        $config = $PrevConfig;
    }
    if (-f $PidFile) {
        open my $in, $PidFile or
            Test::More::BAIL_OUT("$name - Failed to open the pid file $PidFile for reading: $!");
        my $pid = do { local $/; <$in> };
        #warn "Pid: $pid\n";
        close $in;

        if (system("ps $pid") == 0) {
            if (kill(15, $pid) == 0) {
                Test::More::note("$name - Failed to kill the existing nginx process with PID $pid using signal TERM, trying KILL instead...");
                if (kill(9, $pid) == 0) {
                    Test::More::BAIL_OUT("$name - Failed to kill the existing nginx process with PID $pid using signal KILL, giving up...");
                }
            }
        }
    }
    setup_server_root();
    write_config_file(\$config);
    if ( ! Module::Install::Can->can_run('nginx') ) {
        Test::More::BAIL_OUT("$name - Cannot find the nginx executable in the PATH environment");
    }
    #if (system("nginx -p $ServRoot -c $ConfFile -t") != 0) {
    #Test::More::BAIL_OUT("$name - Invalid config file");
    #}
    my $cmd = "nginx -p $ServRoot -c $ConfFile";
    if (system($cmd) != 0 || !-f $PidFile) {
        Test::More::BAIL_OUT("$name - Cannot start nginx using command \"$cmd\".");
    }
    my $req_spec = parse_request($name, \$request);
    ## $req_spec
    my $method = $req_spec->{method};
    my $req = HTTP::Request->new($method);
    my $content = $req_spec->{content};
    #$req->header('Content-Type' => $type);
    $req->header('Accept', '*/*');
    $req->url($req_spec->{url});
    if ($content) {
        if ($method eq 'GET' or $method eq 'HEAD') {
            croak "HTTP 1.0/1.1 $method request should not have content: $content";
        }
        $req->content($content);
    } elsif ($method eq 'POST' or $method eq 'PUT') {
        $req->header('Content-Length' => 0);
    }
    my $res = $UserAgent->request($req);
    if (defined $block->response_body) {
        if (!$res->is_success) {
            fail("$name - response_body - response indicates failure: " . $res->status_line);
        } else {
            is($res->content, $block->response_body, "$name - response_body - response is expected");
        }
    }
}

1;
__END__

=head1 NAME

Test::Nginx::Echo - Test scaffold for the echo Nginx module

=head1 AUTHOR

agentzh C<< <agentzh@gmail.com> >>

=head1 COPYRIGHT & LICENSE

Copyright (C) 2009 by agentzh.
Copyright (C) 2009 by Taobao Inc. ( http://www.taobao.com )

This software is licensed under the terms of the BSD License.

