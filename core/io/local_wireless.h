/*************************************************************************/
/*  local_wireless.h                                                     */
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

#ifndef LOCAL_WIRELESS_H
#define LOCAL_WIRELESS_H

#include "dvector.h"
#include "list.h"
#include "map.h"
#include "object.h"

// The network under LocalWireless: UDS between 3DS consoles, UDP on a PC (for testing games
// with two windows). Raw packets only; LocalWireless adds channels, ordering and signals.
class LocalWirelessTransport {
public:
	struct Room {
		String host_name;
		int players;
		int max_players;
		String info;
	};

	virtual Error host(int p_max_players, const String &p_room_info, int p_mode) = 0;
	virtual Error start_scan(int p_mode) = 0;
	virtual bool is_scanning() const = 0;
	virtual void get_rooms(Vector<Room> *r_rooms) const = 0;
	virtual Error join(int p_room_index) = 0;
	virtual void leave() = 0;

	virtual bool is_in_room() = 0; // false once the room is gone (host closed it, out of range)
	virtual bool is_host() const = 0;
	virtual int get_my_id() = 0; // 1 = host, 0 when not in a room
	virtual uint32_t get_player_mask() = 0; // bit (id - 1) per player in the room, this one included
	virtual String get_player_name(int p_id) = 0;

	virtual int get_max_packet_size() const = 0;
	virtual Error send_raw(const uint8_t *p_data, int p_size, int p_to) = 0; // p_to 0: everyone else
	virtual bool pull_raw(Vector<uint8_t> *r_data, int *r_sender) = 0; // false when there is none
	virtual void poll() {} // once per frame
	virtual void add_scan_address(const String &p_ip) {} // UDP: another computer to ask for rooms
	virtual uint32_t get_service_calls() const { return 0; } // calls to the system (3DS: UDS service), for get_stats()

	virtual ~LocalWirelessTransport() {}
};

// Script singleton "LocalWireless": local play between consoles (3DS: UDS, the wireless play
// of "download play" games, up to 16 players, no Internet or access point; PC: UDP, for testing).
//
// One console creates a room (host), the others look for rooms (start_scan) and join one.
// Players are numbered from 1 (the host). Rooms are only seen by the same game.
//
// Two ways to send, both to everyone or to one player:
//   send / send_var: fast, may be lost or arrive out of order (positions, many per second).
//   send_reliable / send_var_reliable: always arrives, in the order sent per sender (hits,
//     pickups, doors, map changes, rules). Resent until confirmed; costs a little more.
// Received packets of both kinds come out of get_packet()/get_var() in arrival order.
//
// Signals: player_joined(id), player_left(id), disconnected() (the room is gone, e.g. the host
// left). For testing, set_simulated_loss / set_simulated_latency degrade outgoing packets.
class LocalWireless : public Object {

	OBJ_TYPE(LocalWireless, Object);

public:
	enum Status {
		STATUS_DISCONNECTED,
		STATUS_HOST,
		STATUS_CLIENT,
	};
	enum {
		MAX_PLAYERS = 16,
		MAX_ROOM_INFO = 200, // bytes of application data in the room's announcement
		HEADER_SIZE = 3, // kind + sequence number, taken from each packet
		MAX_PACKET_SIZE = 1478 - 1 - HEADER_SIZE, // UDS_DATAFRAME_MAXSIZE minus the 3DS destination byte and the header
	};

private:
	static LocalWireless *singleton;
	LocalWirelessTransport *transport;
	Status status;

	struct Packet {
		DVector<uint8_t> data;
		int sender;
		bool reliable;
	};
	List<Packet> packets;
	int last_sender;
	bool last_reliable;

	// Reliable channel, per player (index id - 1).
	struct Pending {
		uint16_t seq;
		Vector<uint8_t> raw; // with header
		uint64_t sent_msec;
		int tries;
	};
	struct Peer {
		uint16_t send_seq;
		List<Pending> pending;
		uint16_t recv_expected;
		Map<uint16_t, DVector<uint8_t> > out_of_order;
		void reset() {
			send_seq = 0;
			pending.clear();
			recv_expected = 0;
			out_of_order.clear();
		}
	};
	Peer peers[MAX_PLAYERS];
	uint32_t known_mask; // players seen at the last poll (signals)

	// Testing.
	float sim_loss;
	int sim_latency;
	struct Delayed {
		Vector<uint8_t> raw;
		int to;
		uint64_t at_msec;
	};
	List<Delayed> delayed;

	// Diagnostics (get_stats); cleared by host() and join().
	struct Stats {
		uint32_t sent, send_failed, received, received_reliable, acks_received, resent, gave_up;
		void clear() { sent = send_failed = received = received_reliable = acks_received = resent = gave_up = 0; }
	} stats;

	// Time spent here per frame (transport and system calls included), for get_stats(): every
	// public entry point is timed once (nested calls are not counted twice).
	struct TimeScope;
	mutable int timing_depth;
	mutable uint64_t timing_start_usec, frame_usec;
	uint64_t win_start_msec, win_sum_usec, win_max_usec;
	uint32_t win_frames, win_calls_start;
	float net_ms, net_max_ms, calls_per_frame;
	void _close_frame_timing();

	Error _transmit(const Vector<uint8_t> &p_raw, int p_to); // to the transport, counted
	Error _send_raw(const Vector<uint8_t> &p_raw, int p_to);
	Error _send_reliable_to(const DVector<uint8_t> &p_data, int p_to);
	void _receive();
	void _deliver(const uint8_t *p_data, int p_size, int p_sender, bool p_reliable);
	void _set_disconnected(bool p_signal);
	void _reset_peers();
	static Error _encode(const Variant &p_var, DVector<uint8_t> *r_data);

protected:
	static void _bind_methods();

public:
	static LocalWireless *get_singleton() { return singleton; }

	// Creates a room for up to p_max_players (2-16). p_room_info (up to 200 bytes; e.g. the
	// host's rules) is shown to whoever scans; p_mode separates rooms of the same game.
	Error host(int p_max_players = 4, const String &p_room_info = "", int p_mode = 0);
	// Looks for rooms of this game in the background; poll is_scanning(), then get_rooms().
	Error start_scan(int p_mode = 0);
	bool is_scanning() const;
	// [{ "host_name", "players", "max_players", "info" }], in the order join() takes.
	Array get_rooms() const;
	Error join(int p_room_index);
	void leave(); // closes the room, if host

	Status get_status();
	bool is_host() const { return status == STATUS_HOST; }
	int get_my_id() const; // 1 = host, 0 when not in a room
	Array get_players() const; // [{ "id", "name" }], this console included

	int get_max_packet_size() const;
	// p_to: a player number, or 0 for everyone else.
	Error send(const DVector<uint8_t> &p_data, int p_to = 0);
	Error send_var(const Variant &p_var, int p_to = 0);
	Error send_reliable(const DVector<uint8_t> &p_data, int p_to = 0);
	Error send_var_reliable(const Variant &p_var, int p_to = 0);

	int get_packet_count();
	DVector<uint8_t> get_packet(); // removes the next packet; then get_packet_sender() etc.
	Variant get_var();
	int get_packet_sender() const { return last_sender; }
	bool was_packet_reliable() const { return last_reliable; }
	int get_pending_reliable_count() const; // sent but not confirmed yet
	// Counters since host()/join(), to find out where packets get lost: { "sent", "send_failed",
	// "received", "received_reliable", "acks_received", "resent", "gave_up", "pending" }; and the
	// cost per frame, averaged over the last second: "net_ms" (time spent in LocalWireless, system
	// calls included), "net_max_ms" (worst frame) and "calls_per_frame" (calls to the system).
	Dictionary get_stats() const;

	void set_simulated_loss(float p_ratio); // 0..1 of outgoing packets dropped (testing)
	float get_simulated_loss() const { return sim_loss; }
	void set_simulated_latency(int p_msec); // outgoing packets held this long (testing)
	int get_simulated_latency() const { return sim_latency; }

	// PC only (UDP): rooms are looked for on this computer, plus these addresses (LAN testing).
	void add_scan_address(const String &p_ip) { transport->add_scan_address(p_ip); }

	void poll(); // called by the engine every frame: resends, delivery, join/leave signals

	LocalWireless(LocalWirelessTransport *p_transport);
	~LocalWireless();
};

VARIANT_ENUM_CAST(LocalWireless::Status);

#endif // LOCAL_WIRELESS_H
