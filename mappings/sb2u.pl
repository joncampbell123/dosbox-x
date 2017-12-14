#!/usr/bin/perl
#
# Read UNICODE.ORG mapping file and produce an array to convert
# from the code page TO unicode.
#
# Feed the mapping to STDIN to produce array to STDOUT
#
# Provide the name of the array in ARGV[1]

my $arrayname = shift @ARGV;
my $sz = 1;
my @map;
my $i;

while (@map < 256) {
    push(@map,0);
}

while (my $line = <STDIN>) {
    chomp $line;
    next if $line =~ m/^\#/;
    next if $line =~ m/^\x1A/; # CTRL+Z
    next if $line eq "";

    my $in=undef,$out=undef;

    if ($line =~ s/^(0x[0-9a-fA-F]+)[ \t]+//) {
        $in = hex($1);
    }
    if ($line =~ s/^(0x[0-9a-fA-F]+)[ \t]+//) {
        $out = hex($1);
    }

    if (defined($in) && defined($out)) {
        next if $in <= 0;
        next if $in > 255;
        $map[$in] = $out;

        $sz = 2 if ($sz < 2 && $out > 0xFF);
        $sz = 4 if ($sz < 4 && $out > 0xFFFF);
    }
}

$type = "uint8_t";
$type = "uint16_t" if $sz == 2;
$type = "uint32_t" if $sz == 4;

$prfmt = "0x%0".($sz*2)."x";

$colwidth = 8;

print "/* single-byte charset to unicode. 0x0000 means no mapping */\n";
print "const $type $arrayname"."[256] = {\n";
for ($y=0;$y < 256;$y += $colwidth) {
    print "\t";
    for ($x=0;$x < $colwidth;$x++) {
        $i = $y + $x;

        print sprintf($prfmt,$map[$i]);
        if ($i != 255) {
            print ",";
        }
        else {
            print " ";
        }
    }

    print " /* ".sprintf("0x%02X-0x%02X",$y,$y+$colwidth-1)." */";

    print "\n";
}
print "};\n";

