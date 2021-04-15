#!/usr/bin/perl
my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
my $datestr = sprintf("%04u%02u%02u%02u%02u%02u",$year+1900,$mon+1,$mday,$hour,$min,$sec);

my $ziptool = "vs2015/tool/zip.exe";

my $subdir="release/windows";

my $brancha=`git branch`;
my $branch;

my @a = split(/\n/,$brancha);
for ($i=0;$i < @a;$i++) {
	my $line = $a[$i];
	chomp $line;

	if ($line =~ s/^\* +//) {
		$branch = $line;
	}
}

if ( "$branch" eq "develop-win-sdl1-async-hack-201802" ) {
    $subdir="release/windows-async";
}

$suffix = $subdir;
$suffix =~ s/^.*\/windows/vsbuild/g;

mkdir "release" unless -d "release";
mkdir "$subdir" unless -d "$subdir";

die "bin directory not exist" unless -d "bin";

die unless -f $ziptool;

print "$zipname\n";

my @platforms = ('ARM', 'ARM64', 'Win32', 'x64');
my @builds = ('Release', 'Release SDL2');
my @files = ('dosbox-x.reference.conf', 'dosbox-x.reference.full.conf', 'dosbox-x.exe', 'FREECG98.bmp', 'changelog.txt', 'shaders');

foreach $platform (@platforms) {
	$plat = $platform;
	$plat = 'win32' if $plat eq 'Win32';
	$plat = 'win64' if $plat eq 'x64';
	$plat = 'arm32' if $plat eq 'ARM';
	$plat = 'arm64' if $plat eq 'ARM64';
	$zipname = "dosbox-x-$suffix-$plat-$datestr.zip";
	foreach $build (@builds) {
		my @filelist = ();

		# no spaces in filenames
		my $p_build = $build;
		$p_build =~ s/ /-/g;

		next if -f $zipname;

		foreach $file (@files) {
			$addfile = "bin/$platform/$build/$file";
			die "Missing file $addfile" unless -e $addfile;

			push(@filelist, $addfile);
		}

		# do it
		$r = system($ziptool, '-9', '-r', "$subdir/$zipname", @filelist);
		exit 1 unless $r == 0;
	}
}
