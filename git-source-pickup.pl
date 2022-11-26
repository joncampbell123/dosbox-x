#!/usr/bin/env perl
use warnings;
use strict;

our @months = qw(jan feb mar apr may jun jul aug sep oct nov dec);
my $project = `git config --get remote.origin.url | sed -e 's/\\/\$//' | sed -e 's/^.*\\///'`;
chomp $project;
$project =~ s/\.git$//;
die if $project eq "";

my $branch = `git branch | grep '^\*' | sed -e 's/^\* //'`; chomp $branch;
my $branchfname = "-branch-$branch" if $branch ne "";
print "Current branch: $branch\n";

print "Ensuring the build tree is clean...\n";
my $x = system("./git-update-all-wo-push");
die unless $x == 0;

open(my $S, '-|', 'git --no-pager log --max-count=1') || die "$!";
my $lcommit = "x";
my $lcdate = "unknown";

foreach my $line (<$S>) {
	chomp $line;

	if ($line =~ m/^commit +/) {
		$lcommit = lc($');
		$lcommit =~ s/ +$//;
	}
	else {
		my @nv = split(/: +/,$line);
		my $value = $nv[1];
		my $name = $nv[0];

		if ($name =~ m/^Date$/i) {
			# Fri Apr 15 08:51:32 2011 -0700
			#  0   1   2  3        4     5
			my @b = split(/ +/,$value);

			my $mon = lc($b[1]);
			my $day = $b[2];
			my $time = $b[3];
			my $year = $b[4];

			for my $i (0 .. $#months) {
				if ($months[$i] eq $mon) {
					$mon = $i + 1;
					last;
				}
			}

			my $date = sprintf("%04u%02u%02u", $year+0, $mon, $day+0);
			$time =~ s/://g;
			$lcdate = $date."-".$time;
		}
	}
}

close($S);

#my $filename = $project."-rev-".sprintf("%08u",$lcrev)."-src.tar.bz2";
my $filename = "../${project}-${lcdate}-commit-${lcommit}-src${branchfname}.tar";
if (not -f "$filename.xz" ) {
	print "Packing source (all build files except LIB,OBJ,etc.)\n";
	print "  to: $filename\n";


	system({'tar'} 'tar', '-C', '..', '-cvf', $filename, $project) == 0
		or die "$!";
	system({'xz'} 'xz', '-6e', $filename) == 0
		or die "$!";
}
