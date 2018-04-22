#!/usr/bin/perl
open(TMPL,"<index.html.template") || die;
open(INDEX,">index.html") || die;

my $line;

while ($line = <TMPL>) {
    chomp $line;

    if ($line =~ m/^%.*%$/) {
        $line =~ s/^%//;
        $line =~ s/%$//;

        if ($line eq "LIST_PLACEHOLDER") {
        }
        else {
            die "Unknown placeholder or directive '$line'";
        }
    }
    else {
        print INDEX "$line\n";
    }
}

close(INDEX);
close(TMPL);

