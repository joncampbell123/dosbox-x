This is DOSBox-X's internal virtual Drive Z:. It contains various utilities
that make a reasonable approximation of a fully setup DOS-like environment.
The Z drive is automatically inserted to the front of the PATH.

You can also add your own programs or files to the Z drive by putting them in
the "drivez" subdirectory of the DOSBox-X program or configuration directory.
If any of them have the same file names as the built-in ones (e.g. EDIT.COM
and XCOPY.EXE in the "DOS" directory), they will replace the built-in ones.

You may also hide or remove built-in files on the Z drive with config option
"drive z hide files" in [dos] section of the DOSBox-X configuration; the Z:
drive itself can also be moved to a different drive with a DOS command like
"mount -z y:", which moves the Z: drive to the Y: drive.

Please look at the DOSBox-X Wiki (https://dosbox-x.com/wiki) for more details
on customizations of the Z: drive and other information.
