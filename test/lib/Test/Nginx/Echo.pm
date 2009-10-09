package Test::Nginx::Echo;

use lib 'lib';
use lib 'inc';

use LWP::UserAgent; # XXX should use a socket level lib here
use Test::Base -Base;

sub run_tests () {
    for my $block (blocks()) {
        run_test($block);
    }
}

sub run_test ($) {
    my $block = shift;
    my $name = $block->name;
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

