Collected here are "grind" dumps of the DIV instruction on various Intel processors.

Officially, DIV is not really supposed to modify any flags by Intel documentation.
But, at least on Pentium and lower processors, the flags do in fact change.
And in some cases, how these flags change can mean the difference between correct
and incorrect CPU detection by DOS programs (i.e. detecting the difference between
an Intel 386 vs a NexGen 5x86).

To preserve prior weird behavior, dumps of the DIV instruction's behavior are provided
here.

