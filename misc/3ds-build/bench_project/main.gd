extends Node2D

# Benchmark de CPU/GPU para comparar builds (-O3 / -O2 / -Os).
# N sprites quicando, movidos e girados por GDScript, + um laço de cálculo puro.
# Controles: A = liga/desliga turbo do New 3DS (804 <-> 268 MHz, simula Old 3DS)
#            Cima/Baixo = +/- 50 sprites
# "Script" = tempo gasto no _process (CPU pura do GDScript), a métrica principal.

const LARGURA = 400
const ALTURA = 240

var tex
var sprites = []
var vel = []
var plat = null

var quadros = 0
var acumulado = 0.0
var script_us = 0
var pior_ms = 0.0

func _ready():
	randomize()
	tex = load("res://icon.png")
	if Globals.has_singleton("Platform3DS"):
		plat = Globals.get_singleton("Platform3DS")
	set_process(true)
	set_process_input(true)
	ajustar(200)

func agora_us():
	if plat:
		return plat.get_ticks_usec()
	return OS.get_ticks_msec() * 1000 # PC: só milissegundos no 2.1

func ajustar(n):
	n = clamp(n, 0, 2000)
	while sprites.size() < n:
		var s = Sprite.new()
		s.set_texture(tex)
		s.set_scale(Vector2(0.5, 0.5))
		s.set_pos(Vector2(randf() * LARGURA, randf() * ALTURA))
		add_child(s)
		sprites.append(s)
		vel.append(Vector2(rand_range(-80, 80), rand_range(-80, 80)))
	while sprites.size() > n:
		var ultimo = sprites.size() - 1
		sprites[ultimo].queue_free()
		sprites.remove(ultimo)
		vel.remove(ultimo)
	move_child(get_node("Label"), get_child_count() - 1) # texto sempre por cima

func _process(delta):
	var t0 = agora_us()

	for i in range(sprites.size()):
		var s = sprites[i]
		var v = vel[i]
		var p = s.get_pos() + v * delta
		if p.x < 0 or p.x > LARGURA:
			v.x = -v.x
		if p.y < 0 or p.y > ALTURA:
			v.y = -v.y
		vel[i] = v
		s.set_pos(p)
		s.set_rot(s.get_rot() + delta)

	var x = 0.0
	for j in range(2000):
		x += sin(j * 0.01) * cos(j * 0.02)

	script_us += agora_us() - t0
	quadros += 1
	acumulado += delta
	pior_ms = max(pior_ms, delta * 1000.0)

	if acumulado >= 1.0:
		var turbo = "-"
		var build = "PC"
		if plat:
			build = plat.get_build_optimization()
			if plat.is_new_3ds():
				turbo = "ligado (804 MHz)" if plat.get_cpu_speedup() else "desligado (268 MHz)"
		var texto = "Build: %s   Sprites: %d\n" % [build, sprites.size()]
		texto += "FPS: %.1f   Quadro medio: %.1f ms   Pior: %.1f ms\n" % [quadros / acumulado, acumulado * 1000.0 / quadros, pior_ms]
		texto += "Script: %.2f ms/quadro\n" % (script_us / 1000.0 / quadros)
		texto += "Turbo: %s\n" % turbo
		texto += "A: turbo  Cima/Baixo: +/-50 sprites"
		get_node("Label").set_text(texto)
		print(texto.replace("\n", " | "))
		quadros = 0
		acumulado = 0.0
		script_us = 0
		pior_ms = 0.0

func _input(ev):
	if ev.type != InputEvent.JOYSTICK_BUTTON or not ev.pressed:
		return
	if ev.button_index == 1 and plat: # A
		plat.set_cpu_speedup(not plat.get_cpu_speedup())
	elif ev.button_index == 12: # cima
		ajustar(sprites.size() + 50)
	elif ev.button_index == 13: # baixo
		ajustar(sprites.size() - 50)
