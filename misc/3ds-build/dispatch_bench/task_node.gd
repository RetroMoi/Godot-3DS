extends Node
# Modo a: uma tarefa = um nó no grupo de _fixed_process (despachado pela engine).

var counter = 0

func _ready():
	set_fixed_process(true)

func _fixed_process(delta):
	counter += 1
