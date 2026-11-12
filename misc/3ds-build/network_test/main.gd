extends Node2D

# Teste de rede Wi-Fi/Internet (as classes normais do Godot: IP, HTTPClient, PacketPeerUDP).
# No 3DS só IPv4 e só HTTP (sem HTTPS). No Azahar a rede do PC é usada.

const PORTA_UDP = 4242

var linhas = []
var http = null
var http_etapa = ""
var http_corpo = RawArray()
var udp = null

func _ready():
	get_node("ip").connect("pressed", self, "_meu_ip")
	get_node("resolver").connect("pressed", self, "_resolver")
	get_node("http").connect("pressed", self, "_http_iniciar")
	get_node("udp").connect("pressed", self, "_udp_eco")
	get_node("limpar").connect("pressed", self, "_limpar")
	log_("Toque num botao na tela de baixo.")
	set_process(true)

func log_(texto):
	print(texto)
	linhas.append(texto)
	while linhas.size() > 12:
		linhas.remove(0)
	var t = ""
	for l in linhas:
		t += l + "\n"
	get_node("saida").set_text(t)

func _limpar():
	linhas.clear()
	log_("")

func _meu_ip():
	var ips = IP.get_local_addresses()
	log_("Meu IP: " + str(ips))

func _resolver():
	var t = OS.get_ticks_msec()
	var ip = IP.resolve_hostname("example.com")
	log_("example.com = %s (%d ms)" % [ip, OS.get_ticks_msec() - t])

func _http_iniciar():
	if http != null:
		log_("HTTP: ja em andamento")
		return
	http = HTTPClient.new()
	var err = http.connect("example.com", 80)
	if err != OK:
		log_("HTTP: erro ao conectar: %d" % err)
		http = null
		return
	http_etapa = "conectando"
	http_corpo = RawArray()
	log_("HTTP: conectando a example.com:80...")

func _http_passo():
	http.poll()
	var st = http.get_status()
	if http_etapa == "conectando":
		if st == HTTPClient.STATUS_CONNECTED:
			var headers = ["User-Agent: Godot3DS/1.0", "Accept: */*"]
			var err = http.request(HTTPClient.METHOD_GET, "/", headers)
			if err != OK:
				_http_fim("erro no pedido: %d" % err)
				return
			http_etapa = "pedindo"
			log_("HTTP: conectado, pedindo /")
		elif st != HTTPClient.STATUS_CONNECTING and st != HTTPClient.STATUS_RESOLVING:
			_http_fim("falhou ao conectar (status %d)" % st)
	elif http_etapa == "pedindo":
		if st == HTTPClient.STATUS_BODY or (st == HTTPClient.STATUS_CONNECTED and http.has_response()):
			log_("HTTP: resposta %d" % http.get_response_code())
			http_etapa = "lendo"
		elif st != HTTPClient.STATUS_REQUESTING:
			_http_fim("falhou no pedido (status %d)" % st)
	elif http_etapa == "lendo":
		if st == HTTPClient.STATUS_BODY:
			var pedaco = http.read_response_body_chunk()
			if pedaco.size() > 0:
				http_corpo.append_array(pedaco)
		else:
			var texto = http_corpo.get_string_from_utf8()
			var titulo = ""
			var i = texto.find("<title>")
			var j = texto.find("</title>")
			if i >= 0 and j > i:
				titulo = texto.substr(i + 7, j - i - 7)
			_http_fim("%d bytes, titulo: %s" % [http_corpo.size(), titulo])

func _http_fim(texto):
	log_("HTTP: " + texto)
	http.close()
	http = null
	http_etapa = ""

func _udp_eco():
	var ips = IP.get_local_addresses()
	if ips.size() == 0:
		log_("UDP: sem IP (Wi-Fi desligado?)")
		return
	if udp == null:
		udp = PacketPeerUDP.new()
		var err = udp.listen(PORTA_UDP)
		if err != OK:
			log_("UDP: erro ao escutar a porta %d: %d" % [PORTA_UDP, err])
			udp = null
			return
	udp.set_send_address(ips[0], PORTA_UDP)
	udp.put_packet(("ola do 3DS " + str(OS.get_ticks_msec())).to_utf8())
	log_("UDP: enviado para %s:%d" % [ips[0], PORTA_UDP])

func _process(delta):
	if http != null:
		_http_passo()
	if udp != null:
		while udp.get_available_packet_count() > 0:
			var p = udp.get_packet()
			log_("UDP: recebido de %s:%d: %s" % [udp.get_packet_ip(), udp.get_packet_port(), p.get_string_from_utf8()])
