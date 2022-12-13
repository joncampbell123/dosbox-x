How to use the Game Link feature
================================

Game Link is a local inter-process communication protocol that lets other
tools (notably, Grid Cartographer) connect to DOSBox and inspect memory
values and manage input and output. Clients like Grid Cartographer can track
your movement through game worlds and draw a map as you go. This README
assumes you own Grid Cartographer 4, but alternate clients can easily be
written.

This feature is based on the reference implementation found at
https://github.com/hiddenasbestos/dosbox-gridc/ and is compatible with
revision 4 of the protocol, which is the latest one as of 2022-12-01. That
reference implementation is based on DOSBox SVN, so it lacks a lot of
features that DOSBox-X has.

GameLink can operate in two modes: fully remote controlled, and tracking
only.


Fully Remote / Integrated Mode
------------------------------

In remote-controlled mode, DOSBox transmits the screen contents to Grid
Cartographer (GC), which displays it inside its main window. Likewise,
keyboard/mouse input is sent by GC. There will still be a DOSBox window, but
it will display nothing, you can simply minimize it.

An example config looks like this:

```
[sdl]
output=gamelink
windowresolution=1280x800

[render]
scaler=xbrz
```

`windowresolution` sets the (fixed) frame buffer size that is sent to GC. GC
will then perform scaling of this fixed-resolution image to the appropriate
resolution, including optional aspect correction.

Hardware scalers are not available beyond what GC offers, but it is possible
to use a DOSBox software scaler in addition to that. See the example above
using the `xbrz` scaler.



Tracking Mode
-------------

In tracking mode, DOSBox will work as usual. Grid Cartographer (GC) or other
clients can inspect memory, but don't get input/output control. This mode is
especially useful in dual-screen setups.

An example config looks like this:

```
[sdl]
gamelinkmaster = true
```

Just add this single option to an existing working configuration and GC can
track your movement through your game world. Everything else should work as
normal.



Supported Games
---------------

Grid Cartographer (GC) comes with a library of game profiles. Each game needs
a custom profile, since the memory addresses of interesting values varies
from game to game. See the GC web site and the web in general for details.



Compatibility
-------------

Unfortunately, DOSBox-X loads executables at different addresses from the
reference DOSBox-GRIDC. This breaks all game profiles. There are two
workarounds in place to deal with this:

A default offset will be added to every memory address request. This should
work with some simple programs.

For all other programs, you need to find out its original load address and
modify the GC profile to tell DOSBox this address.  This is more involved:


1. You need to run the game in the original DOSBox-GRIDC together with GC
itself.

2. Then you run the same game in DOSBox-X, which has been configured with one
extra option:

```
[sdl]
gamelinksnoop = true
```

3. Load the same save game in both DOSBox instances, and the console output of
DOSBox-X should contain a message telling you the original load address.

4. Find the game profile XML file and locate the `<peek>` tag inside. It will
contain a list of addresses.

5. Add `0x1000'0000` to that load address (so `14c0` becomes `100014c0`) and add the
result to the end of this list. 

6. Exit everything, Restart GC and run DOSBox-X without `gamelinksnoop`

7. Enjoy MT32 music while mapping in GC or any of the other DOSBox-X features.




Developer Information
---------------------

To write your own client, look at `src/gamelink/gamelink.cpp` to get an idea
how data is transmitted back and forth via one mutex and one shared memory
segment. `src/gamelink/gamelink.h` contains defititions of the structs that
make up the shared memory. Access to that memory is only permitted while
holding the mutex.
