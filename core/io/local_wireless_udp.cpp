/*************************************************************************/
/*  local_wireless_udp.cpp                                               */
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

#include "local_wireless_udp.h"

#include "globals.h"
#include "os/os.h"

// Every packet: 'L', 'W', kind, then by kind:
//   PROBE:   comm_id(4) mode(1)                             client -> host (scan)
//   BEACON:  comm_id(4) mode(1) players(1) max(1) name info  host -> client
//   JOIN:    comm_id(4) mode(1) name                        client -> host
//   WELCOME: id(1)                                          host -> client (roster follows)
//   REJECT:  -                                              host -> client (room full)
//   ROSTER:  mask(2) then per player: id(1) name            host -> clients (also a keepalive)
//   PING:    id(1)                                          client -> host (keepalive)
//   LEAVE:   id(1)                                          client -> host / host -> clients
//   DATA:    from(1) to(1) payload                          both ways; the host forwards
// Strings: length(1) + UTF-8 bytes.
enum {
	K_PROBE = 1,
	K_BEACON,
	K_JOIN,
	K_WELCOME,
	K_REJECT,
	K_ROSTER,
	K_PING,
	K_LEAVE,
	K_DATA,
};

#define KEEPALIVE_MSEC 400
#define TIMEOUT_MSEC 3000
#define SCAN_MSEC 600
#define JOIN_WAIT_MSEC 1500
#define MAX_QUEUED_PACKETS 512

static void _put_string(Vector<uint8_t> &r_data, const String &p_str) {

	CharString s = p_str.utf8();
	int len = MIN(s.length(), 255);
	r_data.push_back(len);
	for (int i = 0; i < len; i++)
		r_data.push_back((uint8_t)s[i]);
}

static bool _get_string(const uint8_t *p_data, int p_size, int &r_pos, String &r_str) {

	if (r_pos >= p_size)
		return false;
	int len = p_data[r_pos++];
	if (r_pos + len > p_size)
		return false;
	r_str.parse_utf8((const char *)p_data + r_pos, len);
	r_pos += len;
	return true;
}

static void _put_u32(Vector<uint8_t> &r_data, uint32_t p_v) {

	for (int i = 0; i < 4; i++)
		r_data.push_back((p_v >> (i * 8)) & 0xFF);
}

static uint32_t _get_u32(const uint8_t *p_data) {

	return p_data[0] | (p_data[1] << 8) | (p_data[2] << 16) | ((uint32_t)p_data[3] << 24);
}

LocalWirelessUDP::LocalWirelessUDP() {

	comm_id = 0;
	in_room = false;
	hosting = false;
	my_id = 0;
	max_players = 0;
	mode = 0;
	mask = 0;
	last_heard_host = 0;
	last_keepalive = 0;
	scanning = false;
	scan_mode = 0;
	scan_until = 0;
	host_addr.port = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
		clients[i].used = false;
}

LocalWirelessUDP::~LocalWirelessUDP() {

	leave();
	_close();
}

void LocalWirelessUDP::_init_ids() {

	if (comm_id)
		return;
	String name = Globals::get_singleton()->get("application/name");
	if (name == "")
		name = "Godot";
	comm_id = name.hash() | 1;
	// Two windows on one computer: tell them apart.
	String user = OS::get_singleton()->get_environment("USER");
	if (user == "")
		user = OS::get_singleton()->get_environment("USERNAME");
	if (user == "")
		user = "PC";
	my_name = user + " #" + itos(OS::get_singleton()->get_process_ID() % 1000);
}

Error LocalWirelessUDP::_open(int p_port) {

	_close();
	udp = PacketPeerUDP::create_ref();
	if (udp.is_null())
		return ERR_UNAVAILABLE;
	Error err = udp->listen(p_port);
	if (err != OK)
		udp = Ref<PacketPeerUDP>();
	return err;
}

void LocalWirelessUDP::_close() {

	if (udp.is_valid())
		udp->close();
	udp = Ref<PacketPeerUDP>();
}

void LocalWirelessUDP::add_scan_address(const String &p_ip) {

	if (scan_addresses.find(p_ip) == -1)
		scan_addresses.push_back(p_ip);
}

void LocalWirelessUDP::_write_header(Vector<uint8_t> &r_data, int p_kind) {

	r_data.clear();
	r_data.push_back('L');
	r_data.push_back('W');
	r_data.push_back(p_kind);
}

void LocalWirelessUDP::_send_to(const Address &p_addr, const Vector<uint8_t> &p_data) {

	if (udp.is_null())
		return;
	udp->set_send_address(p_addr.ip, p_addr.port);
	udp->put_packet(p_data.ptr(), p_data.size());
}

int LocalWirelessUDP::_client_id(const Address &p_addr) const {

	for (int i = 1; i < MAX_PLAYERS; i++) {
		if (clients[i].used && clients[i].addr == p_addr)
			return i + 1;
	}
	return 0;
}

Error LocalWirelessUDP::host(int p_max_players, const String &p_room_info, int p_mode) {

	ERR_FAIL_COND_V(in_room, ERR_ALREADY_IN_USE);
	_init_ids();
	Error err = _open(PORT);
	if (err != OK) {
		ERR_PRINTS("LocalWireless (PC): could not open UDP port " + itos(PORT) + " to host (is another window already hosting?).");
		return ERR_CANT_CREATE;
	}
	in_room = true;
	hosting = true;
	my_id = 1;
	max_players = p_max_players;
	mode = p_mode;
	room_info = p_room_info;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		clients[i].used = false;
		names[i] = String();
	}
	clients[0].used = true; // the host itself
	names[0] = my_name;
	mask = 1;
	raw_packets.clear();
	return OK;
}

Error LocalWirelessUDP::start_scan(int p_mode) {

	ERR_FAIL_COND_V(in_room, ERR_ALREADY_IN_USE);
	_init_ids();
	if (udp.is_null() && _open(0) != OK)
		return ERR_CANT_CREATE;
	scan_mode = p_mode;
	rooms.clear();
	Vector<uint8_t> probe;
	_write_header(probe, K_PROBE);
	_put_u32(probe, comm_id);
	probe.push_back(p_mode);
	Address a;
	a.port = PORT;
	a.ip = IP_Address("127.0.0.1");
	_send_to(a, probe);
	for (int i = 0; i < scan_addresses.size(); i++) {
		a.ip = IP_Address(scan_addresses[i]);
		_send_to(a, probe);
	}
	scanning = true;
	scan_until = OS::get_singleton()->get_ticks_msec() + SCAN_MSEC;
	return OK;
}

void LocalWirelessUDP::get_rooms(Vector<Room> *r_rooms) const {

	for (int i = 0; i < rooms.size(); i++)
		r_rooms->push_back(rooms[i].room);
}

Error LocalWirelessUDP::join(int p_room_index) {

	ERR_FAIL_COND_V(in_room, ERR_ALREADY_IN_USE);
	ERR_FAIL_INDEX_V(p_room_index, rooms.size(), ERR_INVALID_PARAMETER);
	if (udp.is_null() && _open(0) != OK)
		return ERR_CANT_CREATE;

	host_addr = rooms[p_room_index].addr;
	Vector<uint8_t> join;
	_write_header(join, K_JOIN);
	_put_u32(join, comm_id);
	join.push_back(scan_mode);
	_put_string(join, my_name);

	// Wait (briefly, like UDS's connect) for the host's answer.
	hosting = false;
	my_id = 0;
	uint64_t until = OS::get_singleton()->get_ticks_msec() + JOIN_WAIT_MSEC;
	uint64_t next_send = 0;
	while (OS::get_singleton()->get_ticks_msec() < until) {
		if (OS::get_singleton()->get_ticks_msec() >= next_send) {
			_send_to(host_addr, join);
			next_send = OS::get_singleton()->get_ticks_msec() + 200;
		}
		_process_incoming();
		if (my_id > 0)
			break;
		if (my_id < 0)
			break; // rejected
		OS::get_singleton()->delay_usec(5000);
	}
	if (my_id <= 0) {
		ERR_PRINTS(my_id < 0 ? "LocalWireless (PC): the room is full." : "LocalWireless (PC): the host did not answer.");
		my_id = 0;
		return ERR_CANT_CONNECT;
	}
	in_room = true;
	last_heard_host = OS::get_singleton()->get_ticks_msec();
	raw_packets.clear();
	return OK;
}

void LocalWirelessUDP::leave() {

	if (!in_room)
		return;
	Vector<uint8_t> bye;
	_write_header(bye, K_LEAVE);
	bye.push_back(my_id);
	if (hosting) {
		for (int i = 1; i < MAX_PLAYERS; i++) {
			if (clients[i].used)
				_send_to(clients[i].addr, bye);
		}
	} else {
		_send_to(host_addr, bye);
	}
	in_room = false;
	hosting = false;
	my_id = 0;
	mask = 0;
	raw_packets.clear();
	for (int i = 0; i < MAX_PLAYERS; i++)
		clients[i].used = false;
	_close();
}

String LocalWirelessUDP::get_player_name(int p_id) {

	if (!in_room || p_id < 1 || p_id > MAX_PLAYERS)
		return String();
	return names[p_id - 1];
}

void LocalWirelessUDP::_send_roster() {

	Vector<uint8_t> roster;
	_write_header(roster, K_ROSTER);
	roster.push_back(mask & 0xFF);
	roster.push_back((mask >> 8) & 0xFF);
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (mask & (1 << i)) {
			roster.push_back(i + 1);
			_put_string(roster, names[i]);
		}
	}
	for (int i = 1; i < MAX_PLAYERS; i++) {
		if (clients[i].used)
			_send_to(clients[i].addr, roster);
	}
}

Error LocalWirelessUDP::send_raw(const uint8_t *p_data, int p_size, int p_to) {

	ERR_FAIL_COND_V(!in_room, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_size <= 0 || p_size > MAX_UDP_PACKET, ERR_INVALID_PARAMETER);
	Vector<uint8_t> data;
	_write_header(data, K_DATA);
	data.push_back(my_id);
	data.push_back(p_to);
	int base = data.size();
	data.resize(base + p_size);
	copymem(data.ptr() + base, p_data, p_size);
	if (!hosting) {
		_send_to(host_addr, data); // the host forwards it
		return OK;
	}
	for (int i = 1; i < MAX_PLAYERS; i++) {
		if (clients[i].used && (p_to == 0 || p_to == i + 1))
			_send_to(clients[i].addr, data);
	}
	return OK;
}

bool LocalWirelessUDP::pull_raw(Vector<uint8_t> *r_data, int *r_sender) {

	if (raw_packets.empty())
		_process_incoming();
	if (raw_packets.empty())
		return false;
	*r_data = raw_packets.front()->get().data;
	*r_sender = raw_packets.front()->get().sender;
	raw_packets.pop_front();
	return true;
}

void LocalWirelessUDP::_process_incoming() {

	if (udp.is_null())
		return;
	while (udp->get_available_packet_count() > 0) {
		const uint8_t *data = NULL;
		int size = 0;
		if (udp->get_packet(&data, size) != OK)
			break;
		Address from;
		from.ip = udp->get_packet_address();
		from.port = udp->get_packet_port();
		if (size >= 3 && data[0] == 'L' && data[1] == 'W')
			_handle(data, size, from);
	}
}

void LocalWirelessUDP::_handle(const uint8_t *p_data, int p_size, const Address &p_from) {

	int kind = p_data[2];
	uint64_t now = OS::get_singleton()->get_ticks_msec();
	int pos = 3;

	if (hosting && in_room) {
		switch (kind) {
			case K_PROBE: {
				if (p_size < 8 || _get_u32(p_data + 3) != comm_id || p_data[7] != mode)
					return;
				Vector<uint8_t> beacon;
				_write_header(beacon, K_BEACON);
				_put_u32(beacon, comm_id);
				beacon.push_back(mode);
				int players = 0;
				for (int i = 0; i < MAX_PLAYERS; i++)
					players += (mask >> i) & 1;
				beacon.push_back(players);
				beacon.push_back(max_players);
				_put_string(beacon, my_name);
				_put_string(beacon, room_info);
				_send_to(p_from, beacon);
			} break;
			case K_JOIN: {
				if (p_size < 8 || _get_u32(p_data + 3) != comm_id || p_data[7] != mode)
					return;
				pos = 8;
				String name;
				_get_string(p_data, p_size, pos, name);
				int id = _client_id(p_from);
				if (!id) {
					for (int i = 1; i < max_players && !id; i++) {
						if (!clients[i].used)
							id = i + 1;
					}
				}
				if (!id) {
					Vector<uint8_t> reject;
					_write_header(reject, K_REJECT);
					_send_to(p_from, reject);
					return;
				}
				Client &c = clients[id - 1];
				c.used = true;
				c.addr = p_from;
				c.name = name;
				c.last_heard = now;
				names[id - 1] = name;
				mask |= 1 << (id - 1);
				Vector<uint8_t> welcome;
				_write_header(welcome, K_WELCOME);
				welcome.push_back(id);
				_send_to(p_from, welcome);
				_send_roster();
			} break;
			case K_PING: {
				int id = _client_id(p_from);
				if (id)
					clients[id - 1].last_heard = now;
			} break;
			case K_LEAVE: {
				int id = _client_id(p_from);
				if (id) {
					clients[id - 1].used = false;
					mask &= ~(1 << (id - 1));
					names[id - 1] = String();
					_send_roster();
				}
			} break;
			case K_DATA: {
				int id = _client_id(p_from);
				if (!id || p_size < 6)
					return;
				clients[id - 1].last_heard = now;
				int to = p_data[4];
				const uint8_t *payload = p_data + 5;
				int psize = p_size - 5;
				if (to == 0 || to == 1) {
					RawPacket p;
					p.sender = id;
					p.data.resize(psize);
					copymem(p.data.ptr(), payload, psize);
					raw_packets.push_back(p);
					if (raw_packets.size() > MAX_QUEUED_PACKETS)
						raw_packets.pop_front();
				}
				if (to != 1) {
					// Forward, keeping the sender (the packet already says who sent it).
					Vector<uint8_t> fwd;
					fwd.resize(p_size);
					copymem(fwd.ptr(), p_data, p_size);
					fwd[3] = id;
					for (int i = 1; i < MAX_PLAYERS; i++) {
						if (clients[i].used && i + 1 != id && (to == 0 || to == i + 1))
							_send_to(clients[i].addr, fwd);
					}
				}
			} break;
		}
		return;
	}

	// Scanning or client.
	switch (kind) {
		case K_BEACON: {
			if (!scanning || p_size < 10 || _get_u32(p_data + 3) != comm_id || p_data[7] != scan_mode)
				return;
			ScanRoom r;
			r.addr = p_from;
			r.room.players = p_data[8];
			r.room.max_players = p_data[9];
			pos = 10;
			_get_string(p_data, p_size, pos, r.room.host_name);
			_get_string(p_data, p_size, pos, r.room.info);
			for (int i = 0; i < rooms.size(); i++) {
				if (rooms[i].addr == p_from)
					return; // answered twice (two addresses)
			}
			rooms.push_back(r);
		} break;
		case K_WELCOME: {
			if (p_size >= 4 && p_from == host_addr)
				my_id = p_data[3];
		} break;
		case K_REJECT: {
			if (p_from == host_addr)
				my_id = -1;
		} break;
		case K_ROSTER: {
			if (!(p_from == host_addr) || p_size < 5)
				return;
			last_heard_host = now;
			mask = p_data[3] | (p_data[4] << 8);
			pos = 5;
			for (int i = 0; i < MAX_PLAYERS; i++)
				names[i] = String();
			while (pos < p_size) {
				int id = p_data[pos++];
				String name;
				if (!_get_string(p_data, p_size, pos, name))
					break;
				if (id >= 1 && id <= MAX_PLAYERS)
					names[id - 1] = name;
			}
		} break;
		case K_LEAVE: {
			if (p_from == host_addr && in_room)
				last_heard_host = 0; // the host closed the room: gone at the next poll
		} break;
		case K_DATA: {
			if (!in_room || !(p_from == host_addr) || p_size < 6)
				return;
			last_heard_host = now;
			RawPacket p;
			p.sender = p_data[3];
			p.data.resize(p_size - 5);
			copymem(p.data.ptr(), p_data + 5, p_size - 5);
			raw_packets.push_back(p);
			if (raw_packets.size() > MAX_QUEUED_PACKETS)
				raw_packets.pop_front();
		} break;
	}
}

void LocalWirelessUDP::poll() {

	_process_incoming();
	uint64_t now = OS::get_singleton()->get_ticks_msec();

	if (scanning && now >= scan_until) {
		scanning = false;
		if (!in_room)
			_close(); // reopened by join
	}

	if (!in_room)
		return;

	if (hosting) {
		bool changed = false;
		for (int i = 1; i < MAX_PLAYERS; i++) {
			if (clients[i].used && now - clients[i].last_heard > TIMEOUT_MSEC) {
				clients[i].used = false;
				mask &= ~(1 << i);
				names[i] = String();
				changed = true;
			}
		}
		if (changed || now - last_keepalive >= KEEPALIVE_MSEC) {
			_send_roster();
			last_keepalive = now;
		}
	} else {
		if (now - last_heard_host > TIMEOUT_MSEC) {
			// The host is gone: LocalWireless sees is_in_room() false and leaves.
			in_room = false;
			mask = 0;
			return;
		}
		if (now - last_keepalive >= KEEPALIVE_MSEC) {
			Vector<uint8_t> ping;
			_write_header(ping, K_PING);
			ping.push_back(my_id);
			_send_to(host_addr, ping);
			last_keepalive = now;
		}
	}
}
