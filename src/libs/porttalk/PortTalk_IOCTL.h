/******************************************************************************/
/*                                                                            */
/*                    PortTalk Driver for Windows NT/2000/XP                  */
/*                        Version 2.0, 12th January 2002                      */
/*                          http://www.beyondlogic.org                        */
/*                                                                            */
/* Copyright  2002 Craig Peacock. Craig.Peacock@beyondlogic.org              */
/* Any publication or distribution of this code in source form is prohibited  */
/* without prior written permission of the copyright holder. This source code */
/* is provided "as is", without any guarantee made as to its suitability or   */
/* fitness for any particular use. Permission is herby granted to modify or   */
/* enhance this sample code to produce a derivative program which may only be */
/* distributed in compiled object form only.                                  */
/******************************************************************************/

#define PORTTALK_TYPE 40000 /* 32768-65535 are reserved for customers */

// The IOCTL function codes from 0x800 to 0xFFF are for customer use.

#define IOCTL_IOPM_RESTRICT_ALL_ACCESS \
    CTL_CODE(PORTTALK_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_IOPM_ALLOW_EXCUSIVE_ACCESS \
    CTL_CODE(PORTTALK_TYPE, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SET_IOPM \
    CTL_CODE(PORTTALK_TYPE, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_ENABLE_IOPM_ON_PROCESSID \
    CTL_CODE(PORTTALK_TYPE, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_READ_PORT_UCHAR \
    CTL_CODE(PORTTALK_TYPE, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_WRITE_PORT_UCHAR \
    CTL_CODE(PORTTALK_TYPE, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)

