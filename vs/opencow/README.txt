Opencow: Open Layer for Unicode                 http://opencow.sourceforge.net/
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
forked: https://github.com/crazii/opencow

Introduction
------------
Opencow is an open-source friendly replacement library for the Microsoft Layer 
for Unicode. This library allows a Unicode Windows application to run on all 
versions of Windows including 95, 98 and ME.


Licence
-------
Opencow is tri-licenced with MPL 1.1/GPL 2.0/LGPL 2.1 in the same manner as the
Mozilla project is. See the file LICENCE.txt for the text of the licence. The
short version of this means that Opencow may be used in both open-source and 
closed-source commercial projects. Modifications to the code do not need to be
made public, although it would be highly appreciated if any bug fixes or 
alternative implementations were passed on for integration with the main 
opencow release.


Implementation
--------------
Opencow does not currently support the complete API that MSLU supports. See
the MSDN documentation for details of which API calls are supported by MSLU.
See the file src/opencow.def for the list of which API calls are currently 
supported by Opencow. The intention is to eventually support the same API set
that MSLU supports so that opencow.dll is a complete MSLU replacement.


Usage
-----
1. Modify your application to build in Unicode mode 
    VC6:  remove the definition of _MBCS, and define _UNICODE and UNICODE
    VC7+: change the 'Character Set' to 'Use Unicode Character Set' in 
          the 'General' project properties 

2. Download the binary release of opencow.dll

3. Download the latest release of libunicows, see links (a).

4. Build your project, ensuring that the first library on the link line is
   libunicows. See the documentation of libunicows for details.

5. Distribute your application with opencow.dll in the same directory as the
   application.


Building
--------
1. If compiling with Visual Studio 6 you will need to install the Microsoft 
   Platform SDK. See links (c). Visual Studio .NET doesn't require the 
   Platform SDK to compile the project.
   
2. Open opencow.dsw and build all projects.


Debugging
---------
It is possible to have message boxes displayed by opencow for debugging
purposes in both debug and release builds. The message boxes are enabled
by setting an environment variable "OPENCOW_DEBUG" to one of the following
values:

    "0"     Disable all debug message boxes. (release build default)
    "1"     Display a message box only on DLL load and unload.
    "2"     Display a message box for DLL load, unload, and for every 
            call to GetProcAddress for a function which should be 
            implemented by opencow. (debug build default)

It is possible to disable the debug message boxes in a running application 
by choosing the 'Cancel' button when a message box is displayed.


Notes
-----

1. There is a feature(?) in VS.NET 2003 which prevents libunicows from 
   overriding the unicode API when linking with debug information. You
   must build using VC6, or use the release version of the application 
   in order to test libunicow/opencow.
   

Technical Description
---------------------
The Microsoft Layer for Unicode (MSLU or Unicows) is a DLL (unicows.dll) and 
import library (unicows.lib) distributed by Microsoft which allows a single 
32-bit unicode application to run on versions of Microsoft Windows which do 
not natively support unicode applications (95, 98 and ME). The Microsft 
library is unsurprisingly distributed under a licence which is not compatible 
with open source software (b).

The support for Unicode on Windows 95/98/ME is provided by implementing the 
Unicode Win32 API in the DLL. This DLL is not used by Windows platforms 
which natively support unicode, but only by Windows 95/98/ME. Internally 
the unicode Win32 API translates strings between the unicode encoding used 
for input and output parameters and the multi-byte codepage strings supported 
by the OS, calling the ANSI Win32 API with the translated strings.

This project, the Open Layer for Unicode (opencow) is designed to be a 
drop-in replacement for the MSLU, providing the exact same functionality with 
an open source compatible licence. The final deliverable of this project is a 
single compiled DLL (opencow.dll) which implements the same external interface 
as the Microsoft DLL. This DLL is used by software projects along with 
libunicows (A).

This project came about due to the Mozilla project's need to have an 
open-source compatible library for MSLU and is expected to be used to provide 
Win32 unicode functionality for Mozilla (b).


Links
-----
(a) libunicows
    http://libunicows.sourceforge.net/
    
(b) Mozilla bug 239279, Need Mozilla binary that supports Windows unicode API
    https://bugzilla.mozilla.org/show_bug.cgi?id=239279
    
(c) Microsoft Platform SDK
    http://www.microsoft.com/msdownload/platformsdk/sdkupdate/
    

Contact
-------
You may contact the author, Brodie Thiesfield, at brofield@jellycan.com
