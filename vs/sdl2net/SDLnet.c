/*
  SDL_net:  An example cross-platform network library for use with SDL
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2012 Simeon Maxein <smaxein@googlemail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "SDLnetsys.h"
#include "SDL_net.h"

#ifdef WITHOUT_SDL
#include <string.h>
#include <stdarg.h>
#endif

#if defined(SDL_BUILD_MAJOR_VERSION) && defined(SDL_COMPILE_TIME_ASSERT)
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MAJOR_VERSION,
                        SDL_NET_MAJOR_VERSION == SDL_BUILD_MAJOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MINOR_VERSION,
                        SDL_NET_MINOR_VERSION == SDL_BUILD_MINOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MICRO_VERSION,
                        SDL_NET_PATCHLEVEL == SDL_BUILD_MICRO_VERSION);
#endif

#if defined(SDL_COMPILE_TIME_ASSERT)
SDL_COMPILE_TIME_ASSERT(SDL_NET_MAJOR_VERSION_min, SDL_NET_MAJOR_VERSION >= 0);
/* Limited only by the need to fit in SDL_version */
SDL_COMPILE_TIME_ASSERT(SDL_NET_MAJOR_VERSION_max, SDL_NET_MAJOR_VERSION <= 255);

SDL_COMPILE_TIME_ASSERT(SDL_NET_MINOR_VERSION_min, SDL_NET_MINOR_VERSION >= 0);
/* Limited only by the need to fit in SDL_version */
SDL_COMPILE_TIME_ASSERT(SDL_NET_MINOR_VERSION_max, SDL_NET_MINOR_VERSION <= 255);

SDL_COMPILE_TIME_ASSERT(SDL_NET_PATCHLEVEL_min, SDL_NET_PATCHLEVEL >= 0);
/* Limited by its encoding in SDL_VERSIONNUM and in the ABI versions */
SDL_COMPILE_TIME_ASSERT(SDL_NET_PATCHLEVEL_max, SDL_NET_PATCHLEVEL <= 99);
#endif

const SDLNet_version *SDLNet_Linked_Version(void)
{
    static SDLNet_version linked_version;
    SDL_NET_VERSION(&linked_version);
    return(&linked_version);
}

/* Since the UNIX/Win32/BeOS code is so different from MacOS,
   we'll just have two completely different sections here.
*/
static int SDLNet_started = 0;

#ifndef __USE_W32_SOCKETS
#include <signal.h>
#endif

#ifndef __USE_W32_SOCKETS

int SDLNet_GetLastError(void)
{
    #if defined(__OS2__) && !defined(__EMX__)
    return sock_errno();
    #else
    return errno;
    #endif
}

void SDLNet_SetLastError(int err)
{
    #if defined(__OS2__) && !defined(__EMX__)
    (void) err; /* FIXME: OS2 doesn't have a function to reset socket errno */
    #else
    errno = err;
    #endif
}

#endif

static char errorbuf[1024];

void SDLCALL SDLNet_SetError(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    SDL_vsnprintf(errorbuf, sizeof(errorbuf), fmt, argp);
    va_end(argp);
#ifndef WITHOUT_SDL
    SDL_SetError("%s", errorbuf);
#endif
}

const char * SDLCALL SDLNet_GetError(void)
{
#ifdef WITHOUT_SDL
    return errorbuf;
#else
    return SDL_GetError();
#endif
}

/* Initialize/Cleanup the network API */
int  SDLNet_Init(void)
{
    if ( !SDLNet_started ) {
#ifdef __USE_W32_SOCKETS
        /* Start up the windows networking */
        WORD version_wanted = MAKEWORD(1,1);
        WSADATA wsaData;

        if ( WSAStartup(version_wanted, &wsaData) != 0 ) {
            SDLNet_SetError("Couldn't initialize Winsock 1.1\n");
            return(-1);
        }
#elif defined(__OS2__) && !defined(__EMX__)
        if (sock_init() < 0) {
            SDLNet_SetError("Couldn't initialize IBM OS/2 sockets");
            return(-1);
        }
#else
        /* SIGPIPE is generated when a remote socket is closed */
        void (*handler)(int);
        handler = signal(SIGPIPE, SIG_IGN);
        if ( handler != SIG_DFL ) {
            signal(SIGPIPE, handler);
        }
#endif
    }
    ++SDLNet_started;
    return(0);
}

void SDLNet_Quit(void)
{
    if ( SDLNet_started == 0 ) {
        return;
    }
    if ( --SDLNet_started == 0 ) {
#ifdef __USE_W32_SOCKETS
        /* Clean up windows networking */
        if ( WSACleanup() == SOCKET_ERROR ) {
            if ( WSAGetLastError() == WSAEINPROGRESS ) {
                WSACleanup();
            }
        }
#elif defined(__OS2__) && !defined(__EMX__)
        /* -- nothing */
#else
        /* Restore the SIGPIPE handler */
        void (*handler)(int);
        handler = signal(SIGPIPE, SIG_DFL);
        if ( handler != SIG_IGN ) {
            signal(SIGPIPE, handler);
        }
#endif
    }
}

/* Resolve a host name and port to an IP address in network form */
int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port)
{
    int retval = 0;

    /* Perform the actual host resolution */
    if ( host == NULL ) {
        address->host = INADDR_ANY;
    } else {
        address->host = inet_addr(host);
        if ( address->host == INADDR_NONE ) {
            struct hostent *hp;

            hp = gethostbyname(host);
            if ( hp ) {
                SDL_memcpy(&address->host,hp->h_addr,hp->h_length);
            } else {
                retval = -1;
            }
        }
    }
    address->port = SDLNet_Read16(&port);

    /* Return the status */
    return(retval);
}

/* Resolve an ip address to a host name in canonical form.
   If the ip couldn't be resolved, this function returns NULL,
   otherwise a pointer to a static buffer containing the hostname
   is returned.  Note that this function is not thread-safe.
*/
/* Written by Miguel Angel Blanch.
 * Main Programmer of Arianne RPG.
 * http://come.to/arianne_rpg
 */
const char *SDLNet_ResolveIP(const IPaddress *ip)
{
    struct hostent *hp;
    struct in_addr in;

    hp = gethostbyaddr((const char *)&ip->host, sizeof(ip->host), AF_INET);
    if ( hp != NULL ) {
        return hp->h_name;
    }

    in.s_addr = ip->host;
    return inet_ntoa(in);
}

int SDLNet_GetLocalAddresses(IPaddress *addresses, int maxcount)
{
    int count = 0;
#ifdef SIOCGIFCONF
/* Defined on Mac OS X */
#ifndef _SIZEOF_ADDR_IFREQ
#define _SIZEOF_ADDR_IFREQ sizeof
#endif
    SOCKET sock;
    struct ifconf conf;
    char data[4096];
    struct ifreq *ifr;
    struct sockaddr_in *sock_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sock == INVALID_SOCKET ) {
        return 0;
    }

    conf.ifc_len = sizeof(data);
    conf.ifc_buf = (caddr_t) data;
    if ( ioctl(sock, SIOCGIFCONF, &conf) < 0 ) {
        closesocket(sock);
        return 0;
    }

    ifr = (struct ifreq*)data;
    while ((char*)ifr < data+conf.ifc_len) {
        if (ifr->ifr_addr.sa_family == AF_INET) {
            if (count < maxcount) {
                sock_addr = (struct sockaddr_in*)&ifr->ifr_addr;
                addresses[count].host = sock_addr->sin_addr.s_addr;
                addresses[count].port = sock_addr->sin_port;
            }
            ++count;
        }
        ifr = (struct ifreq*)((char*)ifr + _SIZEOF_ADDR_IFREQ(*ifr));
    }
    closesocket(sock);
#elif defined(__WIN32__)
    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter;
    PIP_ADDR_STRING pAddress;
    DWORD dwRetVal = 0;
    ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);

    pAdapterInfo = (IP_ADAPTER_INFO *) SDL_malloc(sizeof (IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        return 0;
    }

    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == ERROR_BUFFER_OVERFLOW) {
        pAdapterInfo = (IP_ADAPTER_INFO *) SDL_realloc(pAdapterInfo, ulOutBufLen);
        if (pAdapterInfo == NULL) {
            return 0;
        }
        dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    }

    if (dwRetVal == NO_ERROR) {
        for (pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
            for (pAddress = &pAdapter->IpAddressList; pAddress; pAddress = pAddress->Next) {
                if (count < maxcount) {
                    addresses[count].host = inet_addr(pAddress->IpAddress.String);
                    addresses[count].port = 0;
                }
                ++count;
            }
        }
    }
    SDL_free(pAdapterInfo);
#endif
    return count;
}

/* function versions for binary compatibility */

#undef SDLNet_Write16
#undef SDLNet_Write32
#undef SDLNet_Read16
#undef SDLNet_Read32

/* Write a 16/32 bit value to network packet buffer */
extern DECLSPEC void SDLCALL SDLNet_Write16(Uint16 value, void *area);
extern DECLSPEC void SDLCALL SDLNet_Write32(Uint32 value, void *area);

/* Read a 16/32 bit value from network packet buffer */
extern DECLSPEC Uint16 SDLCALL SDLNet_Read16(const void *area);
extern DECLSPEC Uint32 SDLCALL SDLNet_Read32(const void *area);

void  SDLNet_Write16(Uint16 value, void *areap)
{
    _SDLNet_Write16(value, areap);
}

void   SDLNet_Write32(Uint32 value, void *areap)
{
    _SDLNet_Write32(value, areap);
}

Uint16 SDLNet_Read16(const void *areap)
{
    return _SDLNet_Read16(areap);
}

Uint32 SDLNet_Read32(const void *areap)
{
    return _SDLNet_Read32(areap);
}
