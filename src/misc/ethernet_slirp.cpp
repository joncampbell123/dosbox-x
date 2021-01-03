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

#include "config.h"

#if C_SLIRP

#include "ethernet_slirp.h"
#include <time.h>
#include <algorithm>
#include "dosbox.h"

#ifdef WIN32
#if _WIN32_WINNT < 0x600
/* Very quick Windows XP-compatible inet_pton implementation */
int inet_pton_win(int af, const char* src, void* dst)
{
	if(af == AF_INET)
	{
		unsigned long* num = (unsigned long*)dst;
		*num = inet_addr(src);
	}
	LOG_MSG("SLIRP: inet_pton unimplemented for AF %i (source %s)", af, src);
	return -1;
}
#else /* _WIN32_WINNT >= 0x600 */
#include <ws2tcpip.h>
#endif /* _WIN32_WINNT */
#else /* !WIN32 */
#include <arpa/inet.h>
#endif /* WIN32 */

ssize_t slirp_receive_packet(const void* buf, size_t len, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	conn->ReceivePacket((const uint8_t*)buf, len);
	return len;
}

void slirp_guest_error(const char* msg, void* opaque)
{
	(void)opaque;
	LOG_MSG("SLIRP: ERROR: %s", msg);
}

int64_t slirp_clock_get_ns(void* opaque)
{
	(void)opaque;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	/* if clock_gettime fails we have more serious problems */
	return ts.tv_nsec + (ts.tv_sec * 1e9);
}

void *slirp_timer_new(SlirpTimerCb cb, void* cb_opaque, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	return conn->TimerNew(cb, cb_opaque);
}

void slirp_timer_free(void* timer, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	struct slirp_timer *real_timer = (struct slirp_timer*)timer;
	conn->TimerFree(real_timer);
}

void slirp_timer_mod(void* timer, int64_t expire_time, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	struct slirp_timer *real_timer = (struct slirp_timer*)timer;
	conn->TimerMod(real_timer, expire_time);
}

int slirp_add_poll(int fd, int events, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	return conn->PollAdd(fd, events);
}

int slirp_get_revents(int idx, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	return conn->PollGetSlirpRevents(idx);
}

void slirp_register_poll_fd(int fd, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	conn->PollRegister(fd);
}

void slirp_unregister_poll_fd(int fd, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	conn->PollUnregister(fd);
}

void slirp_notify(void* opaque)
{
	(void)opaque;
	return;
}

SlirpEthernetConnection::SlirpEthernetConnection()
      : EthernetConnection()
{
	slirp_callbacks.send_packet = slirp_receive_packet;
	slirp_callbacks.guest_error = slirp_guest_error;
	slirp_callbacks.clock_get_ns = slirp_clock_get_ns;
	slirp_callbacks.timer_new = slirp_timer_new;
	slirp_callbacks.timer_free = slirp_timer_free;
	slirp_callbacks.timer_mod = slirp_timer_mod;
	slirp_callbacks.register_poll_fd = slirp_register_poll_fd;
	slirp_callbacks.unregister_poll_fd = slirp_unregister_poll_fd;
	slirp_callbacks.notify = slirp_notify;
}

SlirpEthernetConnection::~SlirpEthernetConnection()
{
	if(slirp) slirp_cleanup(slirp);
}

bool SlirpEthernetConnection::Initialize(Section* dosbox_config)
{
	Section_prop* section = static_cast<Section_prop*>(dosbox_config);

	LOG_MSG("SLIRP version: %s", slirp_version_string());

	/* Config */
	config.version = 1;
	config.restricted = section->Get_bool("restricted");
	config.disable_host_loopback = section->Get_bool("disable_host_loopback");
	config.if_mtu = section->Get_int("mtu"); /* 0 = IF_MTU_DEFAULT */
	config.if_mru = section->Get_int("mru"); /* 0 = IF_MRU_DEFAULT */
	config.enable_emu = 0; /* Buggy, don't use */
	/* IPv4 */
	config.in_enabled = 1;
	inet_pton(AF_INET, section->Get_string("ipv4_network"), &config.vnetwork);
	inet_pton(AF_INET, section->Get_string("ipv4_netmask"), &config.vnetmask);
	inet_pton(AF_INET, section->Get_string("ipv4_host"), &config.vhost);
	inet_pton(AF_INET, section->Get_string("ipv4_nameserver"), &config.vnameserver);
	inet_pton(AF_INET, section->Get_string("ipv4_dhcp_start"), &config.vdhcp_start);
	/* IPv6 code is left here as reference but disabled as no DOS-era
	 * software supports it and might get confused by it */
	config.in6_enabled = 0;
	inet_pton(AF_INET6, "fec0::", &config.vprefix_addr6);
	config.vprefix_len = 64;
	inet_pton(AF_INET6, "fec0::2", &config.vhost6);
	inet_pton(AF_INET6, "fec0::3", &config.vnameserver6);
	/* DHCPv4, BOOTP, TFTP */
	config.vhostname = "DOSBox-X";
	config.vdnssearch = NULL;
	config.vdomainname = NULL;
	config.tftp_server_name = NULL;
	config.tftp_path = NULL;
	config.bootfile = NULL;

	slirp = slirp_new(&config, &slirp_callbacks, this);
	if(slirp)
	{
		LOG_MSG("SLIRP successfully initialized");
		return true;
	}
	else
	{
		LOG_MSG("SLIRP failed to initialize");
		return false;
	}
}

void SlirpEthernetConnection::SendPacket(const uint8_t* packet, int len)
{
	slirp_input(slirp, packet, len);
}

void SlirpEthernetConnection::GetPackets(std::function<void(const uint8_t*, int)> callback)
{
	get_packet_callback = callback;
	uint32_t timeout_ms = 0;
	PollsClear();
	PollsAddRegistered();
	slirp_pollfds_fill(slirp, &timeout_ms, slirp_add_poll, this);
	bool poll_failed = !PollsPoll(timeout_ms);
	slirp_pollfds_poll(slirp, poll_failed, slirp_get_revents, this);
	TimersRun();
}

void SlirpEthernetConnection::ReceivePacket(const uint8_t* packet, int len)
{
	get_packet_callback(packet, len);
}

struct slirp_timer* SlirpEthernetConnection::TimerNew(SlirpTimerCb cb, void *cb_opaque)
{
	struct slirp_timer* timer = new struct slirp_timer;
	timer->expires = 0;
	timer->cb = cb;
	timer->cb_opaque = cb_opaque;
	timers.push_back(timer);
	return timer;
}

void SlirpEthernetConnection::TimerFree(struct slirp_timer* timer)
{
	std::remove(timers.begin(), timers.end(), timer);
	delete timer;
}

void SlirpEthernetConnection::TimerMod(struct slirp_timer* timer, int64_t expire_time)
{
	/* expire_time is in milliseconds for some reason */
	timer->expires = expire_time * 1e6;
}

void SlirpEthernetConnection::TimersRun()
{
	int64_t now = slirp_clock_get_ns(NULL);
	std::for_each(timers.begin(), timers.end(), [now](struct slirp_timer*& timer)
	{
		if(timer->expires && timer->expires < now)
		{
			timer->expires = 0;
			timer->cb(timer->cb_opaque);
		}
	});
}

void SlirpEthernetConnection::TimersClear()
{
	std::for_each(timers.begin(), timers.end(), [](struct slirp_timer*& timer)
	{
		delete timer;
	});
	timers.clear();
}

void SlirpEthernetConnection::PollRegister(int fd)
{
#ifdef WIN32
	return;
	/* TODO: fd may be invalid */
#endif
	PollUnregister(fd);
	registered_fds.push_back(fd);
}

void SlirpEthernetConnection::PollUnregister(int fd)
{
	std::remove(registered_fds.begin(), registered_fds.end(), fd);
}

void SlirpEthernetConnection::PollsAddRegistered()
{
	std::for_each(registered_fds.begin(), registered_fds.end(), [this](int fd)
	{
		PollAdd(fd, SLIRP_POLL_IN | SLIRP_POLL_OUT);
	});
}

#ifndef WIN32

void SlirpEthernetConnection::PollsClear()
{
	polls.clear();
}

int SlirpEthernetConnection::PollAdd(int fd, int slirp_events)
{
	int real_events = 0;
	if(slirp_events & SLIRP_POLL_IN)  real_events |= POLLIN;
	if(slirp_events & SLIRP_POLL_OUT) real_events |= POLLOUT;
	if(slirp_events & SLIRP_POLL_PRI) real_events |= POLLPRI;
	struct pollfd new_poll;
	new_poll.fd = fd;
	new_poll.events = real_events;
	polls.push_back(new_poll);
	return (polls.size() - 1);
}

bool SlirpEthernetConnection::PollsPoll(uint32_t timeout_ms)
{
	int ret = poll(polls.data(), polls.size(), timeout_ms);
	return (ret > -1);
}

int SlirpEthernetConnection::PollGetSlirpRevents(int idx)
{
	int real_revents = polls.at(idx).revents;
	int slirp_revents = 0;
	if(real_revents & POLLIN)  slirp_revents |= SLIRP_POLL_IN;
	if(real_revents & POLLOUT) slirp_revents |= SLIRP_POLL_OUT;
	if(real_revents & POLLPRI) slirp_revents |= SLIRP_POLL_PRI;
	if(real_revents & POLLERR) slirp_revents |= SLIRP_POLL_ERR;
	if(real_revents & POLLHUP) slirp_revents |= SLIRP_POLL_HUP;
	return slirp_revents;
}

#else

void SlirpEthernetConnection::PollsClear()
{
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
}

int SlirpEthernetConnection::PollAdd(int fd, int slirp_events)
{
	if(slirp_events & SLIRP_POLL_IN)  FD_SET(fd, &readfds);
	if(slirp_events & SLIRP_POLL_OUT) FD_SET(fd, &writefds);
	if(slirp_events & SLIRP_POLL_PRI) FD_SET(fd, &exceptfds);
	return fd;
}

bool SlirpEthernetConnection::PollsPoll(uint32_t timeout_ms)
{
	struct timeval timeout;
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;
	int ret = select(0, &readfds, &writefds, &exceptfds, &timeout);
	return (ret > -1);
}

int SlirpEthernetConnection::PollGetSlirpRevents(int idx)
{
	int slirp_revents = 0;
	if(FD_ISSET(idx, &readfds))
	{
		char buf[8];
		int read = recv(idx, buf, sizeof(buf), MSG_PEEK);
		int error = (read == SOCKET_ERROR) ? WSAGetLastError() : 0;
		if(read > 0 || error == WSAEMSGSIZE)
		{
			slirp_revents |= SLIRP_POLL_IN;
		}
		else if(read == 0)
		{
			slirp_revents |= SLIRP_POLL_IN;
			slirp_revents |= SLIRP_POLL_HUP;
		}
		else
		{
			slirp_revents |= SLIRP_POLL_IN;
			slirp_revents |= SLIRP_POLL_ERR;
		}
	}
	if(FD_ISSET(idx, &writefds))
	{
		slirp_revents |= SLIRP_POLL_OUT;
	}
	if(FD_ISSET(idx, &exceptfds))
	{
		u_long atmark = 0;
		if(ioctlsocket(idx, SIOCATMARK, &atmark) == 0 && atmark == 1)
		{
			slirp_revents |= SLIRP_POLL_PRI;
		}
		else
		{
			slirp_revents |= SLIRP_POLL_ERR;
		}
	}
	return slirp_revents;
}

#endif

#endif
