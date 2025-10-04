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

#include "config.h"

#if C_SLIRP

#include "ethernet_slirp.h"
#include "logging.h"
#include <time.h>
#include <algorithm>
#include <cassert>
#include <map>
#include "dosbox.h"

extern std::string niclist;

#if __APPLE__ && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
typedef enum {
    _CLOCK_REALTIME = 0,
#if !defined(CLOCK_REALTIME)
#define CLOCK_REALTIME _CLOCK_REALTIME
#endif
    _CLOCK_MONOTONIC = 6,
#if !defined(CLOCK_MONOTONIC)
#define CLOCK_MONOTONIC _CLOCK_MONOTONIC
#endif
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
    _CLOCK_MONOTONIC_RAW = 4,
#if !defined(CLOCK_MONOTONIC_RAW)
#define CLOCK_MONOTONIC_RAW _CLOCK_MONOTONIC_RAW
#endif
    _CLOCK_MONOTONIC_RAW_APPROX = 5,
#if !defined(CLOCK_MONOTONIC_RAW_APPROX)
#define CLOCK_MONOTONIC_RAW_APPROX _CLOCK_MONOTONIC_RAW_APPROX
#endif
    _CLOCK_UPTIME_RAW = 8,
#if !defined(CLOCK_UPTIME_RAW)
#define CLOCK_UPTIME_RAW _CLOCK_UPTIME_RAW
#endif
    _CLOCK_UPTIME_RAW_APPROX = 9,
#if !defined(CLOCK_UPTIME_RAW_APPROX)
#define CLOCK_UPTIME_RAW_APPROX _CLOCK_UPTIME_RAW_APPROX
#endif
#endif
    _CLOCK_PROCESS_CPUTIME_ID = 12,
#if !defined(CLOCK_PROCESS_CPUTIME_ID)
#define CLOCK_PROCESS_CPUTIME_ID _CLOCK_PROCESS_CPUTIME_ID
#endif
    _CLOCK_THREAD_CPUTIME_ID = 16
#if !defined(CLOCK_THREAD_CPUTIME_ID)
#define CLOCK_THREAD_CPUTIME_ID _CLOCK_THREAD_CPUTIME_ID
#endif
} clockid_t;

extern "C" {
/* clock_gettime() only available in macOS 10.12+ (Sierra) */
int clock_gettime(clockid_t clk_id, struct timespec *tp);
}
#endif

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
/* Use proper inet_pton implementation if we have it */
#include <ws2tcpip.h>
#endif /* _WIN32_WINNT */
#else /* !WIN32 */
#include <arpa/inet.h>
#include <sys/socket.h>
#endif /* WIN32 */

/* Begin boilerplate to map libslirp's C-based callbacks to our C++
 * object. This is done by passing our SlirpEthernetConnection as user data.
 */

// libslirp >= 4.8.0 defines slirp_ssize_t but will break compatibility with older versions
#ifdef _WIN32
SSIZE_T 
#else
ssize_t 
#endif
slirp_receive_packet(const void* buf, size_t len, void* opaque)
{
	SlirpEthernetConnection* conn = (SlirpEthernetConnection*)opaque;
	conn->ReceivePacket((const uint8_t*)buf, len);
	return len;
}

void slirp_guest_error(const char* msg, void* opaque)
{
	(void)opaque;
	LOG_MSG("SLIRP: Slirp error: %s", msg);
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

/* End boilerplate */

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

	LOG_MSG("SLIRP: Slirp version: %s", slirp_version_string());

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
        const auto section = static_cast<Section_prop *>(dosbox_config);
		bool is_udp = false;
		ClearPortForwards(is_udp, forwarded_tcp_ports);
		forwarded_tcp_ports = SetupPortForwards(is_udp, section->Get_string("tcp_port_forwards"));
		is_udp = true;
		ClearPortForwards(is_udp, forwarded_udp_ports);
		forwarded_udp_ports = SetupPortForwards(is_udp, section->Get_string("udp_port_forwards"));

		LOG_MSG("SLIRP: Successfully initialized");
        niclist = "You have currently enabled the slirp backend for NE2000 Ethernet emulation.\nTo show a list of network interfaces please enable the pcap backend instead.\nCheck [ne2000] and [ethernet, pcap] sections of the DOSBox-X configuration.";
		return true;
	}
	else
	{
		LOG_MSG("SLIRP: Failed to initialize");
		return false;
	}
}

void SlirpEthernetConnection::ClearPortForwards(const bool is_udp, std::map<int, int> &existing_port_forwards)
{
	const auto protocol = is_udp ? "UDP" : "TCP";
	const in_addr host_addr = {htonl(INADDR_ANY)};

	for (const auto &pair : existing_port_forwards) {
		const auto &host_port = pair.first;
		const auto &guest_port = pair.second;
		if (slirp_remove_hostfwd(slirp, is_udp, host_addr, host_port) >= 0)
			LOG_MSG("SLIRP: Removed old %s port %d:%d forward", protocol, host_port, guest_port);
		else
			LOG_MSG("SLIRP: Failed removing old %s port %d:%d forward", protocol, host_port, guest_port);
	}
	existing_port_forwards.clear();
}

std::map<int, int> SlirpEthernetConnection::SetupPortForwards(const bool is_udp, const std::string &port_forward_rules)
{
	std::map<int, int> forwarded_ports;
	const auto protocol = is_udp ? "UDP" : "TCP";
	constexpr in_addr bind_addr = {INADDR_ANY};

	// Split the rules first by spaces
	for (auto &forward_rule : split(port_forward_rules, ' ')) {
		if (forward_rule.empty())
			continue;

		// Split the rule into host:guest portions
		auto forward_rule_parts = split(forward_rule, ':');
		// if only one is provided, then the guest port is the same
		if (forward_rule_parts.size() == 1)
			forward_rule_parts.push_back(forward_rule_parts[0]);

		// We should now have two parts, host and guest
		if (forward_rule_parts.size() != 2) {
			LOG_MSG("SLIRP: Invalid %s port forward rule: %s", protocol, forward_rule.c_str());
			continue;
		}

		// We're not going to populate this rules port extents
		std::vector<int> ports = {};
		ports.reserve(4);

		// Process the host and guest portions separately
		for (const auto &port_range_part : forward_rule_parts) {
			auto port_range = split(port_range_part, '-');

			// If only one value is provided, then the start and end are the same
			if (port_range.size() == 1 && !port_range[0].empty())
				port_range.push_back(port_range[0]);

			// We should now have two parts, start and end
			if (port_range.size() != 2) {
				LOG_MSG("SLIRP: Invalid %s port range: %s", protocol, port_range_part.c_str());
				break;
			}

			// Now convert the ports to integers and push them into the 'ports' vector
			assert(port_range.size() == 2);
			for (const auto &port : port_range) {
				if (port.empty()) {
					LOG_MSG("SLIRP: Invalid %s port range: %s", protocol, port_range_part.c_str());
					break;
				}
				// Check if the port is in-range
				int port_num = -1;
				try {
					port_num = std::stoi(port);
				} catch (std::invalid_argument &e) {
					LOG_MSG("SLIRP: Invalid %s port: %s", protocol, port.c_str());
					break;
				} catch (std::out_of_range &e) {
					LOG_MSG("SLIRP: Invalid %s port: %s", protocol, port.c_str());
					break;
				}
				ports.push_back(port_num);
			}
		}

		// If both halves of the rule are valid, we will have four ports in the vector
		if (ports.size() != 4) {
			LOG_MSG("SLIRP: Invalid %s port forward rule: %s", protocol, forward_rule.c_str());
			continue;
		}
		assert(ports.size() == 4);

		// assign the ports to the host and guest portions
		const auto &host_port_start = ports[0];
		const auto &host_port_end = ports[1];
		const auto &guest_port_start = ports[2];
		const auto &guest_port_end = ports[3];

		// Check if the host port range is valid
		if (host_port_end < host_port_start || guest_port_end < guest_port_start) {
			LOG_MSG("SLIRP: Invalid %s port range: %s", protocol, forward_rule.c_str());
			continue;
		}

		// Sanity check that the maximum range doesn't go beyond the maximum port number
		constexpr auto min_valid_port = 1;
		constexpr auto max_valid_port = 65535;
		const auto range = std::max(host_port_end - host_port_start, guest_port_end - guest_port_start);
		if (host_port_start < min_valid_port || host_port_start + range > max_valid_port ||
		    guest_port_start < min_valid_port || guest_port_start + range > max_valid_port) {
			LOG_MSG("SLIRP: Invalid %s port range: %s", protocol, forward_rule.c_str());
			continue;
		}

		// Start adding the port forwards
		auto n = range + 1;
		auto host_port = host_port_start;
		auto guest_port = guest_port_start;
		LOG_MSG("SLIRP: Processing %s port forward rule: %s", protocol, forward_rule.c_str());
		while (n--) {
			// Add the port forward rule
			if (slirp_add_hostfwd(slirp, is_udp, bind_addr, host_port, bind_addr, guest_port) == 0) {
				LOG_MSG("SLIRP: Setup %s port %d:%d forward", protocol, host_port, guest_port);
				forwarded_ports[host_port] = guest_port;
			} else {
				LOG_MSG("SLIRP: Failed setting up %s port %d:%d forward", protocol, host_port, guest_port);
			}
			++host_port;
			++guest_port;
		} // end port addition loop
	} // end forward rule loop

	return forwarded_ports;
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
	/* expire_time is in milliseconds despite slirp wanting a nanosecond clock */
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
	/* BUG: Skip this entirely on Win32 as libslirp gives us invalid fds. */
	return;
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

/* Begin the bulk of the platform-specific code.
 * This mostly involves handling data structures and mapping
 * libslirp's view of our polling system to whatever we use
 * internally.
 * libslirp really wants poll() as it gives information about
 * out of band TCP data and connection hang-ups.
 * This is easy to do on Unix, but on other systems it needs
 * custom implementations that give this data. */

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
	/* Windows does not support poll(). It has WSAPoll() but this is
	 * reported as broken by libcurl and other projects, and Microsoft
	 * doesn't seem to want to fix this any time soon.
	 * glib provides g_poll() but that doesn't seem to work either.
	 * The solution I've made uses plain old select(), but checks for
	 * extra conditions and adds those to the flags we pass to libslirp.
	 * There's no one-to-one mapping of poll() flags on Windows, so here's
	 * my definition:
	 * SLIRP_POLL_HUP: The remote closed the socket gracefully.
	 * SLIRP_POLL_ERR: An exception happened or reading failed
	 * SLIRP_POLL_PRI: TCP Out-of-band data available
	 */
	int slirp_revents = 0;
	if(FD_ISSET(idx, &readfds))
	{
		/* This code is broken on ReactOS peeking a closed socket
		 * will cause the next recv() to fail instead of acting
		 * normally. See CORE-17425 on their JIRA */
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
