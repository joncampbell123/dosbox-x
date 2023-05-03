#!/usr/bin/perl
use integer;
open (THEFILE,'>','../src/hardware/ega-switch.h')
	or die "Can't open my file $!";

print THEFILE "switch (bit_mask) {\n";
for ($i = 0; $i < 256; $i++) {
	print THEFILE "\tcase $i:\n";
	$b=128;
	$add=0;
	do  {
		if ($i & $b) {
			print THEFILE "\t{\n";
			print THEFILE "\t\tBit8u color=0;\n";
			print THEFILE "\t\tif (pixels.b[0] & $b) color|=1;\n";
			print THEFILE "\t\tif (pixels.b[1] & $b) color|=2;\n";
			print THEFILE "\t\tif (pixels.b[2] & $b) color|=4;\n";
			print THEFILE "\t\tif (pixels.b[3] & $b) color|=8;\n";
			print THEFILE "\t\t*(write_pixels+$add)=color;\n";
			print THEFILE "\t\t*(write_pixels+$add+512*1024)=color;\n";
			print THEFILE "\t}\n";
		}

		$b=$b >> 1;
		$add=$add+1;
	} until ($b == 0);
	print THEFILE "\tbreak;\n";
} 
print THEFILE "}\n";


close (THEFILE);
