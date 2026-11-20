# Bench de despacho

Quanto custa, por quadro, chamar N tarefas mínimas (`counter += 1`). Serve para decidir onde
mora o "task/sequence runner" do jogo.

## Modos

| Modo | O que mede |
|---|---|
| `a` | N nós com `_fixed_process`, despachados pela engine (`call_multilevel`) |
| `b` | laço em GDScript chamando `task.step(delta)` em N objetos de script (`Reference`, sem nó) |
| `c` | o mesmo laço com `task.call("step", delta)` |
| `d` | laço retomando N co-rotinas: `state = state.resume()` (cada retomada cria um `GDFunctionState`) |
| `vazio` | o mesmo laço `while` sem chamada: o custo do laço em si |

Detalhes da medição:
- Cada configuração tem 30 quadros de aquecimento e 120 medidos.
- No modo `a`, o tempo vai de um nó marcador antes dos N nós até outro depois deles.
- O tempo vem de `Platform3DS.get_ticks_usec()`, então só existe no 3DS. No PC o bench roda, mas sem µs.
- A tabela aparece na tela no fim; com `LR_VERBOSE=1` ela também sai no log, em linhas `BENCH`.

## Resultados no Azahar (relógio emulado; Old e New 3DS deram o mesmo)

| Modo | N=1 µs/quadro | N=10 µs/quadro | N=100 µs/quadro |
|---|---|---|---|
| a (nós) | 19,3 | 52,2 | 377,2 |
| vazio (laço) | 14,2 | 32,8 | 220,7 |
| b (`step()`) | 23,4 | 113,2 | 1016,5 |
| c (`call()`) | 26,7 | 138,0 | 1245,4 |
| d (`resume()`) | 50,1 | 364,7 | 3516,1 |

Linhas de base:
- `a` com N=0 custa 15,6 µs: são os dois marcadores.
- ~14 µs por quadro é a leitura do relógio pelo script.

Custo líquido por tarefa, com N=100 e descontada a base:

| Modo | Despacho | Laço | Total por tarefa |
|---|---|---|---|
| a (engine, nós) | ~3,6 µs | — (laço em C++) | **~3,6 µs** |
| b (`step()`) | ~8,0 µs | 2,2 µs | **~10,2 µs** |
| c (`call()`) | ~10,2 µs | 2,2 µs | **~12,5 µs** |
| d (`resume()`) | ~33 µs | 2,2 µs | **~35 µs** |

Pelas medições anteriores (CONTEXTO 5.2), no Old 3DS real o script custa ~4–5x o emulador. É preciso confirmar no console.

## No console

Exportar ou empacotar esta pasta e rodar. A tabela aparece em ~45 s, com "fim." no final.
