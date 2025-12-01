# DOSBox-X Game Cheater

This project originated many years ago when I wrote a save file editor for DOS games. This editor could search for where character attributes were stored in the files and provided the functionality to modify save files. The program was quite simple, and I never published it—it was just for personal entertainment.

I've been consistently searching for game trainers that could work with DOSBox-X. Of course, I'm aware of tools like Game Buster and FPE, but on one hand, there were copyright concerns, and on the other hand, they needed to reside in the DOS environment, which often led to compatibility issues. Once, while entering the Debugger mode in DOSBox-X—which opens a separate window to monitor the currently running program—I discovered that it could dump a segment of memory content and save it as a file. By using my previous program to treat this memory dump as a game save file, I could identify the memory addresses where character attributes were stored. Combined with the memory modification commands in Debugger mode, I could then alter character attributes in the game.

Using this program has a bit of a learning curve. Users need some understanding of hexadecimal and how values are stored in memory. One wrong move could easily crash the system.

So I began studying the source code of the DOSBox-X Debugger and developed a Game Cheater that operates within the Debugger mode. The biggest advantage of this approach is that it solves the complex memory access patterns of the 80x86 CPU. For general users, if you struggle to understand the segment:offset address access method, using the linear memory mode for memory searches makes things much simpler. You also don’t need to worry about how to search memory in EMS, XMS, or even protected mode—linear memory mode resolves all these issues for you.

## Compiler

### Visual Studio
Before building the solution, you must select the Release or Debug('Release SDL2' is recommended) configuration, as well as the target platform (e.g., Win32, x64). After making these selections, follow the sequence below to open the properties window where you can enter the compilation options:

Project -> Properties -> C/C++ -> Command Line

In the input field titled Additional Options within this window, enter the following option and then click OK to compile successfully:

/utf-8

### Mingw64
Compiling with Mingw64 requires no additional steps; simply execute the following command:
./build-mingw-sdl2

Due to an unknown cause, the spacebar is unresponsive within DOSBox-X in user-compiled versions(v2025.10.07).

### Others
Since I lack access to both a Linux graphical environment and a macOS system, I cannot verify the project's functionality on these platforms.

## Full Tutor

https://knifour.gitbook.io/dosbox-x-game-cheater/

## How to Use the DOSBox-X Game Cheater

You can enter Debugger mode by selecting "Start DOSBox-X Debugger" from the DOSBox-X Debug menu when the game reaches the point where character status is displayed, or by directly pressing the Alt + Pause keys.

At this point, the game will be paused and another Debugger window will appear, containing four sub-windows:
- Register Overview
- Data view
- Code Overview
- Output

You can use the Tab and Shift+Tab keys to switch focus between windows. The focused window can be scrolled using the Up and Down arrow keys. In the Data View window, this functionality allows you to observe the contents of nearby memory areas, while in the Output window, it enables you to review messages that have scrolled out of view.

### The title bar of the focused window will be displayed in a cyan color.

At the bottom of the window is a command line starting with I->

### Commands in the command line are case-insensitive.
You can type `HELP` and then press the [Enter] key to execute the command (all command line instructions are executed in this manner), Its purpose is to display all available commands in Debugger mode. Those who are interested can explore them further on their own.

Entering `CHEAT <project name>` in the command line will activate the cheat mode.

At this point, the Code Overview will be replaced by the Cheater Overview, and the project name will be displayed after the Cheater Overview title.

After entering Game Cheater mode, you can use the `HELP` command to view all available commands in Cheater mode. Entering `HELP [command]` will display detailed explanations for that specific command.

### Whether in Debugger mode or Cheater mode, pressing [F5] will resume the game execution.

Using the `EXIT` command will exit Cheater mode. In Debugger mode, using the `QUIT` command will exit both the Debugger mode and close DOSBox-X. I haven't found a way in Debugger mode to exit the debugger while keeping the game running. Fortunately, you can minimize the Debugger window to prevent it from obstructing your game view. It is recommended to run Game Cheater on a 4K screen for a more seamless and comfortable experience.
