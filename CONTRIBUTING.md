## First steps

Before contributing to DOSBox-X, please check the [issue tracker](https://github.com/joncampbell123/dosbox-x/issues) to see whether your potential contribution is already being discussed, or if you'd like to discuss about it first.

## Scope of contributions that are welcome

- The normal operation of DOS games and applications within DOSBox-X 
- DOS, Windows 1.0/2.x/3.x and Windows 95/98/ME guest system support
- Architectural symmetry with a real DOS environment (particularly MS-DOS or PC DOS)
- Improving the user interface, general functions and usability of DOSBox-X
- Improving a platform-specific build of DOSBox-X or getting it on par with other platforms supported
- Non-implemented (or incorrectly implemented) system calls or functions
- Missing, incorrect, or inaccurate hardware emulation, e.g. video, sound, input
- Bug fixes and/or improvements that allow different types of DOS software to run in DOSBox-X

Our goal is to eventually make DOSBox-X a complete emulation package that covers all pre-2000 DOS and Windows 3.x/9x based system scenarios. Compared with DOSBox, DOSBox-X focuses more on general emulation and accuracy. It is our hope to give users all the options to tweak and configure the DOS virtual machine, from original IBM PC hardware with 64KB of RAM all the way up to late 1990's hardware, whatever it takes to get the DOS game or software package to run, and give users the feeling that they are running native DOS systems. Help is greatly appreciated since the main project developer only has limited time to work on DOSBox-X. We also hope that DOSBox-X (along with [DOSLIB](https://github.com/joncampbell123/doslib) and [Hackipedia](http://hackipedia.org/)) can [help with new DOS developments](https://dosbox-x.com/newdosdevelopment.html).

You do not necessarily need to be versed in coding; there are other ways to contribute to the project as well. For example, you can submit bug reports and suggest enhancements or other improvements in the [issue tracker](https://github.com/joncampbell123/dosbox-x/issues). General conversations and helping other DOSBox-X users (e.g. suggesting solutions to their problems) are appreciated as well. You may also help improve DOSBox-X's website and documentation (such as the [DOSBox-X Wiki](https://dosbox-x.com/wiki)), translate its language file to another language, and/or help packaging the software for different platforms.


## Code

Information on building/compiling the DOSBox-X source code can be found in the [BUILD](BUILD.md) page.

For the code style, please refer to the .editorconfig file, your development environment probably supports it (see https://editorconfig.org/).

The emphasis is put on code clarity, simplicity and documented whenever possible.

Documenting can be 'dynamic', Doxygen-commented methods are preferred but evocative method names are also accepted as long as they're self-explanative.

Recommendations about coding:
- favor the writing of short methods, opposed to long methods, with the exception for really simple to understand code
- maximize code reuse, i.e. don't repeat yourself (DRY)
- simplify your code path for extra clarity, avoid crazing nesting

Obviously when you'll dig in, you will quickly spot that there are many places where these rules are simply not followed ! Over the years, hundreds of people have worked on DOSBox (which DOSBox-X is a fork of), while that helped to raise this project to where it is today, it certainly didn't in terms of clean code. But keep the faith, a huge code cleanup endeavor has started with the goal to raise the metrics of DOSBox-X guts, hopefully ASAP.

For more descriptions of the source code, please take a look at the [DOSBox-X source code description](README.source-code-description) page. Information about the debugger is also available in the [DOSBox-X Debugger](README.debugger) page.


## Code of Conduct

Please read the [Code of Conduct](CODE_OF_CONDUCT.md) page for general information on contributing to or getting support from the project.

That said, welcome to this repository !
