extends Node2D

# Teste do stretch_mode="viewport" no 3DS: o jogo é 200x240 (metade de 400x480) e cada pixel
# vira um bloco 2x2. A tela de cima mostra y 0-120; a de baixo, o centro x 20-180, y 120-240.
#
# Marcadores (devem aparecer assim, sem espelhar):
#   tela de cima:  vermelho no canto superior esquerdo, verde no superior direito,
#                  azul no inferior esquerdo, um "L" branco no meio
#   tela de baixo: amarelo no canto superior esquerdo, magenta no inferior direito;
#                  a faixa ciano (x 0-20) e a laranja (x 180-200) NÃO aparecem
# A camada "so_baixo" (Platform3DS.SCREEN_BOTTOM) cobre o jogo todo com um véu roxo: ele só
# deve aparecer na tela de baixo.

var touch = Vector2(-10, -10)

func _ready():
	if Globals.has_singleton("Platform3DS"):
		var plat = Globals.get_singleton("Platform3DS")
		plat.set_canvas_layer_screens(1, plat.SCREEN_BOTTOM)
	var layer = CanvasLayer.new()
	layer.set_layer(1)
	layer.set_name("so_baixo")
	var veil = ColorFrame.new()
	veil.set_frame_color(Color(0.6, 0.2, 0.9, 0.35))
	veil.set_pos(Vector2(0, 0))
	veil.set_size(Vector2(200, 240))
	veil.set_ignore_mouse(true)
	layer.add_child(veil)
	add_child(layer)
	set_process(true)
	set_process_input(true)

func _input(ev):
	if ev.type == InputEvent.SCREEN_TOUCH or ev.type == InputEvent.MOUSE_BUTTON:
		touch = ev.pos if ev.type == InputEvent.MOUSE_BUTTON else Vector2(ev.x, ev.y)
		update()

func _process(delta):
	get_node("info").set_text("FPS %d  toque %d,%d" % [OS.get_frames_per_second(), touch.x, touch.y])

func _draw():
	draw_rect(Rect2(0, 0, 200, 240), Color(0.12, 0.12, 0.16))
	draw_rect(Rect2(0, 0, 10, 10), Color(1, 0, 0))        # cima, canto sup. esq.
	draw_rect(Rect2(190, 0, 10, 10), Color(0, 1, 0))      # cima, canto sup. dir.
	draw_rect(Rect2(0, 110, 10, 10), Color(0, 0.3, 1))    # cima, canto inf. esq.
	draw_rect(Rect2(90, 40, 4, 30), Color(1, 1, 1))       # "L"
	draw_rect(Rect2(90, 66, 16, 4), Color(1, 1, 1))
	draw_rect(Rect2(0, 120, 20, 120), Color(0, 1, 1))     # faixa escondida (esq.)
	draw_rect(Rect2(180, 120, 20, 120), Color(1, 0.5, 0)) # faixa escondida (dir.)
	draw_rect(Rect2(20, 120, 10, 10), Color(1, 1, 0))     # baixo, canto sup. esq.
	draw_rect(Rect2(170, 230, 10, 10), Color(1, 0, 1))    # baixo, canto inf. dir.
	draw_rect(Rect2(touch.x - 2, touch.y - 2, 4, 4), Color(1, 1, 1))
