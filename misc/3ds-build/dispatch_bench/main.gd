extends Node

# Bench de despacho por quadro: quanto custa chamar N tarefas mínimas (counter += 1).
#   a) N nós com _fixed_process (a engine despacha: call_multilevel)
#   b) laço chamando task.step(delta) em N objetos de script (Reference)
#   c) laço chamando task.call("step", delta)
#   d) laço retomando N co-rotinas: state = state.resume() (cada retomada cria um GDFunctionState)
#   vazio) o mesmo laço sem chamada (custo do laço em si)
# Cada configuração: WARMUP quadros de aquecimento, depois MEASURE quadros medidos.
# Resultado: µs por quadro (média e pior) e µs por tarefa. Tempo pelo Platform3DS (só no 3DS);
# no PC o bench roda, mas sem µs (OS.get_ticks_usec não existe para scripts no 2.1).

const WARMUP = 30
const MEASURE = 120
const TaskScript = preload("res://task.gd")
const TaskNodeScript = preload("res://task_node.gd")
const MarkerScript = preload("res://marker.gd")

var configs = []
var ci = -1
var frame = 0
var tasks = []
var states = []
var nodes = []
var n = 0
var mode = ""
var sum = 0
var worst = 0
var t_begin = 0
var results = []
var plat = null

func _ready():
	if Globals.has_singleton("Platform3DS"):
		plat = Globals.get_singleton("Platform3DS")
	for N in [0, 1, 10, 100]:
		configs.append(["a", N])
	for m in ["vazio", "b", "c", "d"]:
		for N in [1, 10, 100]:
			configs.append([m, N])
	_next()
	set_fixed_process(true)

func _now():
	if plat != null:
		return plat.get_ticks_usec()
	return OS.get_ticks_msec() * 1000

func _clear():
	for x in nodes:
		x.queue_free()
	nodes = []
	tasks = []
	states = []

func _next():
	if ci >= 0:
		var avg = float(sum) / MEASURE
		var per = avg / n if n > 0 else 0.0
		results.append([mode, n, avg, worst, per])
		print("BENCH %s N=%d: %.1f us/quadro (pior %d), %.2f us/tarefa" % [mode, n, avg, worst, per])
	_clear()
	ci += 1
	frame = 0
	sum = 0
	worst = 0
	if ci >= configs.size():
		_show(true)
		set_fixed_process(false)
		return
	mode = configs[ci][0]
	n = configs[ci][1]
	if mode == "a":
		var b = Node.new()
		b.set_script(MarkerScript)
		b.bench = self
		add_child(b)
		nodes.append(b)
		for i in range(n):
			var t = Node.new()
			t.set_script(TaskNodeScript)
			add_child(t)
			nodes.append(t)
		var e = Node.new()
		e.set_script(MarkerScript)
		e.bench = self
		e.is_end = true
		add_child(e)
		nodes.append(e)
	else:
		for i in range(n):
			var t = TaskScript.new()
			tasks.append(t)
			if mode == "d":
				states.append(t.run())
	_show(false)

# Modo a: chamado pelos marcadores, na ordem da árvore.
func mark(is_end):
	if not is_end:
		t_begin = _now()
	else:
		_record(_now() - t_begin)

func _record(us):
	frame += 1
	if frame > WARMUP:
		sum += us
		if us > worst:
			worst = us
	if frame >= WARMUP + MEASURE:
		_next()

func _fixed_process(delta):
	if mode == "a" or ci >= configs.size():
		return
	var i = 0
	var t0 = _now()
	if mode == "vazio":
		while i < n:
			i += 1
	elif mode == "b":
		while i < n:
			tasks[i].step(delta)
			i += 1
	elif mode == "c":
		while i < n:
			tasks[i].call("step", delta)
			i += 1
	elif mode == "d":
		while i < n:
			states[i] = states[i].resume()
			i += 1
	_record(_now() - t0)

func _show(done):
	var text = "Bench de despacho (%s)\n" % ("3DS" if plat != null else "PC: sem µs")
	text += "modo  N    us/quadro  pior   us/tarefa\n"
	for r in results:
		text += "%-5s %-4d %8.1f %6d %8.2f\n" % r
	if not done:
		text += "\nmedindo %s N=%d..." % [mode, n]
	else:
		text += "\nfim."
	get_node("out").set_text(text)
