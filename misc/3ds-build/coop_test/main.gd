extends Node2D

# Referência de co-op local (2 jogadores) numa caverna pequena, para o Open.
#
# Quem decide o quê:
#   - cada console decide o movimento e a vida do PRÓPRIO player (sem atraso nos controles);
#     o dano por contato com inimigos é detectado no próprio console;
#   - o host decide o mundo: IA e vida dos inimigos, acertos, renascimento dos inimigos.
#     No convidado os inimigos são "marionetes": só desenham o que o host manda.
# Regras do host (vão no anúncio da sala): sem fogo amigo, uma vida cada, drops individuais,
# morte = volta ao ponto de origem do mapa; sair encerra a sessão (com confirmação).
#
# Tela de cima: a caverna. Tela de baixo: sala (criar/procurar/entrar) e avisos.
# Controles (ações coop_* no engine.cfg, no padrão Nintendo: A confirma, B volta):
#   sala: A (Enter) cria, X (S) procura, Y (J) entra na primeira sala achada (ou toque nos botões);
#   jogo: direcional/Circle Pad (setas no PC) move; A ataca; B (Esc) abre a confirmação de saída.
# Obs.: os ui_* padrão do 2.1 seguem o layout do Xbox (ui_accept = botão de baixo = B no 3DS).
# PC: rode duas janelas do projeto; uma cria a sala, a outra procura e entra.

const LOBBY_HELP = "A: criar sala   X: procurar   Y: entrar na 1a sala"
const RULES = "sem fogo amigo; 1 vida cada; drops individuais"
const SEND_INTERVAL = 0.05      # 20 envios rápidos por segundo
const INTERP_DELAY = 100        # ms: as marionetes são desenhadas um pouco no passado
const SPEED = 80.0
const MAX_HP = 5
const ENEMY_HP = 3
const SPAWN = Vector2(40, 120)  # ponto de origem do mapa (renascimento)
const WALLS = [Rect2(0, 0, 400, 16), Rect2(0, 224, 400, 16), Rect2(0, 0, 16, 240), Rect2(384, 0, 16, 240),
		Rect2(110, 50, 36, 110), Rect2(250, 30, 28, 90), Rect2(210, 170, 100, 18)]
const ENEMY_START = [Vector2(320, 130), Vector2(190, 60)]
const FLAG_ATTACK = 1

var net = null
var lw = null
var in_world = false
var waiting_welcome = false
var scanning = false
var msg = ""

# player local
var pos = SPAWN
var hp = MAX_HP
var invul = 0.0
var attack = 0.0
var send_timer = 0.0

# outros players: id -> {"snaps": [[ms, pos]], "hp": n, "flags": n}
var remotes = {}
# inimigos: id -> {"pos", "hp", "start", "respawn"} (host) / {"snaps", "hp"} (convidado)
var enemies = {}

var auto = ""
var bot = false
var bot_t = 0.0

var prompt_open = false
var prev_keys = {}
var pressed_now = {}   # ações apertadas neste quadro (coop_a, coop_b, coop_x, coop_y)

func _ready():
	net = get_node("net")
	lw = net.lw
	lw.connect("player_joined", self, "_on_player_joined")
	lw.connect("player_left", self, "_on_player_left")
	lw.connect("disconnected", self, "_on_disconnected")
	get_node("criar").connect("pressed", self, "_create")
	get_node("procurar").connect("pressed", self, "_scan")
	for i in range(3):
		get_node("sala%d" % i).connect("pressed", self, "_join", [i])
	_show_lobby(LOBBY_HELP)
	set_process(true)
	# Testes automáticos no PC (duas janelas sem cliques):
	#   COOP_AUTO=host | join   cria a sala / entra na primeira encontrada
	#   COOP_BOT=1              o player anda em círculos e ataca sozinho
	#   COOP_WINPOS=x,y         posição da janela
	if OS.get_environment("COOP_WINPOS") != "":
		var xy = OS.get_environment("COOP_WINPOS").split(",")
		OS.set_window_position(Vector2(int(xy[0]), int(xy[1])))
	auto = OS.get_environment("COOP_AUTO")
	bot = OS.get_environment("COOP_BOT") == "1"
	if auto == "host":
		_create()
	elif auto == "join":
		_scan()

# --- sala ------------------------------------------------------------------------------

func _show_lobby(text):
	in_world = false
	waiting_welcome = false
	prompt_open = false
	remotes.clear()
	enemies.clear()
	msg = text if text == LOBBY_HELP else text + "\n" + LOBBY_HELP
	get_node("info").set_pos(Vector2(44, 398))
	get_node("info").set_size(Vector2(312, 80))
	get_node("criar").show()
	get_node("procurar").show()
	for i in range(3):
		get_node("sala%d" % i).hide()
	update()

func _create():
	if lw.host(2, RULES) != OK:
		msg = "Não foi possível criar a sala."
		return
	net.reset()
	_enter_world()
	msg = "Sala criada. Esperando o outro jogador..."

func _scan():
	if lw.start_scan() == OK:
		scanning = true
		msg = "Procurando salas..."

func _join(i):
	if lw.join(i) != OK:
		msg = "Não foi possível entrar."
		return
	net.reset()
	waiting_welcome = true
	msg = "Entrando..."
	net.send_event(["hello", net.PROTOCOL], 1)
	for j in range(3):
		get_node("sala%d" % j).hide()

func _enter_world():
	in_world = true
	pos = SPAWN if lw.is_host() else SPAWN + Vector2(0, 24)
	hp = MAX_HP
	get_node("info").set_pos(Vector2(44, 300))  # sem botões no jogo: sobra espaço para avisos
	get_node("info").set_size(Vector2(312, 176))
	get_node("criar").hide()
	get_node("procurar").hide()
	for i in range(3):
		get_node("sala%d" % i).hide()
	enemies.clear()
	if lw.is_host():
		for i in range(ENEMY_START.size()):
			enemies[i + 1] = {"pos": ENEMY_START[i], "hp": ENEMY_HP, "start": ENEMY_START[i], "respawn": 0.0}

func _leave(text):
	lw.leave()
	_show_lobby(text)

func _on_player_joined(id):
	if in_world:
		msg = "Jogador %d entrou." % id

func _on_player_left(id):
	remotes.erase(id)
	if in_world:
		msg = "Jogador %d saiu." % id

func _on_disconnected():
	_show_lobby("A sessão terminou (o host saiu ou ficou fora de alcance).")

# --- quadro ----------------------------------------------------------------------------

func _read_keys():
	for a in ["coop_a", "coop_b", "coop_x", "coop_y"]:
		var down = Input.is_action_pressed(a)
		pressed_now[a] = down and not prev_keys.has(a)
		if down:
			prev_keys[a] = true
		else:
			prev_keys.erase(a)

func _process(delta):
	_read_keys()
	if not in_world and not waiting_welcome and lw.get_status() == 0:
		if pressed_now["coop_a"]:
			_create()
		elif pressed_now["coop_x"] and not scanning:
			_scan()
		elif pressed_now["coop_y"] and lw.get_rooms().size() > 0:
			_join(0)
	if scanning and not lw.is_scanning():
		scanning = false
		var rooms = lw.get_rooms()
		msg = "%d sala(s) encontrada(s)." % rooms.size()
		for i in range(3):
			var b = get_node("sala%d" % i)
			if i < rooms.size():
				var r = rooms[i]
				b.set_text("%s (%d/%d) %s" % [r["host_name"], r["players"], r["max_players"], r["info"]])
				b.show()
			else:
				b.hide()
		if auto == "join":
			if rooms.size() > 0:
				_join(0)
			else:
				_scan()  # o host ainda não abriu: procura de novo

	if lw.get_status() != 0:
		_handle_network()

	if in_world:
		_update_player(delta)
		if lw.is_host():
			_update_enemies(delta)
		send_timer += delta
		if send_timer >= SEND_INTERVAL:
			send_timer = 0.0
			net.send_player(pos, hp, FLAG_ATTACK if attack > 0 else 0)
			if lw.is_host():
				var list = []
				for id in enemies:
					list.append([id, enemies[id]["pos"], enemies[id]["hp"]])
				net.send_enemies(list)

	_update_hud()
	update()

func _handle_network():
	var now = OS.get_ticks_msec()
	for m in net.receive():
		if m[0] == "player":
			if not remotes.has(m[1]):
				remotes[m[1]] = {"snaps": [], "hp": MAX_HP, "flags": 0}
			var r = remotes[m[1]]
			r["snaps"].append([now, m[2]])
			if r["snaps"].size() > 8:
				r["snaps"].pop_front()
			r["hp"] = m[3]
			r["flags"] = m[4]
		elif m[0] == "enemies" and not lw.is_host():
			for e in m[2]:
				if not enemies.has(e[0]):
					enemies[e[0]] = {"snaps": [], "hp": e[2]}
				enemies[e[0]]["snaps"].append([now, e[1]])
				if enemies[e[0]]["snaps"].size() > 8:
					enemies[e[0]]["snaps"].pop_front()
				enemies[e[0]]["hp"] = e[2]
		elif m[0] == "event":
			_handle_event(m[1], m[2])

func _handle_event(sender, ev):
	if typeof(ev) != TYPE_ARRAY or ev.size() == 0:
		return
	var kind = ev[0]
	if kind == "hello" and lw.is_host():
		if ev[1] != net.PROTOCOL:
			net.send_event(["reject", "versão diferente do jogo"], sender)
		else:
			net.send_event(["welcome", net.PROTOCOL, RULES], sender)
	elif kind == "welcome" and waiting_welcome:
		waiting_welcome = false
		_enter_world()
		msg = "Na sala. Regras: " + ev[2]
	elif kind == "reject":
		_leave("Recusado pelo host: " + ev[1])
	elif kind == "hit" and lw.is_host():
		_hit_enemy(ev[1])
	elif kind == "enemy_hp" and not lw.is_host():
		if enemies.has(ev[1]):
			enemies[ev[1]]["hp"] = ev[2]

# --- player local ----------------------------------------------------------------------

func _update_player(delta):
	var accept_now = pressed_now["coop_a"]
	var cancel_now = pressed_now["coop_b"]

	if prompt_open:
		if accept_now:
			_leave("Você saiu da sessão.")
		elif cancel_now:
			prompt_open = false
		return
	if cancel_now:
		prompt_open = true
		return

	var dir = Vector2(Input.get_joy_axis(0, JOY_ANALOG_0_X), Input.get_joy_axis(0, JOY_ANALOG_0_Y))
	if Input.is_action_pressed("ui_left"): dir.x -= 1
	if Input.is_action_pressed("ui_right"): dir.x += 1
	if Input.is_action_pressed("ui_up"): dir.y -= 1
	if Input.is_action_pressed("ui_down"): dir.y += 1
	if bot:
		bot_t += delta
		dir = Vector2(cos(bot_t * 1.3), sin(bot_t * 1.7))
		if int(bot_t * 2) % 3 == 0 and attack <= 0:
			accept_now = true
	if dir.length() > 1: dir = dir.normalized()
	_move(dir * SPEED * delta)

	invul = max(0.0, invul - delta)
	attack = max(0.0, attack - delta)
	if accept_now and attack <= 0:
		attack = 0.2
		# Sem fogo amigo: o ataque só testa inimigos.
		for id in enemies:
			if enemies[id]["hp"] > 0 and _enemy_pos(id).distance_to(pos) < 26:
				if lw.is_host():
					_hit_enemy(id)
				else:
					net.send_event(["hit", id], 1)  # o host decide

	# Dano por contato, decidido aqui (cada um cuida do próprio player).
	if invul <= 0:
		for id in enemies:
			if enemies[id]["hp"] > 0 and _enemy_pos(id).distance_to(pos) < 12:
				hp -= 1
				invul = 1.0
				break
	if hp <= 0:
		hp = MAX_HP
		pos = SPAWN  # regra: renasce na origem do mapa
		msg = "Você caiu e voltou à entrada."

func _move(m):
	var nx = pos + Vector2(m.x, 0)
	if not _blocked(nx): pos = nx
	var ny = pos + Vector2(0, m.y)
	if not _blocked(ny): pos = ny

func _blocked(p):
	var box = Rect2(p - Vector2(6, 6), Vector2(12, 12))
	for w in WALLS:
		if w.intersects(box):
			return true
	return false

# --- inimigos (host) -------------------------------------------------------------------

func _update_enemies(delta):
	for id in enemies:
		var e = enemies[id]
		if e["hp"] <= 0:
			e["respawn"] -= delta
			if e["respawn"] <= 0:
				e["hp"] = ENEMY_HP
				e["pos"] = e["start"]
				net.send_event(["enemy_hp", id, e["hp"]])
			continue
		# Persegue o player mais perto (o do host ou o convidado).
		var target = pos
		for rid in remotes:
			var rp = _remote_pos(rid)
			if rp.distance_to(e["pos"]) < target.distance_to(e["pos"]):
				target = rp
		if target.distance_to(e["pos"]) < 140:
			var step = (target - e["pos"]).normalized() * 40 * delta
			var np = e["pos"] + step
			var box = Rect2(np - Vector2(7, 7), Vector2(14, 14))
			var ok = true
			for w in WALLS:
				if w.intersects(box): ok = false
			if ok: e["pos"] = np

func _hit_enemy(id):
	if not enemies.has(id) or enemies[id]["hp"] <= 0:
		return
	var e = enemies[id]
	e["hp"] -= 1
	if e["hp"] <= 0:
		e["respawn"] = 3.0
	net.send_event(["enemy_hp", id, e["hp"]])

# --- interpolação ----------------------------------------------------------------------

func _sample(snaps):
	# Posição ~100 ms no passado, entre os dois estados que a cercam.
	if snaps.size() == 0:
		return Vector2()
	var t = OS.get_ticks_msec() - INTERP_DELAY
	if t <= snaps[0][0]:
		return snaps[0][1]
	for i in range(snaps.size() - 1):
		var a = snaps[i]
		var b = snaps[i + 1]
		if t <= b[0]:
			var k = float(t - a[0]) / max(1, b[0] - a[0])
			return a[1].linear_interpolate(b[1], k)
	return snaps[snaps.size() - 1][1]

func _remote_pos(id):
	return _sample(remotes[id]["snaps"])

func _enemy_pos(id):
	return enemies[id]["pos"] if lw.is_host() else _sample(enemies[id]["snaps"])

# --- desenho ---------------------------------------------------------------------------

func _update_hud():
	var who = "fora de sala"
	if lw.get_status() == 1: who = "host"
	elif lw.get_status() == 2: who = "convidado"
	var others = ""
	for id in remotes:
		others += "  J%d: %d/%d" % [id, remotes[id]["hp"], MAX_HP]
	var text = "%s (id %d)   FPS %d\nVida: %d/%d%s\n%s" % [who, lw.get_my_id(), OS.get_frames_per_second(), hp, MAX_HP, others, msg]
	if prompt_open:
		text = "SAIR ENCERRA A SESSÃO MULTIPLAYER.\nA: sair   B: voltar\n\n" + text
	get_node("info").set_text(text)

func _draw():
	if not in_world:
		return
	draw_rect(Rect2(0, 0, 400, 240), Color(0.08, 0.07, 0.1))
	for w in WALLS:
		draw_rect(w, Color(0.35, 0.32, 0.3))
	for id in enemies:
		if enemies[id]["hp"] > 0:
			var ep = _enemy_pos(id)
			draw_circle(ep, 7, Color(0.9, 0.2, 0.25))
			for i in range(enemies[id]["hp"]):
				draw_rect(Rect2(ep + Vector2(-7 + i * 5, -12), Vector2(4, 2)), Color(1, 0.6, 0.6))
	for id in remotes:
		_draw_player(_remote_pos(id), Color(0.3, 0.7, 1), remotes[id]["flags"] & FLAG_ATTACK)
	var col = Color(0.4, 1, 0.4) if invul <= 0 or int(invul * 10) % 2 == 0 else Color(0.4, 1, 0.4, 0.3)
	_draw_player(pos, col, attack > 0)

func _draw_player(p, c, attacking):
	draw_rect(Rect2(p - Vector2(6, 6), Vector2(12, 12)), c)
	if attacking:
		draw_circle(p, 26, Color(c.r, c.g, c.b, 0.2))
