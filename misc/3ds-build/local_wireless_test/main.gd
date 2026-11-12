extends Node2D

# Teste do jogo local sem fio (singleton LocalWireless, só no 3DS).
# Tela de baixo: criar sala, procurar salas, entrar, sair.
# Tela de cima: cada console move o seu quadrado (Circle Pad / direcional) e vê os dos outros.
# Só jogos com o mesmo nome de projeto se enxergam.

const DESCONECTADO = 0
const HOST = 1
const CLIENTE = 2

var lw = null
var meu_pos = Vector2(200, 150)
var outros = {}   # id do jogador -> posição
var nomes = {}    # id do jogador -> nome do console
var procurando = false
var tempo_lista = 0.0
var msg = ""
var cores = [Color(1, 0.3, 0.3), Color(0.3, 1, 0.3), Color(0.3, 0.6, 1), Color(1, 1, 0.3),
		Color(1, 0.4, 1), Color(0.3, 1, 1), Color(1, 0.6, 0.2), Color(0.85, 0.85, 0.85)]

func _ready():
	if Globals.has_singleton("LocalWireless"):
		lw = Globals.get_singleton("LocalWireless")
		msg = "Crie uma sala ou procure uma."
	else:
		msg = "LocalWireless so existe no 3DS."
	get_node("criar").connect("pressed", self, "_criar")
	get_node("procurar").connect("pressed", self, "_procurar")
	get_node("sair").connect("pressed", self, "_sair")
	for i in range(4):
		get_node("sala%d" % i).connect("pressed", self, "_entrar", [i])
		get_node("sala%d" % i).hide()
	set_process(true)

func _criar():
	if lw == null:
		return
	var err = lw.host(4, "Teste de jogo local")
	msg = "Sala criada. Esperando jogadores..." if err == OK else "Erro ao criar a sala: %d" % err
	_esconder_salas()

func _procurar():
	if lw == null:
		return
	var err = lw.start_scan()
	if err == OK:
		procurando = true
		msg = "Procurando salas..."
	else:
		msg = "Erro ao procurar: %d" % err

func _entrar(i):
	var err = lw.join(i)
	msg = "Entrou na sala." if err == OK else "Erro ao entrar: %d" % err
	_esconder_salas()

func _sair():
	if lw == null:
		return
	lw.leave()
	outros.clear()
	nomes.clear()
	msg = "Saiu da sala."

func _esconder_salas():
	for i in range(4):
		get_node("sala%d" % i).hide()

func _mostrar_salas():
	var salas = lw.get_rooms()
	msg = "%d sala(s) encontrada(s)." % salas.size()
	for i in range(4):
		var b = get_node("sala%d" % i)
		if i < salas.size():
			var s = salas[i]
			b.set_text("Entrar: %s (%d/%d) %s" % [s["host_name"], s["players"], s["max_players"], s["info"]])
			b.show()
		else:
			b.hide()

func _process(delta):
	var dir = Vector2(Input.get_joy_axis(0, JOY_ANALOG_0_X), Input.get_joy_axis(0, JOY_ANALOG_0_Y))
	if Input.is_action_pressed("ui_left"):
		dir.x -= 1
	if Input.is_action_pressed("ui_right"):
		dir.x += 1
	if Input.is_action_pressed("ui_up"):
		dir.y -= 1
	if Input.is_action_pressed("ui_down"):
		dir.y += 1
	meu_pos += dir * 120 * delta
	meu_pos = Vector2(clamp(meu_pos.x, 10, 390), clamp(meu_pos.y, 70, 230))

	var estado = DESCONECTADO
	var meu_id = 0
	if lw != null:
		estado = lw.get_status()
		if procurando and not lw.is_scanning():
			procurando = false
			_mostrar_salas()

		if estado != DESCONECTADO:
			meu_id = lw.get_my_id()
			lw.send_var([meu_pos.x, meu_pos.y])
			while lw.get_packet_count() > 0:
				var v = lw.get_var()
				var de = lw.get_packet_sender()
				if typeof(v) == TYPE_ARRAY and v.size() == 2:
					outros[de] = Vector2(v[0], v[1])

			tempo_lista += delta
			if tempo_lista > 1.0:
				tempo_lista = 0.0
				nomes.clear()
				for p in lw.get_players():
					nomes[p["id"]] = p["name"]
				for id in outros.keys():
					if not nomes.has(id):
						outros.erase(id) # saiu da sala
		elif outros.size() > 0:
			outros.clear()
			nomes.clear()
			msg = "Desconectado."

	var texto_estado = ["Fora de sala", "Host (dono da sala)", "Cliente"][estado]
	var jogadores = ""
	for id in nomes:
		jogadores += "%d:%s  " % [id, nomes[id]]
	get_node("info").set_text("%s   Eu: %d   FPS %d\nJogadores: %s\n%s" % [
			texto_estado, meu_id, OS.get_frames_per_second(), jogadores, msg])
	update()

func _cor(id):
	return cores[(id - 1) % cores.size()] if id > 0 else Color(1, 1, 1)

func _draw():
	var fonte = get_node("info").get_font("font")
	for id in outros:
		_quadrado(outros[id], _cor(id), str(id), fonte)
	var meu_id = lw.get_my_id() if lw != null else 0
	_quadrado(meu_pos, _cor(meu_id), "eu", fonte)

func _quadrado(pos, cor, texto, fonte):
	draw_rect(Rect2(pos - Vector2(8, 8), Vector2(16, 16)), cor)
	draw_string(fonte, pos + Vector2(-8, -12), texto, cor)
