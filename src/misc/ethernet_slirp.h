/*
 *  Copyright (C) 2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_ETHERNET_SLIRP_H
#define DOSBOX_ETHERNET_SLIRP_H

#include "config.h"

#if C_SLIRP

#include "ethernet.h"
#include <slirp/libslirp.h>
#include <list>

/*
 * libslirp really wants a poll() API, so we'll use that when we're
 * not on Windows. When we are on Windows, we'll fall back to using
 * select() as well as some Windows APIs.
 *
 * For reference, QEMU seems to use glib's g_poll() API.
 * This doesn't seem to work on Windows from my tests.
 */
#ifndef WIN32
#include <poll.h>
#endif

/** A libslirp timer
 * libslirp has to simulate periodic tasks such as IPv6 router
 * advertisements. It does this by giving us a callback and expiry
 * time. We have to hold on to it and call the callback when the
 * time is right.
 */
struct slirp_timer {
	int64_t expires; /*!< When to fire the callback, in nanoseconds */
	SlirpTimerCb cb; /*!< The callback to fire */
	void* cb_opaque; /*!< Data libslirp wants us to pass to the callback */
};

/** A libslirp-based Ethernet connection
 * This backend uses a virtual Ethernet device. Only TCP, UDP and some ICMP
 * work over this interface. This is because libslirp terminates guest
 * connections during routing and passes them to sockets created in the host.
 */
class SlirpEthernetConnection : public EthernetConnection {
	public:
		/* Boilerplate EthernetConnection interface */
		SlirpEthernetConnection();
		~SlirpEthernetConnection();
		bool Initialize(Section* config);
		void SendPacket(const uint8_t* packet, int len);
		void GetPackets(std::function<void(const uint8_t*, int)> callback);

		/* Called by libslirp when it has a packet for us */
		void ReceivePacket(const uint8_t* packet, int len);

                /* Called by libslirp to create, free and modify timers */
		struct slirp_timer* TimerNew(SlirpTimerCb cb, void *cb_opaque);
		void TimerFree(struct slirp_timer* timer);
		void TimerMod(struct slirp_timer* timer, int64_t expire_time);

                /* Called by libslirp to interact with our polling system */
		int PollAdd(int fd, int slirp_events);
		int PollGetSlirpRevents(int idx);
		void PollRegister(int fd);
		void PollUnregister(int fd);

	private:
		/* Runs and clears all the timers */
		void TimersRun();
		void TimersClear();

                /* Builds a list of descriptors and polls them */
		void PollsAddRegistered();
		void PollsClear();
		bool PollsPoll(uint32_t timeout_ms);

		Slirp* slirp = nullptr; /*!< Handle to libslirp */
		SlirpConfig config = { 0 }; /*!< Configuration passed to libslirp */
		SlirpCb slirp_callbacks = { 0 }; /*!< Callbacks used by libslirp */
		std::list<struct slirp_timer*> timers; /*!< Stored timers */
                
		/** The GetPacket callback
		 * When libslirp has a new packet for us it calls ReceivePacket,
		 * but the EthernetConnection interface requires users to poll
		 * for new packets using GetPackets. We temporarily store the
		 * callback from GetPackets here for ReceivePacket.
		 * This might seem racy, but keep in mind we control when
		 * libslirp sends us packets via our polling system.
		 */
		std::function<void(const uint8_t*, int)> get_packet_callback;

		std::list<int> registered_fds; /*!< File descriptors to watch */

#ifndef WIN32
		std::vector<struct pollfd> polls; /*!< Descriptors for poll() */
#else
		fd_set readfds; /*!< Read descriptors for select() */
		fd_set writefds; /*!< Write descriptors for select() */
		fd_set exceptfds; /*!< Exceptional descriptors for select() */
#endif
};

#endif

#endif
