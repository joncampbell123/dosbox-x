#!/usr/bin/env perl
use strict;
use warnings;

my @t = localtime();
my $sec =  $t[0];
my $min =  $t[1];
my $hour = $t[2];
my $mday = $t[3];
my $mon  = $t[4] + 1;
my $year = $t[5] + 1900;
my $yearn = $year + ($mon == 12 && $mday == 31 ? 1 : 0);

my @months = qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec );

my $tmp = sprintf('%s %u, %u %u:%02u:%02u%s', $months[$mon-1], $mday, $year, (($hour + 11) % 12) + 1, $min, $sec, $hour >= 12 ? "pm" : "am");

my $commit = `git rev-parse --short=7 HEAD`;
chomp($commit);

open(my $X, '>', 'include/build_timestamp.h') || die;
print $X qq{/*auto-generated*/\n};
print $X qq{#define UPDATED_STR "$tmp"\n};
print $X qq{#define GIT_COMMIT_HASH "$commit"\n};
print $X qq{#define COPYRIGHT_END_YEAR "$yearn"\n};
close($X);

my $file = 'contrib/linux/com.dosbox_x.DOSBox-X.metainfo.xml.in';
open(my $FILE, $file) or die "Can't read from $file!\n";

my @lines;
while (my $line = <$FILE>) {
	if ($line =~ /date=/) {
		push @lines, (q{          <release version="@PACKAGE_VERSION@" date="} . $year . '-' . $mon . '-' . $mday . qq{"/>\n});
	} elsif ($line =~ /<!-- Copyright/) {
		push @lines, (qq{<!-- Copyright 2011-$yearn Jonathan Campbell -->\n});
	} else {
		push @lines, $line;
	}
}
close($FILE);

open($FILE, '>', $file) or die "Can't write to $file!\n";
print $FILE @lines;
close($FILE);
