#!/usr/bin/env perl

use strict;
use warnings;

my @nl_counts;
my $last_nl_count_level;

my @bl_counts;
my $last_bl_count_level;

sub fmt_pos ($) {
    (my $s = $_[0]) =~ s{\#(.*)}{/"$1"};
    $s;
}

print "=encoding utf-8\n\n";

while (<>) {
    s{\[(http[^ \]]+) ([^\]]*)\]}{$2 (L<$1>)}gi;
    s{ \[\[ ( [^\]\|]+ ) \| ([^\]]*) \]\] }{"L<$2|" . fmt_pos($1) . ">"}gixe;
    s{<code>(.*?)</code>}{C<$1>}gi;
    s{'''(.*?)'''}{B<$1>}g;
    s{''(.*?)''}{I<$1>}g;
    s{^\s*<[^>]+>\s*$}{};

    if (/^\s*$/) {
        print "\n";
        next;
    }

=begin cmt

    if ($. == 1) {
        warn $_;
        for my $i (0..length($_) - 1) {
            my $chr = substr($_, $i, 1);
            warn "chr ord($i): ".ord($chr)." \"$chr\"\n";
        }
    }

=end cmt
=cut

    if (/(=+) (.*) \1$/) {
        #warn "HERE! $_" if $. == 1;
        my ($level, $title) = (length $1, $2);
        collapse_lists();

        print "\n=head$level $title\n\n";
    } elsif (/^(\#+) (.*)/) {
        my ($level, $txt) = (length($1) - 1, $2);
        if (defined $last_nl_count_level && $level != $last_nl_count_level) {
            print "\n=back\n\n";
        }
        $last_nl_count_level = $level;
        $nl_counts[$level] ||= 0;
        if ($nl_counts[$level] == 0) {
            print "\n=over\n\n";
        }
        $nl_counts[$level]++;
        print "\n=item $nl_counts[$level].\n\n";
    } elsif (/^(\*+) (.*)/) {
        my ($level, $txt) = (length($1) - 1, $2);
        if (defined $last_bl_count_level && $level != $last_bl_count_level) {
            print "\n=back\n\n";
        }
        $last_bl_count_level = $level;
        $bl_counts[$level] ||= 0;
        if ($bl_counts[$level] == 0) {
            print "\n=over\n\n";
        }
        $bl_counts[$level]++;
        print "\n=item *\n\n";
        print "$txt\n";
    } else {
        collapse_lists();
        print;
    }
}

collapse_lists();

sub collapse_lists {
    while (defined $last_nl_count_level && $last_nl_count_level >= 0) {
        print "\n=back\n\n";
        $last_nl_count_level--;
    }
    undef $last_nl_count_level;
    undef @nl_counts;

    while (defined $last_bl_count_level && $last_bl_count_level >= 0) {
        print "\n=back\n\n";
        $last_bl_count_level--;
    }
    undef $last_bl_count_level;
    undef @bl_counts;
}

