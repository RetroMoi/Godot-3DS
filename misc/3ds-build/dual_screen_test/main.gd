extends Node2D

# Teste do modo duas telas (display/3ds_dual_screen = true, área 400x480):
# - tela de cima (y 0-240): informações e um ícone que anda com o direcional / Circle Pad;
# - tela de baixo (x 40-360, y 240-480): botões, slider, lista rolável e área de desenho,
#   tudo usado pelo toque (que no 3DS também vira clique de mouse).
# No PC as faixas ao lado da tela de baixo ficam pretas, como no console.

var contador = 0
var item = "-"
var toque = "-"

func _ready():
	get_node("mais").connect("pressed", self, "_on_mais")
	get_node("zerar").connect("pressed", self, "_on_zerar")
	var lista = get_node("rolagem/lista")
	for i in range(20):
		var b = Button.new()
		b.set_text("Item %d" % (i + 1))
		b.connect("pressed", self, "_on_item", [i + 1])
		lista.add_child(b)
	set_process(true)
	set_process_input(true)

func _on_mais():
	contador += 1

func _on_zerar():
	contador = 0
	get_node("pad").limpar()

func _on_item(n):
	item = str(n)

func _input(ev):
	if ev.type == InputEvent.SCREEN_TOUCH or ev.type == InputEvent.SCREEN_DRAG:
		toque = "%d,%d" % [ev.x, ev.y]

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
	var icone = get_node("icone")
	var p = icone.get_pos() + dir * 150 * delta
	icone.set_pos(Vector2(clamp(p.x, 32, 368), clamp(p.y, 32, 208)))

	var mouse = get_viewport().get_mouse_pos()
	get_node("info").set_text("FPS %d   Contador: %d   Slider: %d\nItem: %s   Toque: %s   Mouse: %d,%d" % [
			OS.get_frames_per_second(), contador, get_node("slider").get_val(), item, toque, mouse.x, mouse.y])
