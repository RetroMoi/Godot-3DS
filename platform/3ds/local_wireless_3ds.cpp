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
#include "io/marshalls.h"
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

struct LocalWireless::Room {
	udsNetworkStruct network;
	String host_name;
	String info;
};

LocalWireless *LocalWireless::singleton = NULL;

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

Error LocalWireless::_init() {

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

Error LocalWireless::host(int p_max_players, const String &p_room_info, int p_mode) {

	ERR_FAIL_COND_V(get_status() != STATUS_DISCONNECTED, ERR_ALREADY_IN_USE);
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

	packets.clear();
	status = STATUS_HOST;
	_invalidate_status();
	return OK;
}

void LocalWireless::_scan_thread_func(void *p_self) {

	LocalWireless *self = (LocalWireless *)p_self;
	Vector<Room *> found;

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
			Room *room = memnew(Room);
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

void LocalWireless::_clear_rooms() {

	for (int i = 0; i < rooms.size(); i++)
		memdelete(rooms[i]);
	rooms.clear();
}

Error LocalWireless::start_scan(int p_mode) {

	ERR_FAIL_COND_V(scanning, ERR_BUSY);
	ERR_FAIL_COND_V(get_status() != STATUS_DISCONNECTED, ERR_ALREADY_IN_USE);
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

bool LocalWireless::is_scanning() const {

	return scanning;
}

Array LocalWireless::get_rooms() const {

	Array result;
	mutex->lock();
	for (int i = 0; i < rooms.size(); i++) {
		Dictionary d;
		d["host_name"] = rooms[i]->host_name;
		d["players"] = rooms[i]->network.total_nodes;
		d["max_players"] = rooms[i]->network.max_nodes;
		d["info"] = rooms[i]->info;
		result.push_back(d);
	}
	mutex->unlock();
	return result;
}

Error LocalWireless::join(int p_room_index) {

	ERR_FAIL_COND_V(get_status() != STATUS_DISCONNECTED, ERR_ALREADY_IN_USE);
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

	packets.clear();
	status = STATUS_CLIENT;
	_invalidate_status();
	return OK;
}

void LocalWireless::leave() {

	if (status == STATUS_DISCONNECTED)
		return;

	if (status == STATUS_HOST)
		udsDestroyNetwork();
	else
		udsDisconnectNetwork();
	udsUnbind((udsBindContext *)bind_context);

	status = STATUS_DISCONNECTED;
	packets.clear();
	_invalidate_status();
}

void LocalWireless::_invalidate_status() {

	status_valid = false;
	names_bitmask = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
		node_names[i] = String();
}

void LocalWireless::_refresh_status() const {

	if (status == STATUS_DISCONNECTED)
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

void LocalWireless::_check_connection() {

	if (status != STATUS_CLIENT)
		return;

	// The host closed the room or went out of range.
	_refresh_status();
	if (status_valid && net_status == NETSTATUS_NOT_CONNECTED)
		leave();
}

LocalWireless::Status LocalWireless::get_status() {

	_check_connection();
	return status;
}

int LocalWireless::get_my_id() const {

	if (status == STATUS_DISCONNECTED)
		return 0;
	_refresh_status();
	return status_valid ? my_id : 0;
}

Array LocalWireless::get_players() const {

	Array result;
	if (status == STATUS_DISCONNECTED)
		return result;

	_refresh_status();
	if (!status_valid)
		return result;

	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!(node_bitmask & (1 << i)))
			continue;
		int id = i + 1;
		// Ask for a name only for players that joined since the last time.
		if (!(names_bitmask & (1 << i)) || node_names[i] == String()) {
			udsNodeInfo node;
			node_names[i] = R_SUCCEEDED(udsGetNodeInformation(id, &node)) ? _node_username(&node) : String();
		}
		Dictionary d;
		d["id"] = id;
		d["name"] = node_names[i];
		result.push_back(d);
	}
	names_bitmask = node_bitmask;
	return result;
}

Error LocalWireless::send(const DVector<uint8_t> &p_data, int p_to) {

	ERR_FAIL_COND_V(status == STATUS_DISCONNECTED, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_data.size() == 0 || p_data.size() > MAX_PACKET_SIZE, ERR_INVALID_PARAMETER);

	u16 to = p_to <= 0 ? UDS_BROADCAST_NETWORKNODEID : (u16)p_to;
	DVector<uint8_t>::Read r = p_data.read();
	Result res = udsSendTo(to, DATA_CHANNEL, UDS_SENDFLAG_Default, r.ptr(), p_data.size());
	if (UDS_CHECK_SENDTO_FATALERROR(res)) {
		ERR_PRINTS("LocalWireless: send failed (0x" + _uds_error(res) + ").");
		return ERR_CONNECTION_ERROR;
	}
	return OK;
}

Error LocalWireless::send_var(const Variant &p_var, int p_to) {

	int len = 0;
	Error err = encode_variant(p_var, NULL, len);
	ERR_FAIL_COND_V(err != OK, err);
	ERR_FAIL_COND_V(len > MAX_PACKET_SIZE, ERR_INVALID_PARAMETER);

	DVector<uint8_t> data;
	data.resize(len);
	{
		DVector<uint8_t>::Write w = data.write();
		encode_variant(p_var, w.ptr(), len);
	}
	return send(data, p_to);
}

void LocalWireless::_pull_packets() {

	if (status == STATUS_DISCONNECTED || !recv_buffer)
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

	while (true) {
		size_t size = 0;
		u16 sender = 0;
		Result res = udsPullPacket(ctx, recv_buffer, MAX_PACKET_SIZE, &size, &sender);
		if (R_FAILED(res) || size == 0)
			break;

		Packet p;
		p.sender = sender;
		p.data.resize(size);
		{
			DVector<uint8_t>::Write w = p.data.write();
			memcpy(w.ptr(), recv_buffer, size);
		}
		packets.push_back(p);
		if (packets.size() > MAX_QUEUED_PACKETS)
			packets.pop_front(); // nobody is reading: keep the newest
	}
}

int LocalWireless::get_packet_count() {

	_check_connection();
	_pull_packets();
	return packets.size();
}

DVector<uint8_t> LocalWireless::get_packet() {

	if (packets.empty())
		_pull_packets();
	if (packets.empty())
		return DVector<uint8_t>();

	Packet p = packets.front()->get();
	packets.pop_front();
	last_sender = p.sender;
	return p.data;
}

Variant LocalWireless::get_var() {

	DVector<uint8_t> data = get_packet();
	if (data.size() == 0)
		return Variant();

	Variant v;
	DVector<uint8_t>::Read r = data.read();
	if (decode_variant(v, r.ptr(), data.size()) != OK)
		return Variant();
	return v;
}

void LocalWireless::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("host", "max_players", "room_info", "mode"), &LocalWireless::host, DEFVAL(4), DEFVAL(""), DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("start_scan", "mode"), &LocalWireless::start_scan, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("is_scanning"), &LocalWireless::is_scanning);
	ObjectTypeDB::bind_method(_MD("get_rooms"), &LocalWireless::get_rooms);
	ObjectTypeDB::bind_method(_MD("join", "room_index"), &LocalWireless::join);
	ObjectTypeDB::bind_method(_MD("leave"), &LocalWireless::leave);
	ObjectTypeDB::bind_method(_MD("get_status"), &LocalWireless::get_status);
	ObjectTypeDB::bind_method(_MD("is_host"), &LocalWireless::is_host);
	ObjectTypeDB::bind_method(_MD("get_my_id"), &LocalWireless::get_my_id);
	ObjectTypeDB::bind_method(_MD("get_players"), &LocalWireless::get_players);
	ObjectTypeDB::bind_method(_MD("send", "data", "to_player"), &LocalWireless::send, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("send_var", "var", "to_player"), &LocalWireless::send_var, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("get_packet_count"), &LocalWireless::get_packet_count);
	ObjectTypeDB::bind_method(_MD("get_packet"), &LocalWireless::get_packet);
	ObjectTypeDB::bind_method(_MD("get_var"), &LocalWireless::get_var);
	ObjectTypeDB::bind_method(_MD("get_packet_sender"), &LocalWireless::get_packet_sender);

	BIND_CONSTANT(STATUS_DISCONNECTED);
	BIND_CONSTANT(STATUS_HOST);
	BIND_CONSTANT(STATUS_CLIENT);
	BIND_CONSTANT(MAX_PLAYERS);
	BIND_CONSTANT(MAX_PACKET_SIZE);
}

LocalWireless::LocalWireless() {

	singleton = this;
	initialized = false;
	status = STATUS_DISCONNECTED;
	comm_id = 0;
	last_sender = 0;
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
	recv_buffer = (uint8_t *)memalloc(MAX_PACKET_SIZE);
}

LocalWireless::~LocalWireless() {

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
	memdelete(mutex);
	singleton = NULL;
}
