/* *  Copyright (C) 2020  The DOSBox Team
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

#ifndef WIN32
#include <poll.h>
#endif

struct slirp_timer {
	int64_t expires;
	SlirpTimerCb cb;
	void* cb_opaque;
};

class SlirpEthernetConnection : public EthernetConnection {
	public:
		SlirpEthernetConnection();
		~SlirpEthernetConnection();
		bool Initialize(Section* config);
		void SendPacket(const uint8_t* packet, int len);
		void GetPackets(std::function<void(const uint8_t*, int)> callback);

		void ReceivePacket(const uint8_t* packet, int len);

		struct slirp_timer* TimerNew(SlirpTimerCb cb, void *cb_opaque);
		void TimerFree(struct slirp_timer* timer);
		void TimerMod(struct slirp_timer* timer, int64_t expire_time);

		int PollAdd(int fd, int slirp_events);
		int PollGetSlirpRevents(int idx);
		void PollRegister(int fd);
		void PollUnregister(int fd);

	private:
		void TimersRun();
		void TimersClear();
		void PollsAddRegistered();
		void PollsClear();
		bool PollsPoll(uint32_t timeout_ms);

		Slirp* slirp = nullptr;
		SlirpConfig config = { 0 };
		SlirpCb slirp_callbacks = { 0 };
		std::list<struct slirp_timer*> timers;
		std::function<void(const uint8_t*, int)> get_packet_callback;
		std::list<int> registered_fds;

#ifndef WIN32
		std::vector<struct pollfd> polls;
#else
		fd_set readfds;
		fd_set writefds;
		fd_set exceptfds;
#endif
};

#endif

#endif
