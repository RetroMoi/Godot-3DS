/*************************************************************************/
/*  local_wireless_3ds.h                                                 */
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

#ifndef LOCAL_WIRELESS_3DS_H
#define LOCAL_WIRELESS_3DS_H

#include "io/local_wireless.h"
#include "os/mutex.h"
#include "os/thread.h"

// LocalWireless over UDS (the 3DS's local wireless). Rooms only seen by the same game: the
// network ID comes from the project name.
//
// Every frame goes out as a UDS broadcast, with the destination player in its first byte (0 =
// everyone); receivers drop frames addressed to someone else. UDS sends to a single node
// (udsSendTo with a NetworkNodeID) were never seen working on hardware: a co-op test built on
// them never got the host's answer, while broadcasts were validated between two consoles. For
// the 2-4 players of a local session the cost is nil (the radio broadcasts anyway).
class LocalWirelessUDS : public LocalWirelessTransport {

	enum {
		MAX_UDS_PACKET = 1478, // UDS_DATAFRAME_MAXSIZE
		MAX_ROOM_INFO = LocalWireless::MAX_ROOM_INFO,
		MAX_PLAYERS = LocalWireless::MAX_PLAYERS,
	};

	struct UDSRoom;
	struct RawPacket {
		Vector<uint8_t> data;
		int sender;
	};

	bool initialized;
	bool in_room;
	bool hosting;
	void *bind_context; // udsBindContext
	uint32_t comm_id;
	String passphrase;
	List<RawPacket> raw_packets;
	uint8_t *recv_buffer;
	uint8_t *send_buffer; // destination byte + data
	uint32_t service_calls; // IPC requests to the UDS service (get_stats)

	// Sleep (lid closed) turns the radio off and the system drops the room. On waking up the
	// session is over (is_in_room() turns false, so the game gets "disconnected"), and the UDS
	// service is restarted before the next room: without that, creating a room again could fail.
	static volatile bool slept; // set by the APT hook (another thread), handled in poll()
	bool room_lost; // the system dropped the room (sleep, or its status says so)
	bool needs_reset; // restart the UDS service (udsExit + udsInit) before the next room
	void _reset_service();

	// Scan, done in a thread (it takes a moment and would stall the game).
	Mutex *mutex;
	Thread *scan_thread;
	volatile bool scanning;
	int scan_mode;
	Vector<UDSRoom *> rooms;

	// Connection status cache: udsGetConnectionStatus is a call to the system (expensive at the
	// Old 3DS's 268 MHz), so it is queried at most once per frame, and only when the system says
	// it changed (with a refresh every 30 frames just in case). Names are fetched when the set of
	// players changes.
	bool status_valid;
	uint64_t status_frame;
	uint32_t net_status;
	int my_id;
	uint16_t node_bitmask;
	uint16_t names_bitmask;
	String node_names[MAX_PLAYERS];
	uint64_t pull_frame;

	static void _scan_thread_func(void *p_self);
	void _clear_rooms();
	Error _init();
	void _refresh_status();
	void _invalidate_status();
	void _pull_packets();

public:
	static void notify_sleep() { slept = true; } // from the APT hook: the console is going to sleep

	virtual void poll();
	virtual Error host(int p_max_players, const String &p_room_info, int p_mode);
	virtual Error start_scan(int p_mode);
	virtual bool is_scanning() const;
	virtual void get_rooms(Vector<Room> *r_rooms) const;
	virtual Error join(int p_room_index);
	virtual void leave();
	virtual bool is_in_room();
	virtual bool is_host() const { return in_room && hosting; }
	virtual int get_my_id();
	virtual uint32_t get_player_mask();
	virtual String get_player_name(int p_id);
	virtual int get_max_packet_size() const { return MAX_UDS_PACKET - 1; } // 1 byte: destination
	virtual Error send_raw(const uint8_t *p_data, int p_size, int p_to);
	virtual bool pull_raw(Vector<uint8_t> *r_data, int *r_sender);
	virtual uint32_t get_service_calls() const { return service_calls; }

	LocalWirelessUDS();
	~LocalWirelessUDS();
};

#endif
