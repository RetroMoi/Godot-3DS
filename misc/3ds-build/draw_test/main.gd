extends Node2D

# Teste das primitivas 2D do renderizador do 3DS. Rode a mesma cena no editor (PC) e no 3DS:
# as duas telas devem ficar iguais.
#
# Linha de cima : linhas de 1, 3 e 6 px, linha com gradiente, círculo pulsando,
#                 estrela (polígono côncavo) e hexágono com uma cor por vértice.
# Meio          : textura 100x60 (não é potência de 2) desenhada com draw_texture à esquerda e
#                 como polígono com UV ao lado (devem ser idênticas); Polygon2D girando.
# Embaixo       : draw_primitive com 1 ponto (quadradinho), 3 (triângulo colorido) e
#                 4 (quadrilátero texturizado); texto cortado pela moldura (clip);
#                 faixa com a textura repetida (tile) e faixa com o ícone repetido.

var textura = preload("res://textura.png")
var icone = preload("res://icon.png")
var t = 0.0

func _ready():
	set_process(true)

func _process(delta):
	t += delta
	get_node("poly").set_rot(t)
	get_node("fps").set_text("FPS %d" % OS.get_frames_per_second())
	update()

func _draw():
	# --- Linhas ---
	draw_line(Vector2(10, 10), Vector2(120, 10), Color(1, 1, 1))
	draw_line(Vector2(10, 20), Vector2(120, 48), Color(0, 1, 0), 3)
	draw_line(Vector2(132, 8), Vector2(132, 62), Color(0, 0.6, 1), 6)
	draw_primitive(Vector2Array([Vector2(10, 60), Vector2(120, 60)]),
			ColorArray([Color(1, 0, 0), Color(0, 0, 1)]), Vector2Array(), null, 4)

	# --- Círculo ---
	draw_circle(Vector2(175, 35), 20 + 5 * sin(t * 3), Color(1, 0.2, 0.2))

	# --- Estrela côncava, cor única ---
	var estrela = Vector2Array()
	for i in range(10):
		var r = 26
		if i % 2 == 1:
			r = 11
		var a = PI * 2 * i / 10 - PI / 2
		estrela.push_back(Vector2(240, 36) + Vector2(cos(a), sin(a)) * r)
	draw_colored_polygon(estrela, Color(1, 0.9, 0.1))

	# --- Hexágono, uma cor por vértice ---
	var hexa = Vector2Array()
	var cores = ColorArray([Color(1, 0, 0), Color(1, 1, 0), Color(0, 1, 0), Color(0, 1, 1), Color(0, 0, 1), Color(1, 0, 1)])
	for i in range(6):
		var a = PI * 2 * i / 6 + t
		hexa.push_back(Vector2(310, 35) + Vector2(cos(a), sin(a)) * 26)
	draw_polygon(hexa, cores)

	# --- Textura não potência de 2: draw_texture x polígono com UV (devem ser iguais) ---
	draw_texture(textura, Vector2(10, 78))
	var ret = Vector2Array([Vector2(120, 78), Vector2(220, 78), Vector2(220, 138), Vector2(120, 138)])
	var uv = Vector2Array([Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1)])
	draw_polygon(ret, ColorArray([Color(1, 1, 1)]), uv, textura)

	# --- draw_primitive: 1, 3 e 4 pontos ---
	draw_primitive(Vector2Array([Vector2(20, 175)]), ColorArray([Color(1, 1, 1)]), Vector2Array(), null, 6)
	draw_primitive(Vector2Array([Vector2(70, 150), Vector2(105, 205), Vector2(35, 205)]),
			ColorArray([Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1)]), Vector2Array())
	draw_primitive(Vector2Array([Vector2(120, 150), Vector2(200, 155), Vector2(195, 205), Vector2(125, 200)]),
			ColorArray([Color(1, 1, 1), Color(1, 1, 1), Color(1, 1, 1), Color(1, 1, 1)]),
			Vector2Array([Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1)]), textura)

	# --- Textura repetida (tile): 100x60 (repetição feita à mão) e ícone 64x64 (pelo hardware) ---
	draw_texture_rect(textura, Rect2(130, 208, 125, 28), true)
	draw_texture_rect(icone, Rect2(265, 204, 130, 32), true)

	# --- Moldura do teste de clip (o texto do Label "clip" não pode sair dela) ---
	var c = Color(1, 0.5, 0)
	draw_line(Vector2(229, 179), Vector2(331, 179), c)
	draw_line(Vector2(331, 179), Vector2(331, 197), c)
	draw_line(Vector2(331, 197), Vector2(229, 197), c)
	draw_line(Vector2(229, 197), Vector2(229, 179), c)
