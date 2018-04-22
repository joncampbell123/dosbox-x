#!/usr/bin/perl
open(TMPL,"<index.html.template") || die;
open(INDEX,">index.html") || die;

my $line;

while ($line = <TMPL>) {
    chomp $line;

    if ($line eq "<LIST_PLACEHOLDER/>") {
    }
    else {
        print INDEX "$line\n";
    }
}

close(INDEX);
close(TMPL);

