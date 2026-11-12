/*************************************************************************/
/*  network_3ds.cpp                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include "network_3ds.h"

#include "drivers/unix/ip_unix.h"
#include "drivers/unix/packet_peer_udp_posix.h"
#include "drivers/unix/stream_peer_tcp_posix.h"
#include "drivers/unix/tcp_server_posix.h"
#include "os/mutex.h"
#include "print_string.h"

#include <malloc.h>
#include <stdlib.h>

extern "C" {
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/services/soc.h>
#include <3ds/synchronization.h>
}

#define SOC_BUFFER_SIZE 0x100000

static u32 *soc_buffer = NULL;
static bool soc_ready = false;
static LightLock soc_lock; // init can come from the main thread and the hostname resolver thread
static bool _init_locked();

bool Network3DS::init() {

	if (soc_ready)
		return true;

	LightLock_Lock(&soc_lock);
	bool ok = _init_locked();
	LightLock_Unlock(&soc_lock);
	return ok;
}

static bool _init_locked() {

	if (soc_ready)
		return true;

	soc_buffer = (u32 *)memalign(0x1000, SOC_BUFFER_SIZE);
	if (!soc_buffer) {
		ERR_PRINT("Network: no memory for the SOC buffer (1 MB).");
		return false;
	}

	Result res = socInit(soc_buffer, SOC_BUFFER_SIZE);
	if (R_FAILED(res)) {
		ERR_PRINTS("Network: socInit failed (0x" + String::num_int64((uint32_t)res, 16) + ").");
		free(soc_buffer);
		soc_buffer = NULL;
		return false;
	}

	soc_ready = true;
	return true;
}

void Network3DS::finish() {

	if (!soc_ready)
		return;
	socExit();
	free(soc_buffer);
	soc_buffer = NULL;
	soc_ready = false;
}

// The posix classes, created through these so that SOC is up before the first socket.

class StreamPeerTCP3DS : public StreamPeerTCPPosix {
	static StreamPeerTCP *_create_3ds() {
		Network3DS::init();
		return memnew(StreamPeerTCPPosix);
	}

public:
	static void make_default() { StreamPeerTCP::_create = _create_3ds; }
};

class TCPServer3DS : public TCPServerPosix {
	static TCP_Server *_create_3ds() {
		Network3DS::init();
		return memnew(TCPServerPosix);
	}

public:
	static void make_default() { TCP_Server::_create = _create_3ds; }
};

class PacketPeerUDP3DS : public PacketPeerUDPPosix {
	static PacketPeerUDP *_create_3ds() {
		Network3DS::init();
		return memnew(PacketPeerUDPPosix);
	}

public:
	static void make_default() { PacketPeerUDP::_create = _create_3ds; }
};

class IP3DS : public IP_Unix {
	static IP *_create_3ds() { return memnew(IP3DS); }

public:
	virtual void _resolve_hostname(List<IP_Address> &r_addresses, const String &p_hostname, Type p_type = TYPE_ANY) const {
		if (Network3DS::init())
			IP_Unix::_resolve_hostname(r_addresses, p_hostname, TYPE_IPV4);
	}
	virtual void get_local_addresses(List<IP_Address> *r_addresses) const {
		if (Network3DS::init())
			IP_Unix::get_local_addresses(r_addresses);
	}
	static void make_default() { IP::_create = _create_3ds; }
};

void Network3DS::make_default() {

	LightLock_Init(&soc_lock);
	StreamPeerTCP3DS::make_default();
	TCPServer3DS::make_default();
	PacketPeerUDP3DS::make_default();
	IP3DS::make_default();
}
