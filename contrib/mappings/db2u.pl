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

while (@map < 65536) {
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
        next if $in > 65535;
        $map[$in] = $out;

        $sz = 2 if ($sz < 2 && $out > 0xFF);
        $sz = 4 if ($sz < 4 && $out > 0xFFFF);
    }
}

$type = "uint8_t";
$type = "uint16_t" if $sz == 2;
$type = "uint32_t" if $sz == 4;

$prfmt = "0x%0".($sz*2)."x";

$pagesize = 0x40;
$colwidth = 8;

my @tbl;
my @rbl;
my $rawsz = 0;

for ($y=0;$y < 65536;$y += $pagesize) {
    $c = 0;
    for ($x=0;$x < $pagesize;$x++) {
        if ($map[$y+$x] != 0) {
            $c++;
            last;
        }
    }

    if ($c != 0) {
        push(@tbl,$y);
        push(@rbl,$rawsz);
        $rawsz += $pagesize;
    }
}

print "/* DBCS (double-byte) charset to unicode. 0x0000 means no mapping */\n";
print "/* hi = (code >> 6) */\n";
print "/* lo = code & 0x3F */\n";
print "/* rawoff = $arrayname"."_hitbl[hi] */\n";
print "/* if (rawoff != 0xFFFF) ucode = _raw[rawoff+lo] */\n";
print "const $type $arrayname"."_raw[$rawsz] = {\n";
for ($t=0;$t < @rbl;$t++) {
    $codebase = $tbl[$t];
    $rawbase = $rbl[$t];

    print "\t/* codebase=".sprintf("0x%04x",$codebase)." rawbase=".sprintf("0x%04x",$rawbase)." */\n";

    for ($y=0;$y < $pagesize;$y += $colwidth) {
        print "\t";
        for ($x=0;$x < $colwidth;$x++) {
            $i = $codebase + $y + $x;

            print sprintf($prfmt,$map[$i]);
            if ($i != ($pagesize-1) || ($t+1) != @rbl) {
                print ",";
            }
            else {
                print " ";
            }
        }

        print " /* ".sprintf("0x%04X-0x%04X",$codebase+$y,$codebase+$y+$colwidth-1)." */";

        print "\n";
    }
}
print "};\n";

print "const uint16_t $arrayname"."_hitbl[1024] = {\n";
$t = 0;
for ($y=0;$y < 1024;$y++) {
    if ($t < @tbl) {
        $codebase = $tbl[$t];
        $rawbase = $rbl[$t];
    }
    else {
        $codebase = 0xFFFFFF;
        $rawbase =  0xFFFFFF;
    }

    $comma = ',';
    $comma = ' ' if $y == 1023;

    if (($y*$pagesize) >= $codebase) {
        print "\t".sprintf("0x%04x",$rawbase)."$comma /* ".sprintf("0x%04x-0x%04X",$codebase,$codebase+$pagesize-1)." */\n";
        $t++;
    }
    else {
        print "\t".sprintf("0x%04x",0xFFFF)."$comma /* ".sprintf("0x%04x-0x%04X",$y*$pagesize,$y*$pagesize+$pagesize-1)." NOT PRESENT */\n";
    }
}
print "};\n";

