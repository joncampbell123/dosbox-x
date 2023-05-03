#!/usr/bin/perl
use integer;
open (THEFILE,'>','../src/hardware/font-switch.h')
	or die "Can't open my file $!";

print THEFILE "switch (bit_mask) {\n";
for ($i = 0; $i < 256; $i++) {
	print THEFILE "\tcase $i:\n";
	$b=128;
	$add=0;
	do  {
		if ($i & $b) {
			print THEFILE "\t\t*(draw+$add)=fg;\n";
		} else {
			print THEFILE "\t\t*(draw+$add)=bg;\n";
		}
		$b=$b >> 1;
		$add=$add+1;
	} until ($b == 0);
	print THEFILE "\tbreak;\n";
} 
print THEFILE "}\n";


close (THEFILE);
