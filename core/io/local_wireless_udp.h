/*************************************************************************/
/*  local_wireless_udp.h                                                 */
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

#ifndef LOCAL_WIRELESS_UDP_H
#define LOCAL_WIRELESS_UDP_H

#include "io/local_wireless.h"
#include "io/packet_peer_udp.h"

// LocalWireless over UDP, for PCs: run the game twice on one computer (or on two computers of
// the same network) to test local play without consoles. The host listens on a fixed port; a
// scan asks this computer (and the addresses given to add_scan_address) for rooms; the host
// forwards packets between the other players, as the 3DS's UDS does.
class LocalWirelessUDP : public LocalWirelessTransport {

public:
	enum {
		PORT = 27850,
		MAX_UDP_PACKET = 1478, // same as UDS
		MAX_PLAYERS = LocalWireless::MAX_PLAYERS,
	};

private:
	struct Address {
		IP_Address ip;
		int port;
		bool operator==(const Address &p_other) const { return port == p_other.port && ip == p_other.ip; }
	};
	struct Client {
		bool used;
		Address addr;
		String name;
		uint64_t last_heard;
	};
	struct ScanRoom {
		Room room;
		Address addr;
	};
	struct RawPacket {
		Vector<uint8_t> data;
		int sender;
	};

	Ref<PacketPeerUDP> udp;
	uint32_t comm_id;
	String my_name;
	Vector<String> scan_addresses;

	bool in_room;
	bool hosting;
	int my_id;
	int max_players;
	int mode;
	String room_info;
	uint32_t mask;
	String names[MAX_PLAYERS];
	Client clients[MAX_PLAYERS]; // host: by id - 1
	Address host_addr; // client
	uint64_t last_heard_host;
	uint64_t last_keepalive;

	bool scanning;
	int scan_mode;
	uint64_t scan_until;
	Vector<ScanRoom> rooms;

	List<RawPacket> raw_packets;

	void _init_ids();
	Error _open(int p_port);
	void _close();
	void _send_to(const Address &p_addr, const Vector<uint8_t> &p_data);
	void _send_roster();
	void _write_header(Vector<uint8_t> &r_data, int p_kind);
	void _process_incoming();
	void _handle(const uint8_t *p_data, int p_size, const Address &p_from);
	int _client_id(const Address &p_addr) const;

public:
	virtual void add_scan_address(const String &p_ip);

	virtual Error host(int p_max_players, const String &p_room_info, int p_mode);
	virtual Error start_scan(int p_mode);
	virtual bool is_scanning() const { return scanning; }
	virtual void get_rooms(Vector<Room> *r_rooms) const;
	virtual Error join(int p_room_index);
	virtual void leave();
	virtual bool is_in_room() { return in_room; }
	virtual bool is_host() const { return in_room && hosting; }
	virtual int get_my_id() { return in_room ? my_id : 0; }
	virtual uint32_t get_player_mask() { return in_room ? mask : 0; }
	virtual String get_player_name(int p_id);
	virtual int get_max_packet_size() const { return MAX_UDP_PACKET; }
	virtual Error send_raw(const uint8_t *p_data, int p_size, int p_to);
	virtual bool pull_raw(Vector<uint8_t> *r_data, int *r_sender);
	virtual void poll();

	LocalWirelessUDP();
	~LocalWirelessUDP();
};

#endif // LOCAL_WIRELESS_UDP_H
