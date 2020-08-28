## First steps

Before contributing, please check the Issues tab to see whether your potential contribution is already being discussed, or if you'd like to discuss about it first.

## Scope of contributions that are welcome

- the normal operation of DOS games and applications within DOSBox-X 
- DOS, Windows 3.x and Wiindows 9x guest system support
- architectural symmetry with a real DOS environment
- improving a platform-specific build of DOSBox-X or getting it on par with other platforms supported
- non-implemented (or incorrectly) system calls
- missing or incorrect hardware emulation, e.g. video, sound, input
- fixes and/or improvements that allows demoscene productions to run in DOSBox-X

You do not necessarily need to be versed in coding, in this case you will certainly spend most of your time in the Issues tab; either submitting bugs or asking for new features. You can also help improve DOSBox-X's documentation or translate its language file to another lanuage.

## Code

For the code style, please refer to the .editorconfig file, your development environment probably supports it (see https://editorconfig.org/).

The emphasis is put on code clarity, simplicity and documented whenever possible.

Documenting can be 'dynamic', Doxygen-commented methods are preferred but evocative method names are also accepted as long as they're self-explanative.

Recommendations about coding:
- favor the writing of short methods, opposed to long methods, with the exception for really simple to understand code
- maximize code reuse, i.e. don't repeat yourself (DRY)
- simplify your code path for extra clarity, avoid crazing nesting

Obviously when you'll dig in, you will quickly spot that there are many places where these rules are simply not followed ! Over the years, hundreds of people have worked on DOSBox (which DOSBox-X is a fork of), while that helped to raise this project to where it is today, it certainly didn't in terms of clean code. But keep the faith, a huge code cleanup endeavor has started with the goal to raise the metrics of DOSBox-X guts, hopefully ASAP.

## Code of Conduct

Please read CODE_OF_CONDUCT.md

That said, welcome to this repository !
