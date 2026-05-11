/*
  CHATD:  A chat server using the SDL example network library
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

/* Note that this isn't necessarily the way to run a chat system.
   This is designed to excercise the network code more than be really
   functional.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "SDL.h"
#include "SDL_net.h"
#include "chat.h"

/* This is really easy.  All we do is monitor connections */

static TCPsocket servsock = NULL;
static SDLNet_SocketSet socketset = NULL;
static struct {
    int active;
    TCPsocket sock;
    IPaddress peer;
    Uint8 name[256+1];
} people[CHAT_MAXPEOPLE];


void HandleServer(void)
{
    TCPsocket newsock;
    int which;
    unsigned char data;

    newsock = SDLNet_TCP_Accept(servsock);
    if ( newsock == NULL ) {
        return;
    }

    /* Look for unconnected person slot */
    for ( which=0; which<CHAT_MAXPEOPLE; ++which ) {
        if ( ! people[which].sock ) {
            break;
        }
    }
    if ( which == CHAT_MAXPEOPLE ) {
        /* Look for inactive person slot */
        for ( which=0; which<CHAT_MAXPEOPLE; ++which ) {
            if ( people[which].sock && ! people[which].active ) {
                /* Kick them out.. */
                data = CHAT_BYE;
                SDLNet_TCP_Send(people[which].sock, &data, 1);
                SDLNet_TCP_DelSocket(socketset,
                        people[which].sock);
                SDLNet_TCP_Close(people[which].sock);
#ifdef DEBUG
                SDL_Log("Killed inactive socket %d\n", which);
#endif
                break;
            }
        }
    }
    if ( which == CHAT_MAXPEOPLE ) {
        /* No more room... */
        data = CHAT_BYE;
        SDLNet_TCP_Send(newsock, &data, 1);
        SDLNet_TCP_Close(newsock);
#ifdef DEBUG
        SDL_Log("Connection refused -- chat room full\n");
#endif
    } else {
        /* Add socket as an inactive person */
        people[which].sock = newsock;
        people[which].peer = *SDLNet_TCP_GetPeerAddress(newsock);
        SDLNet_TCP_AddSocket(socketset, people[which].sock);
#ifdef DEBUG
        SDL_Log("New inactive socket %d\n", which);
#endif
    }
}

/* Send a "new client" notification */
void SendNew(int about, int to)
{
    char data[512];
    int n;

    n = strlen((char *)people[about].name)+1;
    data[0] = CHAT_ADD;
    data[CHAT_ADD_SLOT] = about;
    memcpy(&data[CHAT_ADD_HOST], &people[about].peer.host, 4);
    memcpy(&data[CHAT_ADD_PORT], &people[about].peer.port, 2);
    data[CHAT_ADD_NLEN] = n;
    memcpy(&data[CHAT_ADD_NAME], people[about].name, n);
    SDLNet_TCP_Send(people[to].sock, data, CHAT_ADD_NAME+n);
}

void HandleClient(int which)
{
    char data[512];
    int i;

    /* Has the connection been closed? */
    if ( SDLNet_TCP_Recv(people[which].sock, data, 512) <= 0 ) {
#ifdef DEBUG
        SDL_Log("Closing socket %d (was%s active)\n",
                which, people[which].active ? "" : " not");
#endif
        /* Notify all active clients */
        if ( people[which].active ) {
            people[which].active = 0;
            data[0] = CHAT_DEL;
            data[CHAT_DEL_SLOT] = which;
            for ( i=0; i<CHAT_MAXPEOPLE; ++i ) {
                if ( people[i].active ) {
                    SDLNet_TCP_Send(people[i].sock,data,CHAT_DEL_LEN);
                }
            }
        }
        SDLNet_TCP_DelSocket(socketset, people[which].sock);
        SDLNet_TCP_Close(people[which].sock);
        people[which].sock = NULL;
    } else {
        switch (data[0]) {
            case CHAT_HELLO: {
                /* Yay!  An active connection */
                memcpy(&people[which].peer.port,
                        &data[CHAT_HELLO_PORT], 2);
                memcpy(people[which].name,
                        &data[CHAT_HELLO_NAME], 256);
                people[which].name[256] = 0;
#ifdef DEBUG
                SDL_Log("Activating socket %d (%s)\n",
                        which, people[which].name);
#endif
                /* Notify all active clients */
                for ( i=0; i<CHAT_MAXPEOPLE; ++i ) {
                    if ( people[i].active ) {
                        SendNew(which, i);
                    }
                }

                /* Notify about all active clients */
                people[which].active = 1;
                for ( i=0; i<CHAT_MAXPEOPLE; ++i ) {
                    if ( people[i].active ) {
                        SendNew(i, which);
                    }
                }
            }
            break;
            default: {
                /* Unknown packet type?? */;
            }
            break;
        }
    }
}

static void cleanup(int exitcode)
{
    if ( servsock != NULL ) {
        SDLNet_TCP_Close(servsock);
        servsock = NULL;
    }
    if ( socketset != NULL ) {
        SDLNet_FreeSocketSet(socketset);
        socketset = NULL;
    }
    SDLNet_Quit();
    SDL_Quit();
    exit(exitcode);
}

int main(int argc, char *argv[])
{
    IPaddress serverIP;
    int i;

    /* Initialize SDL */
    if ( SDL_Init(0) < 0 ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s\n", 
                     SDL_GetError());
        exit(1);
    }

    /* Initialize the network */
    if ( SDLNet_Init() < 0 ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize net: %s\n", 
                     SDLNet_GetError());
        SDL_Quit();
        exit(1);
    }

    /* Initialize the channels */
    for ( i=0; i<CHAT_MAXPEOPLE; ++i ) {
        people[i].active = 0;
        people[i].sock = NULL;
    }

    /* Allocate the socket set */
    socketset = SDLNet_AllocSocketSet(CHAT_MAXPEOPLE+1);
    if ( socketset == NULL ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't create socket set: %s\n",
                     SDLNet_GetError());
        cleanup(2);
    }

    /* Create the server socket */
    SDLNet_ResolveHost(&serverIP, NULL, CHAT_PORT);
    SDL_Log("Server IP: %x, %d\n", serverIP.host, serverIP.port);
    servsock = SDLNet_TCP_Open(&serverIP);
    if ( servsock == NULL ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't create server socket: %s\n",
                     SDLNet_GetError());
        cleanup(2);
    }
    SDLNet_TCP_AddSocket(socketset, servsock);

    /* Loop, waiting for network events */
    for ( ; ; ) {
        /* Wait for events */
        SDLNet_CheckSockets(socketset, ~0);

        /* Check for new connections */
        if ( SDLNet_SocketReady(servsock) ) {
            HandleServer();
        }

        /* Check for events on existing clients */
        for ( i=0; i<CHAT_MAXPEOPLE; ++i ) {
            if ( SDLNet_SocketReady(people[i].sock) ) {
                HandleClient(i);
            }
        }
    }
    cleanup(0);

    /* Not reached, but fixes compiler warnings */
    return 0;
}

