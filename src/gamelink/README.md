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
gamelink master = true

[render]
scaler=xbrz

[dos]
hma                                              = false
drive z expand path                              = false
minimum dos initial private segment              = 0
minimum mcb segment                              = 0
minimum mcb free                                 = 187
maximum environment block size on exec           = 32768
additional environment block size on exec        = 83
kernel allocation in umb                         = true
shellhigh                                        = true

[config]
set path=z:\system
set prompt=
set comspec=z:\
```

The [dos] and [config] sections are mandatory with exactly these settings!
DOSBox-X has a different default memory layout than mainline DOSBox, the
shown settings fix this. This is needed so that Game Link clients see the
same memory content no matter which DOSBox version you use.

If you use the `xbrz` scaler, you must use `windowresolution` to set the frame
buffer size that is sent to GC. For all other scalers, the native
(software-scaled) resolution is used. GC will then perform scaling of this
fixed-resolution image to the appropriate resolution, including optional
aspect correction.

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
output = surface
# ... or any other non-gamelink output
gamelink master = true

[dos]
hma                                              = false
drive z expand path                              = false
minimum dos initial private segment              = 0
minimum mcb segment                              = 0
minimum mcb free                                 = 187
maximum environment block size on exec           = 32768
additional environment block size on exec        = 83
kernel allocation in umb                         = true
shellhigh                                        = true

[config]
set path=z:\system
set prompt=
set comspec=z:\
```

Add the `gamelink master` option and the shown extra settings to an existing
working configuration and GC can track your movement through your game world.
Everything else should work as normal. See above for an explanation why those
extra settings are needed.



Supported Games
---------------

Grid Cartographer (GC) comes with a library of game profiles. Each game needs
a custom profile, since the memory addresses of interesting values varies
from game to game. See the GC web site and the web in general for details.



Compatibility
-------------

... or "My game works in DOSBox-GRIDC but not in DOSBox-X!"

As already mentioned above, DOSBox-X loads executables at different addresses
from the reference DOSBox-GRIDC. The following extra settings will configure
DOSBox-X for a memory layout that is virtually indistinguishable from
mainline DOSBox:

```

[dos]
hma                                              = false
drive z expand path                              = false
minimum dos initial private segment              = 0
minimum mcb segment                              = 0
minimum mcb free                                 = 187
maximum environment block size on exec           = 32768
additional environment block size on exec        = 83
kernel allocation in umb                         = true
shellhigh                                        = true

[config]
set path=z:\system
set prompt=
set comspec=z:\
```

Note that you will not have a command prompt, unfortunately. But you can type
commands just fine, everything should work as normal.

There is one small hack in there: the COMSPEC environment variable had to be
shortened because the default DOSBOX-X environment is bigger than mainline
DOSBox, and there are actually games sensitive to that (e.g. Ultima 1).
However, this setting might create problems with other games, so you can try
to use these two values instead:

```
[dos]
additional environment block size on exec        = 72

[config]
set comspec=z:\command.com
```

If nothing works, there is a more complicated solution. You need to find out
the game's original load address and tell DOSBox this address. It works like
this:


1. You need to run the game in the original DOSBox-GRIDC together with GC
itself. Load a save game so that your position is detected in GC.

2. Then you run the same game in DOSBox-X, which has been configured with one
extra option:

```
[sdl]
gamelink snoop = true
```

3. Load the same save game in DOSBox-X instances and move to the same
place. Once a match is detected, a popup dialog window should tell you a
configuration value for DOSBox-X.

4. Enter the setting from the dialog window into the DOSBox-X configuration
file, for example like this:

```
[sdl]
gamelink load address = 6768
```

5. Remember to turn off the `gamelink snoop` setting and enjoy your game.



Developer Information
---------------------

To write your own client, look at `src/gamelink/gamelink.cpp` to get an idea
how data is transmitted back and forth via one mutex and one shared memory
segment. `src/gamelink/gamelink.h` contains defititions of the structs that
make up the shared memory. Access to that memory is only permitted while
holding the mutex.
