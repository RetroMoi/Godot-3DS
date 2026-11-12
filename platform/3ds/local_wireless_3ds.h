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

#include "object.h"
#include "os/mutex.h"
#include "os/thread.h"
#include "list.h"
#include "dvector.h"

// Script singleton "LocalWireless": local wireless play between 3DS consoles (UDS, the same
// "download play"-style network games use), up to 16 players, no Internet or access point.
//
// One console creates a room (host), the others look for rooms (start_scan) and join one.
// Packets of up to 1478 bytes go to everyone or to one player; players are numbered from 1
// (the host). Rooms are only seen by the same game: the network ID comes from the project name.
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
		MAX_PACKET_SIZE = 1478, // UDS_DATAFRAME_MAXSIZE
		MAX_ROOM_INFO = 200, // bytes of application data in the beacon
	};

private:
	static LocalWireless *singleton;

	struct Packet {
		DVector<uint8_t> data;
		int sender;
	};

	struct Room;

	bool initialized;
	Status status;
	void *bind_context; // udsBindContext
	uint32_t comm_id;
	String passphrase;
	List<Packet> packets;
	int last_sender;
	uint8_t *recv_buffer;

	// Scan, done in a thread (it takes a moment and would stall the game).
	Mutex *mutex;
	Thread *scan_thread;
	volatile bool scanning;
	int scan_mode;
	Vector<Room *> rooms;

	static void _scan_thread_func(void *p_self);
	void _clear_rooms();
	Error _init();
	void _pull_packets();
	void _check_connection();

protected:
	static void _bind_methods();

public:
	static LocalWireless *get_singleton() { return singleton; }

	// Creates a room for up to p_max_players (2-16). p_room_info (up to 200 bytes) is shown to
	// whoever scans; p_mode separates rooms of the same game (e.g. game modes or versions).
	Error host(int p_max_players = 4, const String &p_room_info = "", int p_mode = 0);

	// Looks for rooms of this game in the background; poll is_scanning(), then get_rooms().
	Error start_scan(int p_mode = 0);
	bool is_scanning() const;
	// [{ "host_name", "players", "max_players", "info" }], in the order join() takes.
	Array get_rooms() const;
	Error join(int p_room_index);

	// Leaves the room (closes it, if host).
	void leave();

	Status get_status();
	bool is_host() const { return status == STATUS_HOST; }
	// This console's player number (1 = host), 0 when not in a room.
	int get_my_id() const;
	// [{ "id", "name" }] of everyone in the room, this console included.
	Array get_players() const;

	// p_to: player number, or 0 for everyone else.
	Error send(const DVector<uint8_t> &p_data, int p_to = 0);
	Error send_var(const Variant &p_var, int p_to = 0);

	int get_packet_count();
	// Removes and returns the next packet; get_packet_sender() then says who sent it.
	DVector<uint8_t> get_packet();
	Variant get_var();
	int get_packet_sender() const { return last_sender; }

	LocalWireless();
	~LocalWireless();
};

VARIANT_ENUM_CAST(LocalWireless::Status);

#endif
