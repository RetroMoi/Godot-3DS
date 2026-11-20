extends Reference
# Tarefa sem nó (modos b, c, d): o trabalho é mínimo, para medir só o despacho.

var counter = 0

func step(delta):
	counter += 1

# Modo d: co-rotina. Cada resume() roda até o próximo yield() e devolve um novo GDFunctionState.
func run():
	while true:
		counter += 1
		yield()
