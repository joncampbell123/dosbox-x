/*
  CHAT:  A chat client using the SDL example network and GUI libraries
  Copyright (C) 1997-2012 Sam Lantinga <slouken@libsdl.org>

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
    Pleasanton, CA 94588 (USA)
    slouken@devolution.com
*/

/* Note that this isn't necessarily the way to run a chat system.
   This is designed to excercise the network code more than be really
   functional.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL_net.h"
#ifdef macintosh
#include "GUI.h"
#include "GUI_widgets.h"
#else
#include <GUI/GUI.h>
#include <GUI/GUI_widgets.h>
#endif
#include "chat.h"


/* Global variables */
static TCPsocket tcpsock = NULL;
static UDPsocket udpsock = NULL;
static SDLNet_SocketSet socketset = NULL;
static UDPpacket **packets = NULL;
static struct {
	int active;
	Uint8 name[256+1];
} people[CHAT_MAXPEOPLE];

static GUI *gui = NULL;
static GUI_TermWin *termwin;
static GUI_TermWin *sendwin;
enum image_names {
	IMAGE_QUIT,
	IMAGE_SCROLL_UP,
	IMAGE_SCROLL_DN,
	NUM_IMAGES
};
char *image_files[NUM_IMAGES] = {
	"quit.bmp", "scroll_up.bmp", "scroll_dn.bmp"
};
SDL_Surface *images[NUM_IMAGES];


void SendHello(char *name)
{
	IPaddress *myip;
	char hello[1+1+256];
	int i, n;

	/* No people are active at first */
	for ( i=0; i<CHAT_MAXPEOPLE; ++i ) {
		people[i].active = 0;
	}
	if ( tcpsock != NULL ) {
		/* Get our chat handle */
		if ( (name == NULL) &&
		     ((name=getenv("CHAT_USER")) == NULL) &&
		     ((name=getenv("USER")) == NULL ) ) {
			name="Unknown";
		}
		termwin->AddText("Using name '%s'\n", name);

		/* Construct the packet */
		hello[0] = CHAT_HELLO;
		myip = SDLNet_UDP_GetPeerAddress(udpsock, -1);
		memcpy(&hello[CHAT_HELLO_PORT], &myip->port, 2);
		if ( strlen(name) > 255 ) {
			n = 255;
		} else {
			n = strlen(name);
		}
		hello[CHAT_HELLO_NLEN] = n;
		strncpy(&hello[CHAT_HELLO_NAME], name, n);
		hello[CHAT_HELLO_NAME+n++] = 0;

		/* Send it to the server */
		SDLNet_TCP_Send(tcpsock, hello, CHAT_HELLO_NAME+n);
	}
}

void SendBuf(char *buf, int len)
{
	int i;

	/* Redraw the prompt and add a newline to the buffer */
	sendwin->Clear();
	sendwin->AddText(CHAT_PROMPT);
	buf[len++] = '\n';

	/* Send the text to each of our active channels */
	for ( i=0; i < CHAT_MAXPEOPLE; ++i ) {
		if ( people[i].active ) {
			if ( len > packets[0]->maxlen ) {
				len = packets[0]->maxlen;
			}
			memcpy(packets[0]->data, buf, len);
			packets[0]->len = len;
			SDLNet_UDP_Send(udpsock, i, packets[0]);
		}
	}
}
void SendKey(SDLKey key, Uint16 unicode)
{
	static char keybuf[80-sizeof(CHAT_PROMPT)+1];
	static int  keypos = 0;
	unsigned char ch;

	/* We don't handle wide UNICODE characters yet */
	if ( unicode > 255 ) {
		return;
	}
	ch = (unsigned char)unicode;

	/* Add the key to the buffer, and send it if we have a line */
	switch (ch) {
		case '\0':
			break;
		case '\r':
		case '\n':
			/* Send our line of text */
			SendBuf(keybuf, keypos);
			keypos = 0;
			break;
		case '\b':
			/* If there's data, back up over it */
			if ( keypos > 0 ) {
				sendwin->AddText((char *)&ch, 1);
				--keypos;
			}
			break;
		default:
			/* If the buffer is full, send it */
			if ( keypos == (sizeof(keybuf)/sizeof(keybuf[0]))-1 ) {
				SendBuf(keybuf, keypos);
				keypos = 0;
			}
			/* Add the text to our send buffer */
			sendwin->AddText((char *)&ch, 1);
			keybuf[keypos++] = ch;
			break;
	}
}

int HandleServerData(Uint8 *data)
{
	int used;

	switch (data[0]) {
		case CHAT_ADD: {
			Uint8 which;
			IPaddress newip;

			/* Figure out which channel we got */
			which = data[CHAT_ADD_SLOT];
			if ((which >= CHAT_MAXPEOPLE) || people[which].active) {
				/* Invalid channel?? */
				break;
			}
			/* Get the client IP address */
			newip.host=SDLNet_Read32(&data[CHAT_ADD_HOST]);
			newip.port=SDLNet_Read16(&data[CHAT_ADD_PORT]);

			/* Copy name into channel */
			memcpy(people[which].name, &data[CHAT_ADD_NAME], 256);
			people[which].name[256] = 0;
			people[which].active = 1;

			/* Let the user know what happened */
			termwin->AddText(
	"* New client on %d from %d.%d.%d.%d:%d (%s)\n", which,
		(newip.host>>24)&0xFF, (newip.host>>16)&0xFF,
			(newip.host>>8)&0xFF, newip.host&0xFF,
					newip.port, people[which].name);

			/* Put the address back in network form */
			newip.host = SDL_SwapBE32(newip.host);
			newip.port = SDL_SwapBE16(newip.port);

			/* Bind the address to the UDP socket */
			SDLNet_UDP_Bind(udpsock, which, &newip);
		}
		used = CHAT_ADD_NAME+data[CHAT_ADD_NLEN];
		break;
		case CHAT_DEL: {
			Uint8 which;

			/* Figure out which channel we lost */
			which = data[CHAT_DEL_SLOT];
			if ( (which >= CHAT_MAXPEOPLE) ||
						! people[which].active ) {
				/* Invalid channel?? */
				break;
			}
			people[which].active = 0;

			/* Let the user know what happened */
			termwin->AddText(
	"* Lost client on %d (%s)\n", which, people[which].name);

			/* Unbind the address on the UDP socket */
			SDLNet_UDP_Unbind(udpsock, which);
		}
		used = CHAT_DEL_LEN;
		break;
		case CHAT_BYE: {
			termwin->AddText("* Chat server full\n");
		}
		used = CHAT_BYE_LEN;
		break;
		default: {
			/* Unknown packet type?? */;
		}
		used = 0;
		break;
	}
	return(used);
}

void HandleServer(void)
{
	Uint8 data[512];
	int pos, len;
	int used;

	/* Has the connection been lost with the server? */
	len = SDLNet_TCP_Recv(tcpsock, (char *)data, 512);
	if ( len <= 0 ) {
		SDLNet_TCP_DelSocket(socketset, tcpsock);
		SDLNet_TCP_Close(tcpsock);
		tcpsock = NULL;
		termwin->AddText("Connection with server lost!\n");
	} else {
		pos = 0;
		while ( len > 0 ) {
			used = HandleServerData(&data[pos]);
			pos += used;
			len -= used;
			if ( used == 0 ) {
				/* We might lose data here.. oh well,
				   we got a corrupt packet from server
				 */
				len = 0;
			}
		}
	}
}
void HandleClient(void)
{
	int n;

	n = SDLNet_UDP_RecvV(udpsock, packets);
	while ( n-- > 0 ) {
		if ( packets[n]->channel >= 0 ) {
			termwin->AddText("[%s] ", 
				people[packets[n]->channel].name);
			termwin->AddText((char *)packets[n]->data, packets[n]->len);
		}
	}
}

GUI_status HandleNet(void)
{
	SDLNet_CheckSockets(socketset, 0);
	if ( SDLNet_SocketReady(tcpsock) ) {
		HandleServer();
	}
	if ( SDLNet_SocketReady(udpsock) ) {
		HandleClient();
	}

	/* Redraw the screen if the window changed */
	if ( termwin->Changed() ) {
		return(GUI_REDRAW);
	} else {
		return(GUI_PASS);
	}
}

void InitGUI(SDL_Surface *screen)
{
	int x1, y1, y2;
	SDL_Rect empty_rect = { 0, 0, 0, 0 };
        GUI_Widget *widget;

	gui = new GUI(screen);

	/* Chat terminal window */
	termwin = new GUI_TermWin(0, 0, 80*8, 50*8, NULL,NULL,CHAT_SCROLLBACK);
	gui->AddWidget(termwin);

	/* Send-line window */
	y1 = termwin->H()+2;
	sendwin = new GUI_TermWin(0, y1, 80*8, 1*8, NULL, SendKey, 0);
	sendwin->AddText(CHAT_PROMPT);
	gui->AddWidget(sendwin);

	/* Add scroll buttons for main window */
	y1 += sendwin->H()+2;
	y2 = y1+images[IMAGE_SCROLL_UP]->h;
	widget = new GUI_ScrollButtons(2, y1, images[IMAGE_SCROLL_UP],
	                   empty_rect, 2, y2, images[IMAGE_SCROLL_DN],
					SCROLLBAR_VERTICAL, termwin);
	gui->AddWidget(widget);

	/* Add QUIT button */
	x1 = (screen->w-images[IMAGE_QUIT]->w)/2;
	y1 = sendwin->Y()+sendwin->H()+images[IMAGE_QUIT]->h/2;
	widget = new GUI_Button(NULL, x1, y1, images[IMAGE_QUIT], NULL);
	gui->AddWidget(widget);

	/* That's all folks */
	return;
}

extern "C"
void cleanup(int exitcode)
{
	int i;

	/* Clean up the GUI */
	if ( gui ) {
        	delete gui;
		gui = NULL;
	}
	/* Clean up any images we have */
	for ( i=0; i<NUM_IMAGES; ++i ) {
		if ( images[i] ) {
			SDL_FreeSurface(images[i]);
			images[i] = NULL;
		}
	}
	/* Close the network connections */
	if ( tcpsock != NULL ) {
		SDLNet_TCP_Close(tcpsock);
		tcpsock = NULL;
	}
	if ( udpsock != NULL ) {
		SDLNet_UDP_Close(udpsock);
		udpsock = NULL;
	}
	if ( socketset != NULL ) {
		SDLNet_FreeSocketSet(socketset);
		socketset = NULL;
	}
	if ( packets != NULL ) {
		SDLNet_FreePacketV(packets);
		packets = NULL;
	}
	SDLNet_Quit();
	SDL_Quit();
	exit(exitcode);
}

int main(int argc, char *argv[])
{
        SDL_Surface *screen;
	int i;
	char *server;
	IPaddress serverIP;

	/* Check command line arguments */
	if ( argv[1] == NULL ) {
		fprintf(stderr, "Usage: %s <server>\n", argv[0]);
		exit(1);
	}
	
        /* Initialize SDL */
        if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
                fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
                exit(1);
	}

	/* Set a 640x480 video mode -- allows 80x50 window using 8x8 font */
	screen = SDL_SetVideoMode(640, 480, 0, SDL_SWSURFACE);
	if ( screen == NULL ) {
                fprintf(stderr, "Couldn't set video mode: %s\n",SDL_GetError());
		SDL_Quit();
                exit(1);
	}

	/* Initialize the network */
	if ( SDLNet_Init() < 0 ) {
		fprintf(stderr, "Couldn't initialize net: %s\n",
						SDLNet_GetError());
		SDL_Quit();
		exit(1);
	}

	/* Get ready to initialize all of our data */

	/* Load the display font and other images */
	for ( i=0; i<NUM_IMAGES; ++i ) {
		images[i] = NULL;
	}
	for ( i=0; i<NUM_IMAGES; ++i ) {
		images[i] = SDL_LoadBMP(image_files[i]);
		if ( images[i] == NULL ) {
			fprintf(stderr, "Couldn't load '%s': %s\n",
					image_files[i], SDL_GetError());
			cleanup(2);
		}
	}

	/* Go! */
	InitGUI(screen);

	/* Allocate a vector of packets for client messages */
	packets = SDLNet_AllocPacketV(4, CHAT_PACKETSIZE);
	if ( packets == NULL ) {
		fprintf(stderr, "Couldn't allocate packets: Out of memory\n");
		cleanup(2);
	}

	/* Connect to remote host and create UDP endpoint */
	server = argv[1];
	termwin->AddText("Connecting to %s ... ", server);
	gui->Display();
	SDLNet_ResolveHost(&serverIP, server, CHAT_PORT);
	if ( serverIP.host == INADDR_NONE ) {
		termwin->AddText("Couldn't resolve hostname\n");
	} else {
		/* If we fail, it's okay, the GUI shows the problem */
		tcpsock = SDLNet_TCP_Open(&serverIP);
		if ( tcpsock == NULL ) {
			termwin->AddText("Connect failed\n");
		} else {
			termwin->AddText("Connected\n");
		}
	}
	/* Try ports in the range {CHAT_PORT - CHAT_PORT+10} */
	for ( i=0; (udpsock == NULL) && i<10; ++i ) {
		udpsock = SDLNet_UDP_Open(CHAT_PORT+i);
	}
	if ( udpsock == NULL ) {
		SDLNet_TCP_Close(tcpsock);
		tcpsock = NULL;
		termwin->AddText("Couldn't create UDP endpoint\n");
	}

	/* Allocate the socket set for polling the network */
	socketset = SDLNet_AllocSocketSet(2);
	if ( socketset == NULL ) {
		fprintf(stderr, "Couldn't create socket set: %s\n",
						SDLNet_GetError());
		cleanup(2);
	}
	SDLNet_TCP_AddSocket(socketset, tcpsock);
	SDLNet_UDP_AddSocket(socketset, udpsock);

	/* Run the GUI, handling network data */
	SendHello(argv[2]);
	gui->Run(HandleNet);
	cleanup(0);

	/* Keep the compiler happy */
	return(0);
}
