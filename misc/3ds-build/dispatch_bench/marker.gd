extends Node
# Modo a: marcador antes (begin) e depois (end) dos N nós. O tempo entre os dois é o despacho
# dos N nós pela engine (mais o despacho do próprio marcador "end").

var bench = null
var is_end = false

func _ready():
	set_fixed_process(true)

func _fixed_process(delta):
	bench.mark(is_end)
