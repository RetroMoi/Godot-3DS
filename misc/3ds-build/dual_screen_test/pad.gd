extends Panel

# Área de desenho na tela de baixo: no 3DS o toque chega aqui como mouse (botão esquerdo),
# em coordenadas locais do Control.

var pontos = []

func _input_event(ev):
	var desenhando = (ev.type == InputEvent.MOUSE_BUTTON and ev.pressed) or (ev.type == InputEvent.MOUSE_MOTION and (ev.button_mask & 1))
	if desenhando:
		pontos.append(Vector2(ev.x, ev.y))
		if pontos.size() > 300:
			pontos.remove(0)
		update()

func limpar():
	pontos.clear()
	update()

func _draw():
	for p in pontos:
		draw_circle(p, 2, Color(1, 0.8, 0.2))
