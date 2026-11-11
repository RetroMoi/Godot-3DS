extends Node2D

# Cena de teste da Fase 0: um Sprite girando + Label com FPS, memória e entrada.
# Escreve as mesmas medições no stdout a cada segundo (visível via 3dslink -s ou no log do emulador).

var tempo = 0.0
var acumulado = 0.0

func _ready():
	set_process(true)
	print("Teste 3DS iniciado; video mode: ", OS.get_video_mode_size(), " janela: ", get_viewport_rect().size)

func _process(delta):
	tempo += delta
	get_node("Sprite").set_rot(tempo)

	var botoes = ""
	for i in range(16):
		if Input.is_joy_button_pressed(0, i):
			botoes += str(i) + " "

	var texto = "FPS: %d\nMem. estatica: %d KB\nPico: %d KB\nDinamica: %d KB\nBotoes: %s" % [
		OS.get_frames_per_second(),
		OS.get_static_memory_usage() / 1024,
		OS.get_static_memory_peak_usage() / 1024,
		OS.get_dynamic_memory_usage() / 1024,
		botoes]
	get_node("Label").set_text(texto)

	acumulado += delta
	if acumulado >= 1.0:
		acumulado = 0.0
		print(texto.replace("\n", " | "))
