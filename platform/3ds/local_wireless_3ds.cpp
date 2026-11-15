/*************************************************************************/
/*  local_wireless_3ds.cpp                                               */
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

#include "local_wireless_3ds.h"

#include "globals.h"
#include "os/os.h"
#include "print_string.h"

#include <stdlib.h>
#include <string.h>

extern "C" {
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/services/uds.h>
}

#define DATA_CHANNEL 1
#define SHAREDMEM_SIZE 0x3000
#define MAX_QUEUED_PACKETS 256
#define SCAN_BUFFER_SIZE 0x4000
#define SCAN_ATTEMPTS 3
#define STATUS_REFRESH_FRAMES 30 // re-read the status at least this often, even without an event
#define PULL_REFRESH_FRAMES 8 // pull packets at least this often, even without a data event

// udsConnectionStatus::status values (as in Citra/Azahar's NWM-UDS implementation).
#define NETSTATUS_NOT_CONNECTED 3

struct LocalWirelessUDS::UDSRoom {
	udsNetworkStruct network;
	String host_name;
	String info;
};

static String _uds_error(Result p_res) {
	return String::num_int64((uint32_t)p_res, 16);
}

static String _node_username(const udsNodeInfo *p_node) {
	char name[64];
	memset(name, 0, sizeof(name));
	if (!udsCheckNodeInfoInitialized(p_node) || R_FAILED(udsGetNodeInfoUsername(p_node, name)))
		return String();
	String s;
	s.parse_utf8(name);
	return s;
}

Error LocalWirelessUDS::_init() {

	if (initialized)
		return OK;

	Result res = udsInit(SHAREDMEM_SIZE, NULL); // NULL: the console's user name
	if (R_FAILED(res)) {
		ERR_PRINTS("LocalWireless: udsInit failed (0x" + _uds_error(res) + ").");
		return ERR_CANT_CONNECT;
	}

	// Only this game sees its rooms: network ID and passphrase come from the project name.
	String name = Globals::get_singleton()->get("application/name");
	if (name == "")
		name = "Godot 3DS";
	comm_id = name.hash();
	passphrase = "godot3ds/" + name;

	initialized = true;
	return OK;
}

Error LocalWirelessUDS::host(int p_max_players, const String &p_room_info, int p_mode) {

	ERR_FAIL_COND_V(in_room, ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(scanning, ERR_BUSY);
	Error err = _init();
	if (err != OK)
		return err;

	udsNetworkStruct network;
	udsGenerateDefaultNetworkStruct(&network, comm_id, (u8)p_mode, (u8)CLAMP(p_max_players, 2, (int)MAX_PLAYERS));

	CharString pass = passphrase.utf8();
	udsBindContext *ctx = (udsBindContext *)bind_context;
	Result res = udsCreateNetwork(&network, pass.get_data(), pass.length() + 1, ctx, DATA_CHANNEL, UDS_DEFAULT_RECVBUFSIZE);
	if (R_FAILED(res)) {
		ERR_PRINTS("LocalWireless: could not create the room (0x" + _uds_error(res) + ").");
		return ERR_CANT_CREATE;
	}

	CharString info = p_room_info.utf8();
	int info_size = MIN(info.length(), (int)MAX_ROOM_INFO);
	if (info_size > 0) {
		res = udsSetApplicationData(info.get_data(), info_size);
		if (R_FAILED(res))
			ERR_PRINTS("LocalWireless: could not set the room info (0x" + _uds_error(res) + ").");
	}

	raw_packets.clear();
	in_room = true;
	hosting = true;
	_invalidate_status();
	return OK;
}

void LocalWirelessUDS::_scan_thread_func(void *p_self) {

	LocalWirelessUDS *self = (LocalWirelessUDS *)p_self;
	Vector<UDSRoom *> found;

	void *buffer = malloc(SCAN_BUFFER_SIZE);
	for (int attempt = 0; buffer && attempt < SCAN_ATTEMPTS && found.empty(); attempt++) {

		udsNetworkScanInfo *networks = NULL;
		size_t total = 0;
		memset(buffer, 0, SCAN_BUFFER_SIZE);
		Result res = udsScanBeacons(buffer, SCAN_BUFFER_SIZE, &networks, &total, self->comm_id, (u8)self->scan_mode, NULL, false);
		if (R_FAILED(res)) {
			ERR_PRINTS("LocalWireless: scan failed (0x" + _uds_error(res) + ").");
			break;
		}

		for (size_t i = 0; i < total; i++) {
			UDSRoom *room = memnew(UDSRoom);
			room->network = networks[i].network;
			room->host_name = _node_username(&networks[i].nodes[0]);

			char info[MAX_ROOM_INFO + 1];
			size_t info_size = 0;
			memset(info, 0, sizeof(info));
			if (R_SUCCEEDED(udsGetNetworkStructApplicationData(&networks[i].network, info, MAX_ROOM_INFO, &info_size)) && info_size > 0)
				room->info.parse_utf8(info, MIN((int)info_size, (int)MAX_ROOM_INFO));

			found.push_back(room);
		}
		if (networks)
			free(networks);
	}
	if (buffer)
		free(buffer);

	self->mutex->lock();
	self->_clear_rooms();
	self->rooms = found;
	self->mutex->unlock();
	self->scanning = false;
}

void LocalWirelessUDS::_clear_rooms() {

	for (int i = 0; i < rooms.size(); i++)
		memdelete(rooms[i]);
	rooms.clear();
}

Error LocalWirelessUDS::start_scan(int p_mode) {

	ERR_FAIL_COND_V(scanning, ERR_BUSY);
	ERR_FAIL_COND_V(in_room, ERR_ALREADY_IN_USE);
	Error err = _init();
	if (err != OK)
		return err;

	if (scan_thread) {
		Thread::wait_to_finish(scan_thread);
		memdelete(scan_thread);
		scan_thread = NULL;
	}

	scan_mode = p_mode;
	scanning = true;
	scan_thread = Thread::create(_scan_thread_func, this);
	if (!scan_thread) {
		scanning = false;
		return ERR_CANT_CREATE;
	}
	return OK;
}

bool LocalWirelessUDS::is_scanning() const {

	return scanning;
}

void LocalWirelessUDS::get_rooms(Vector<Room> *r_rooms) const {

	mutex->lock();
	for (int i = 0; i < rooms.size(); i++) {
		Room r;
		r.host_name = rooms[i]->host_name;
		r.players = rooms[i]->network.total_nodes;
		r.max_players = rooms[i]->network.max_nodes;
		r.info = rooms[i]->info;
		r_rooms->push_back(r);
	}
	mutex->unlock();
}

Error LocalWirelessUDS::join(int p_room_index) {

	ERR_FAIL_COND_V(in_room, ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(scanning, ERR_BUSY);

	mutex->lock();
	if (p_room_index < 0 || p_room_index >= rooms.size()) {
		mutex->unlock();
		ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}
	udsNetworkStruct network = rooms[p_room_index]->network;
	mutex->unlock();

	CharString pass = passphrase.utf8();
	udsBindContext *ctx = (udsBindContext *)bind_context;
	Result res = 0;
	for (int attempt = 0; attempt < 3; attempt++) {
		res = udsConnectNetwork(&network, pass.get_data(), pass.length() + 1, ctx, UDS_BROADCAST_NETWORKNODEID, UDSCONTYPE_Client, DATA_CHANNEL, UDS_DEFAULT_RECVBUFSIZE);
		if (R_SUCCEEDED(res))
			break;
	}
	if (R_FAILED(res)) {
		ERR_PRINTS("LocalWireless: could not join the room (0x" + _uds_error(res) + ").");
		return ERR_CANT_CONNECT;
	}

	raw_packets.clear();
	in_room = true;
	hosting = false;
	_invalidate_status();
	return OK;
}

void LocalWirelessUDS::leave() {

	if (!in_room)
		return;

	if (hosting)
		udsDestroyNetwork();
	else
		udsDisconnectNetwork();
	udsUnbind((udsBindContext *)bind_context);

	in_room = false;
	hosting = false;
	raw_packets.clear();
	_invalidate_status();
}

void LocalWirelessUDS::_invalidate_status() {

	status_valid = false;
	names_bitmask = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
		node_names[i] = String();
}

void LocalWirelessUDS::_refresh_status() {

	if (!in_room)
		return;

	uint64_t frame = OS::get_singleton()->get_frames_drawn();
	if (status_valid && status_frame == frame)
		return; // already up to date this frame

	// Cheap check (no call to the UDS service): did the status change?
	bool changed = udsWaitConnectionStatusEvent(false, false);
	if (status_valid && !changed && frame - status_frame < STATUS_REFRESH_FRAMES)
		return;

	udsConnectionStatus constatus;
	if (R_FAILED(udsGetConnectionStatus(&constatus)))
		return;
	net_status = constatus.status;
	my_id = constatus.cur_NetworkNodeID;
	node_bitmask = constatus.node_bitmask;
	status_frame = frame;
	status_valid = true;
}

bool LocalWirelessUDS::is_in_room() {

	if (!in_room)
		return false;
	if (hosting)
		return true;
	// A client: the host closed the room or went out of range.
	_refresh_status();
	return !(status_valid && net_status == NETSTATUS_NOT_CONNECTED);
}

int LocalWirelessUDS::get_my_id() {

	if (!in_room)
		return 0;
	_refresh_status();
	return status_valid ? my_id : 0;
}

uint32_t LocalWirelessUDS::get_player_mask() {

	if (!in_room)
		return 0;
	_refresh_status();
	return status_valid ? node_bitmask : 0;
}

String LocalWirelessUDS::get_player_name(int p_id) {

	if (!in_room || p_id < 1 || p_id > MAX_PLAYERS)
		return String();
	_refresh_status();
	int i = p_id - 1;
	if (names_bitmask != node_bitmask) {
		// Players joined or left: names are fetched again when asked for.
		for (int j = 0; j < MAX_PLAYERS; j++) {
			if (!(names_bitmask & (1 << j)))
				node_names[j] = String();
		}
		names_bitmask &= node_bitmask;
	}
	if (!(names_bitmask & (1 << i)) && (node_bitmask & (1 << i))) {
		udsNodeInfo node;
		node_names[i] = R_SUCCEEDED(udsGetNodeInformation(p_id, &node)) ? _node_username(&node) : String();
		names_bitmask |= (1 << i);
	}
	return node_names[i];
}

Error LocalWirelessUDS::send_raw(const uint8_t *p_data, int p_size, int p_to) {

	ERR_FAIL_COND_V(!in_room, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_size <= 0 || p_size > MAX_UDS_PACKET - 1, ERR_INVALID_PARAMETER);

	// Always a broadcast; the destination goes in the first byte (see the header).
	send_buffer[0] = (p_to <= 0 || p_to > MAX_PLAYERS) ? 0 : (uint8_t)p_to;
	memcpy(send_buffer + 1, p_data, p_size);
	Result res = udsSendTo(UDS_BROADCAST_NETWORKNODEID, DATA_CHANNEL, UDS_SENDFLAG_Default, send_buffer, p_size + 1);
	if (R_FAILED(res)) {
		if (UDS_CHECK_SENDTO_FATALERROR(res)) {
			ERR_PRINTS("LocalWireless: send failed (0x" + _uds_error(res) + ").");
			return ERR_CONNECTION_ERROR;
		}
		return ERR_BUSY; // the system skipped this frame (not fatal); counted in get_stats()
	}
	return OK;
}

void LocalWirelessUDS::_pull_packets() {

	if (!in_room || !recv_buffer)
		return;

	udsBindContext *ctx = (udsBindContext *)bind_context;

	// At most once per frame, and only when the system signals new data (cheap check, no call
	// to the UDS service); every few frames regardless, in case an event was missed.
	uint64_t frame = OS::get_singleton()->get_frames_drawn();
	if (frame == pull_frame)
		return;
	bool data = udsWaitDataAvailable(ctx, false, false);
	if (!data && frame - pull_frame < PULL_REFRESH_FRAMES)
		return;
	pull_frame = frame;

	// Frames addressed to another player are dropped. Until this console knows its own ID
	// (just after joining), only frames for everyone are taken; reliable ones are resent anyway.
	_refresh_status();
	int me = status_valid ? my_id : 0;

	while (true) {
		size_t size = 0;
		u16 sender = 0;
		Result res = udsPullPacket(ctx, recv_buffer, MAX_UDS_PACKET, &size, &sender);
		if (R_FAILED(res) || size == 0)
			break;
		if (size < 2)
			continue;
		int to = recv_buffer[0];
		if (to != 0 && to != me)
			continue; // for someone else

		RawPacket p;
		p.sender = sender;
		p.data.resize(size - 1);
		memcpy(p.data.ptr(), recv_buffer + 1, size - 1);
		raw_packets.push_back(p);
		if (raw_packets.size() > MAX_QUEUED_PACKETS)
			raw_packets.pop_front(); // nobody is reading: keep the newest
	}
}

bool LocalWirelessUDS::pull_raw(Vector<uint8_t> *r_data, int *r_sender) {

	if (raw_packets.empty())
		_pull_packets();
	if (raw_packets.empty())
		return false;
	*r_data = raw_packets.front()->get().data;
	*r_sender = raw_packets.front()->get().sender;
	raw_packets.pop_front();
	return true;
}

LocalWirelessUDS::LocalWirelessUDS() {

	initialized = false;
	in_room = false;
	hosting = false;
	comm_id = 0;
	scanning = false;
	scan_mode = 0;
	scan_thread = NULL;
	status_valid = false;
	status_frame = 0;
	net_status = 0;
	my_id = 0;
	node_bitmask = 0;
	names_bitmask = 0;
	pull_frame = (uint64_t)-1;
	mutex = Mutex::create();
	bind_context = memalloc(sizeof(udsBindContext));
	memset(bind_context, 0, sizeof(udsBindContext));
	recv_buffer = (uint8_t *)memalloc(MAX_UDS_PACKET);
	send_buffer = (uint8_t *)memalloc(MAX_UDS_PACKET);
}

LocalWirelessUDS::~LocalWirelessUDS() {

	if (scan_thread) {
		Thread::wait_to_finish(scan_thread);
		memdelete(scan_thread);
	}
	leave();
	if (initialized)
		udsExit();

	_clear_rooms();
	memfree(bind_context);
	memfree(recv_buffer);
	memfree(send_buffer);
	memdelete(mutex);
}
