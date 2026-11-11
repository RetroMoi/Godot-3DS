extends Node2D

# Cena de teste: Sprite girando + Label com FPS, memória real do 3DS e botões.
# As mesmas medições vão para o stdout a cada segundo (log do Azahar ou 3dslink -s).

var tempo = 0.0
var acumulado = 0.0
var impresso = 0.0
var plat = null
var toque = "-"

func _ready():
	if Globals.has_singleton("Platform3DS"):
		plat = Globals.get_singleton("Platform3DS")
	set_process(true)
	set_process_input(true)
	print("Teste 3DS iniciado; video mode: ", OS.get_video_mode_size(), " viewport: ", get_viewport_rect().size)

func _input(ev):
	if ev.type == InputEvent.SCREEN_TOUCH:
		toque = ("%d,%d" % [ev.x, ev.y]) if ev.pressed else "-"
	elif ev.type == InputEvent.SCREEN_DRAG:
		toque = "%d,%d (arrasto)" % [ev.x, ev.y]

func eixo(i):
	return "%.2f" % Input.get_joy_axis(0, i)

func kb(bytes):
	return str(bytes / 1024) + " KB"

func mb(bytes):
	return "%.1f MB" % (bytes / 1048576.0)

func _process(delta):
	tempo += delta
	get_node("Sprite").set_rot(tempo)

	acumulado += delta
	if acumulado < 0.1:
		return
	acumulado = 0.0

	var botoes = ""
	for i in range(16):
		if Input.is_joy_button_pressed(0, i):
			botoes += str(i) + " "

	var texto = "FPS: %d\n" % OS.get_frames_per_second()
	if plat:
		var m = plat.get_memory_info()
		texto += "Modelo: %s\n" % ("New 3DS" if m["is_new_3ds"] else "Old 3DS/2DS")
		texto += "Heap: %s / %s (pico %s)\n" % [mb(m["heap_used"]), mb(m["heap_size"]), mb(m["heap_peak"])]
		texto += "Linear: %s / %s (pico %s)\n" % [mb(m["linear_used"]), mb(m["linear_size"]), mb(m["linear_peak"])]
		texto += "VRAM: %s / %s (pico %s)\n" % [mb(m["vram_used"]), mb(m["vram_size"]), mb(m["vram_peak"])]
		texto += "Livre p/ jogo: %s (heap)\n" % mb(m["heap_size"] - m["heap_used"])
		texto += "Total do app: %s\n" % mb(m["app_region_used"])
	else:
		texto += "Mem. estatica: %s\n" % kb(OS.get_static_memory_usage())
	texto += "Pool dinamico: %s\n" % kb(OS.get_dynamic_memory_usage())
	texto += "Botoes: %s\n" % botoes
	texto += "Circle: %s,%s  C-Stick: %s,%s\n" % [eixo(0), eixo(1), eixo(2), eixo(3)]
	texto += "Toque: %s" % toque
	get_node("Label").set_text(texto)
	impresso += 0.1
	if impresso >= 1.0:
		impresso = 0.0
		print(texto.replace("\n", " | "))
