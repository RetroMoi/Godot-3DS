/*************************************************************************/
/*  local_wireless.cpp                                                   */
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

#include "local_wireless.h"

#include "io/marshalls.h"
#include "math_funcs.h"
#include "os/os.h"

// Packet kinds (first byte of every packet on the wire).
enum {
	KIND_FAST = 0x47, // + payload
	KIND_RELIABLE = 0x52, // + seq (2 bytes, little endian) + payload
	KIND_ACK = 0x41, // + seq
};

#define RESEND_MSEC 150
#define MAX_TRIES 60 // ~9 s without a confirmation: give up (the player is surely gone)
#define MAX_QUEUED_PACKETS 256
#define MAX_OUT_OF_ORDER 128

LocalWireless *LocalWireless::singleton = NULL;

static inline int16_t _seq_diff(uint16_t a, uint16_t b) {

	return (int16_t)(uint16_t)(a - b);
}

LocalWireless::LocalWireless(LocalWirelessTransport *p_transport) {

	singleton = this;
	transport = p_transport;
	status = STATUS_DISCONNECTED;
	last_sender = 0;
	last_reliable = false;
	known_mask = 0;
	sim_loss = 0;
	sim_latency = 0;
	_reset_peers();
}

LocalWireless::~LocalWireless() {

	if (transport) {
		transport->leave();
		memdelete(transport);
	}
	singleton = NULL;
}

void LocalWireless::_reset_peers() {

	for (int i = 0; i < MAX_PLAYERS; i++)
		peers[i].reset();
	known_mask = 0;
	delayed.clear();
}

Error LocalWireless::host(int p_max_players, const String &p_room_info, int p_mode) {

	ERR_FAIL_COND_V(status != STATUS_DISCONNECTED, ERR_ALREADY_IN_USE);
	Error err = transport->host(CLAMP(p_max_players, 2, (int)MAX_PLAYERS), p_room_info, p_mode);
	if (err != OK)
		return err;
	packets.clear();
	_reset_peers();
	status = STATUS_HOST;
	return OK;
}

Error LocalWireless::start_scan(int p_mode) {

	ERR_FAIL_COND_V(status != STATUS_DISCONNECTED, ERR_ALREADY_IN_USE);
	return transport->start_scan(p_mode);
}

bool LocalWireless::is_scanning() const {

	return transport->is_scanning();
}

Array LocalWireless::get_rooms() const {

	Vector<LocalWirelessTransport::Room> rooms;
	transport->get_rooms(&rooms);
	Array result;
	for (int i = 0; i < rooms.size(); i++) {
		Dictionary d;
		d["host_name"] = rooms[i].host_name;
		d["players"] = rooms[i].players;
		d["max_players"] = rooms[i].max_players;
		d["info"] = rooms[i].info;
		result.push_back(d);
	}
	return result;
}

Error LocalWireless::join(int p_room_index) {

	ERR_FAIL_COND_V(status != STATUS_DISCONNECTED, ERR_ALREADY_IN_USE);
	Error err = transport->join(p_room_index);
	if (err != OK)
		return err;
	packets.clear();
	_reset_peers();
	status = STATUS_CLIENT;
	return OK;
}

void LocalWireless::leave() {

	if (status == STATUS_DISCONNECTED)
		return;
	transport->leave();
	status = STATUS_DISCONNECTED;
	packets.clear();
	_reset_peers();
}

void LocalWireless::_set_disconnected(bool p_signal) {

	transport->leave();
	status = STATUS_DISCONNECTED;
	_reset_peers();
	// Packets already received stay readable.
	if (p_signal)
		emit_signal("disconnected");
}

LocalWireless::Status LocalWireless::get_status() {

	if (status != STATUS_DISCONNECTED && !transport->is_in_room())
		_set_disconnected(true);
	return status;
}

int LocalWireless::get_my_id() const {

	return status == STATUS_DISCONNECTED ? 0 : transport->get_my_id();
}

Array LocalWireless::get_players() const {

	Array result;
	if (status == STATUS_DISCONNECTED)
		return result;
	uint32_t mask = transport->get_player_mask();
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!(mask & (1 << i)))
			continue;
		Dictionary d;
		d["id"] = i + 1;
		d["name"] = transport->get_player_name(i + 1);
		result.push_back(d);
	}
	return result;
}

int LocalWireless::get_max_packet_size() const {

	return transport->get_max_packet_size() - HEADER_SIZE;
}

/////////////////////////////////////////////////////////////////////////////
// Sending

Error LocalWireless::_send_raw(const Vector<uint8_t> &p_raw, int p_to) {

	if (sim_loss > 0 && Math::randf() < sim_loss)
		return OK; // lost on purpose
	if (sim_latency > 0) {
		Delayed d;
		d.raw = p_raw;
		d.to = p_to;
		d.at_msec = OS::get_singleton()->get_ticks_msec() + sim_latency;
		delayed.push_back(d);
		return OK;
	}
	return transport->send_raw(p_raw.ptr(), p_raw.size(), p_to);
}

Error LocalWireless::send(const DVector<uint8_t> &p_data, int p_to) {

	ERR_FAIL_COND_V(status == STATUS_DISCONNECTED, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_data.size() == 0 || p_data.size() > get_max_packet_size(), ERR_INVALID_PARAMETER);
	Vector<uint8_t> raw;
	raw.resize(p_data.size() + 1);
	raw[0] = KIND_FAST;
	DVector<uint8_t>::Read r = p_data.read();
	copymem(raw.ptr() + 1, r.ptr(), p_data.size());
	return _send_raw(raw, p_to);
}

Error LocalWireless::_send_reliable_to(const DVector<uint8_t> &p_data, int p_to) {

	Peer &peer = peers[p_to - 1];
	Pending p;
	p.seq = peer.send_seq++;
	p.raw.resize(p_data.size() + 3);
	p.raw[0] = KIND_RELIABLE;
	p.raw[1] = p.seq & 0xFF;
	p.raw[2] = p.seq >> 8;
	DVector<uint8_t>::Read r = p_data.read();
	copymem(p.raw.ptr() + 3, r.ptr(), p_data.size());
	p.sent_msec = OS::get_singleton()->get_ticks_msec();
	p.tries = 1;
	peer.pending.push_back(p);
	return _send_raw(p.raw, p_to);
}

Error LocalWireless::send_reliable(const DVector<uint8_t> &p_data, int p_to) {

	ERR_FAIL_COND_V(status == STATUS_DISCONNECTED, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_data.size() == 0 || p_data.size() > get_max_packet_size(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_to < 0 || p_to > MAX_PLAYERS, ERR_INVALID_PARAMETER);
	int me = transport->get_my_id();
	if (p_to > 0) {
		ERR_FAIL_COND_V(p_to == me, ERR_INVALID_PARAMETER);
		return _send_reliable_to(p_data, p_to);
	}
	// Everyone else: one reliable stream per player (each confirms its own copy).
	uint32_t mask = transport->get_player_mask();
	Error err = OK;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if ((mask & (1 << i)) && i + 1 != me) {
			Error e = _send_reliable_to(p_data, i + 1);
			if (e != OK)
				err = e;
		}
	}
	return err;
}

Error LocalWireless::_encode(const Variant &p_var, DVector<uint8_t> *r_data) {

	int len = 0;
	Error err = encode_variant(p_var, NULL, len);
	ERR_FAIL_COND_V(err != OK, err);
	r_data->resize(len);
	DVector<uint8_t>::Write w = r_data->write();
	encode_variant(p_var, w.ptr(), len);
	return OK;
}

Error LocalWireless::send_var(const Variant &p_var, int p_to) {

	DVector<uint8_t> data;
	Error err = _encode(p_var, &data);
	if (err != OK)
		return err;
	return send(data, p_to);
}

Error LocalWireless::send_var_reliable(const Variant &p_var, int p_to) {

	DVector<uint8_t> data;
	Error err = _encode(p_var, &data);
	if (err != OK)
		return err;
	return send_reliable(data, p_to);
}

int LocalWireless::get_pending_reliable_count() const {

	int n = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
		n += peers[i].pending.size();
	return n;
}

/////////////////////////////////////////////////////////////////////////////
// Receiving

void LocalWireless::_deliver(const uint8_t *p_data, int p_size, int p_sender, bool p_reliable) {

	Packet p;
	p.sender = p_sender;
	p.reliable = p_reliable;
	p.data.resize(p_size);
	{
		DVector<uint8_t>::Write w = p.data.write();
		copymem(w.ptr(), p_data, p_size);
	}
	packets.push_back(p);
	if (packets.size() > MAX_QUEUED_PACKETS) {
		WARN_PRINT("LocalWireless: more than 256 packets waiting to be read; dropping the oldest.");
		packets.pop_front();
	}
}

void LocalWireless::_receive() {

	if (status == STATUS_DISCONNECTED)
		return;
	Vector<uint8_t> raw;
	int sender = 0;
	while (transport->pull_raw(&raw, &sender)) {
		if (raw.size() < 1 || sender < 1 || sender > MAX_PLAYERS)
			continue;
		const uint8_t *d = raw.ptr();
		int size = raw.size();
		Peer &peer = peers[sender - 1];
		if (d[0] == KIND_FAST) {
			if (size > 1)
				_deliver(d + 1, size - 1, sender, false);
		} else if (d[0] == KIND_ACK && size >= 3) {
			uint16_t seq = d[1] | (d[2] << 8);
			for (List<Pending>::Element *E = peer.pending.front(); E; E = E->next()) {
				if (E->get().seq == seq) {
					peer.pending.erase(E);
					break;
				}
			}
		} else if (d[0] == KIND_RELIABLE && size >= 4) {
			uint16_t seq = d[1] | (d[2] << 8);
			// Confirm every copy (the previous confirmation may have been lost).
			Vector<uint8_t> ack;
			ack.resize(3);
			ack[0] = KIND_ACK;
			ack[1] = d[1];
			ack[2] = d[2];
			_send_raw(ack, sender);

			int16_t diff = _seq_diff(seq, peer.recv_expected);
			if (diff == 0) {
				_deliver(d + 3, size - 3, sender, true);
				peer.recv_expected++;
				// Packets that arrived early, now in order.
				while (peer.out_of_order.has(peer.recv_expected)) {
					DVector<uint8_t> &later = peer.out_of_order[peer.recv_expected];
					DVector<uint8_t>::Read r = later.read();
					_deliver(r.ptr(), later.size(), sender, true);
					peer.out_of_order.erase(peer.recv_expected);
					peer.recv_expected++;
				}
			} else if (diff > 0 && !peer.out_of_order.has(seq) && peer.out_of_order.size() < MAX_OUT_OF_ORDER) {
				DVector<uint8_t> later;
				later.resize(size - 3);
				DVector<uint8_t>::Write w = later.write();
				copymem(w.ptr(), d + 3, size - 3);
				peer.out_of_order[seq] = later;
			}
			// diff < 0: a copy of something already delivered.
		}
	}
}

int LocalWireless::get_packet_count() {

	_receive();
	return packets.size();
}

DVector<uint8_t> LocalWireless::get_packet() {

	if (packets.empty())
		_receive();
	if (packets.empty())
		return DVector<uint8_t>();
	Packet p = packets.front()->get();
	packets.pop_front();
	last_sender = p.sender;
	last_reliable = p.reliable;
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

/////////////////////////////////////////////////////////////////////////////
// Every frame

void LocalWireless::poll() {

	transport->poll();
	if (status == STATUS_DISCONNECTED)
		return;
	if (!transport->is_in_room()) {
		_set_disconnected(true);
		return;
	}

	uint64_t now = OS::get_singleton()->get_ticks_msec();

	// Simulated latency: release what is due.
	while (!delayed.empty() && delayed.front()->get().at_msec <= now) {
		const Delayed &d = delayed.front()->get();
		transport->send_raw(d.raw.ptr(), d.raw.size(), d.to);
		delayed.pop_front();
	}

	_receive();

	// Joins and leaves (this console's own bit aside).
	uint32_t mask = transport->get_player_mask();
	int me = transport->get_my_id();
	if (me > 0)
		mask &= ~(1u << (me - 1));
	uint32_t joined = mask & ~known_mask;
	uint32_t left = known_mask & ~mask;
	known_mask = mask;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (left & (1 << i)) {
			peers[i].reset();
			emit_signal("player_left", i + 1);
		}
	}
	// No reset on join: a slot is already clean (reset when its previous player left, or when
	// this console hosted/joined), and packets may already be in flight with this player,
	// e.g. the first reliable message sent right after join(), before the system lists the
	// host; wiping it would drop it for good if its first copy was lost (common on UDS just
	// after connecting).
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (joined & (1 << i))
			emit_signal("player_joined", i + 1);
	}

	// Resend what was not confirmed.
	for (int i = 0; i < MAX_PLAYERS; i++) {
		Peer &peer = peers[i];
		List<Pending>::Element *E = peer.pending.front();
		while (E) {
			List<Pending>::Element *next = E->next();
			Pending &p = E->get();
			if (now - p.sent_msec >= RESEND_MSEC) {
				if (p.tries >= MAX_TRIES) {
					WARN_PRINT(("LocalWireless: player " + itos(i + 1) + " did not confirm a reliable packet; giving up on it.").utf8().get_data());
					peer.pending.erase(E);
				} else {
					p.tries++;
					p.sent_msec = now;
					_send_raw(p.raw, i + 1);
				}
			}
			E = next;
		}
	}
}

void LocalWireless::set_simulated_loss(float p_ratio) {

	sim_loss = CLAMP(p_ratio, 0.0f, 1.0f);
}

void LocalWireless::set_simulated_latency(int p_msec) {

	sim_latency = MAX(0, p_msec);
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
	ObjectTypeDB::bind_method(_MD("get_max_packet_size"), &LocalWireless::get_max_packet_size);
	ObjectTypeDB::bind_method(_MD("send", "data", "to_player"), &LocalWireless::send, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("send_var", "var", "to_player"), &LocalWireless::send_var, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("send_reliable", "data", "to_player"), &LocalWireless::send_reliable, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("send_var_reliable", "var", "to_player"), &LocalWireless::send_var_reliable, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("get_packet_count"), &LocalWireless::get_packet_count);
	ObjectTypeDB::bind_method(_MD("get_packet"), &LocalWireless::get_packet);
	ObjectTypeDB::bind_method(_MD("get_var"), &LocalWireless::get_var);
	ObjectTypeDB::bind_method(_MD("get_packet_sender"), &LocalWireless::get_packet_sender);
	ObjectTypeDB::bind_method(_MD("was_packet_reliable"), &LocalWireless::was_packet_reliable);
	ObjectTypeDB::bind_method(_MD("get_pending_reliable_count"), &LocalWireless::get_pending_reliable_count);
	ObjectTypeDB::bind_method(_MD("add_scan_address", "ip"), &LocalWireless::add_scan_address);
	ObjectTypeDB::bind_method(_MD("set_simulated_loss", "ratio"), &LocalWireless::set_simulated_loss);
	ObjectTypeDB::bind_method(_MD("get_simulated_loss"), &LocalWireless::get_simulated_loss);
	ObjectTypeDB::bind_method(_MD("set_simulated_latency", "msec"), &LocalWireless::set_simulated_latency);
	ObjectTypeDB::bind_method(_MD("get_simulated_latency"), &LocalWireless::get_simulated_latency);

	ADD_SIGNAL(MethodInfo("player_joined", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("player_left", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("disconnected"));

	BIND_CONSTANT(STATUS_DISCONNECTED);
	BIND_CONSTANT(STATUS_HOST);
	BIND_CONSTANT(STATUS_CLIENT);
	BIND_CONSTANT(MAX_PLAYERS);
	BIND_CONSTANT(MAX_PACKET_SIZE);
}
