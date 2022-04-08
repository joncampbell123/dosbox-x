#!/usr/bin/env perl
use strict;
use warnings;

our @months = qw(jan feb mar apr may jun jul aug sep oct nov dec);

my $branch = `git branch | grep '^\*' | sed -e 's/^\* //'`;
chomp $branch;
die unless $branch;

open(my $S, '-|', 'git --no-pager log --max-count=1') or die "$!"; 

my $lcommit = "x";
my $lcdate = "unknown";


foreach my $line (<$S>) {
	chomp $line;

	if ($line =~ m/^commit +/) {
		$lcommit = lc($');
		$lcommit =~ s/ +$//;
	}
	else {
		my @nv    = split(/: +/, $line);
		my $value = $nv[1];
		my $name  = $nv[0] || '';

		if ($name =~ m/^Date$/i) {
			# Fri Apr 15 08:51:32 2011 -0700
			#  0   1   2  3        4     5
			my @b = split(/ +/,$value);

			my $mon  = lc($b[1]);
			my $day  = $b[2];
			my $time = $b[3];
			my $year = $b[4];

			for my $i (0 .. $#months) {
				if ($months[$i] eq $mon) {
					$mon = $i + 1;
					last;
				}
			}

			my $date = sprintf("%04u%02u%02u", $year+0, $mon, $day+0);
			$time    =~ s/://g;
			$lcdate  = "${date}-${time}";
		}
	}
}

close($S);

print "$lcdate-$lcommit-$branch\n";

