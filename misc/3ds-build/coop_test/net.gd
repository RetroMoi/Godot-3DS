extends Node

# Sessão co-op sobre o LocalWireless (3DS: UDS; PC: UDP, para testar com duas janelas).
#
# Dois canais:
#   rápido (send): estado dos players e dos inimigos, 20x por segundo. Pode perder ou chegar
#     fora de ordem: cada pacote leva um número de sequência e os velhos são descartados.
#   confiável (send_var_reliable): aperto de mão, acertos, vida dos inimigos. Chega sempre,
#     em ordem.
#
# Os pacotes rápidos são bytes (StreamPeerBuffer): ~8 bytes por player, ~6 por inimigo.

const PROTOCOL = 1     # muda quando o formato dos pacotes muda: versões diferentes não jogam juntas
const MSG_PLAYER = 1   # u8 tipo, u16 seq, s16 x, s16 y, u8 hp, u8 flags
const MSG_ENEMIES = 2  # u8 tipo, u16 seq, u8 n, n x (u8 id, s16 x, s16 y, u8 hp)

var lw = null
var send_seq = 0
var last_seq = {}      # remetente -> último seq rápido aceito

func _ready():
	lw = Globals.get_singleton("LocalWireless")
	# Só para testes no PC: COOP_LOSS=0.3 COOP_LATENCY=80 simulam uma conexão ruim.
	if OS.get_environment("COOP_LOSS") != "":
		lw.set_simulated_loss(float(OS.get_environment("COOP_LOSS")))
	if OS.get_environment("COOP_LATENCY") != "":
		lw.set_simulated_latency(int(OS.get_environment("COOP_LATENCY")))

func reset():
	send_seq = 0
	last_seq.clear()

# --- envio -------------------------------------------------------------------------

func send_player(pos, hp, flags):
	var b = StreamPeerBuffer.new()
	b.put_u8(MSG_PLAYER)
	b.put_u16(_next_seq())
	b.put_16(int(pos.x))
	b.put_16(int(pos.y))
	b.put_u8(hp)
	b.put_u8(flags)
	lw.send(b.get_data_array())

func send_enemies(list):  # [[id, pos, hp], ...]
	var b = StreamPeerBuffer.new()
	b.put_u8(MSG_ENEMIES)
	b.put_u16(_next_seq())
	b.put_u8(list.size())
	for e in list:
		b.put_u8(e[0])
		b.put_16(int(e[1].x))
		b.put_16(int(e[1].y))
		b.put_u8(e[2])
	lw.send(b.get_data_array())

func send_event(msg, to = 0):  # msg: Array, ex. ["hit", 2]
	lw.send_var_reliable(msg, to)

func _next_seq():
	send_seq = (send_seq + 1) % 65536
	return send_seq

# --- recepção ----------------------------------------------------------------------
# Devolve uma lista de mensagens já decodificadas:
#   ["player", remetente, pos, hp, flags]
#   ["enemies", remetente, [[id, pos, hp], ...]]
#   ["event", remetente, Array]
func receive():
	var out = []
	while lw.get_packet_count() > 0:
		var raw = lw.get_packet()
		var sender = lw.get_packet_sender()
		if lw.was_packet_reliable():
			out.append(["event", sender, bytes2var(raw)])
			continue
		var p = StreamPeerBuffer.new()
		p.set_data_array(raw)
		var kind = p.get_u8()
		var seq = p.get_u16()
		if not _is_newer(sender, kind, seq):
			continue  # chegou atrasado: já temos algo mais novo
		if kind == MSG_PLAYER:
			var pos = Vector2(p.get_16(), p.get_16())
			out.append(["player", sender, pos, p.get_u8(), p.get_u8()])
		elif kind == MSG_ENEMIES:
			var n = p.get_u8()
			var list = []
			for i in range(n):
				var id = p.get_u8()
				var pos = Vector2(p.get_16(), p.get_16())
				list.append([id, pos, p.get_u8()])
			out.append(["enemies", sender, list])
	return out

func _is_newer(sender, kind, seq):
	var key = sender * 256 + kind
	if last_seq.has(key):
		var diff = (seq - last_seq[key] + 65536) % 65536
		if diff == 0 or diff > 32768:
			return false
	last_seq[key] = seq
	return true
