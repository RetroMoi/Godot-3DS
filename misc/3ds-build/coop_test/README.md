# Teste Co-op Caverna

Referência de co-op local para 2 jogadores, usando `LocalWireless`: UDS no 3DS e UDP no PC.
O desenho completo está no cabeçalho de `main.gd`; o protocolo de pacotes fica em `net.gd`.

- Cada console decide o movimento e a vida do próprio player.
- O host decide o mundo: IA e vida dos inimigos, acertos e renascimento.
- No convidado, os inimigos são marionetes, interpoladas 100 ms no passado.
- Estado rápido 20x/s pelo canal sem garantia (`send`).
- Eventos pelo canal confiável e ordenado (`send_reliable`): hello/welcome com versão do protocolo, acerto, vida do inimigo.
- Regras do host: sem fogo amigo, uma vida cada, drops individuais; morrer devolve ao ponto de origem do mapa.
- Sair pede confirmação e encerra a sessão.

## Controles

| | 3DS | PC |
|---|---|---|
| Criar sala | A | Enter |
| Procurar | X | S |
| Entrar na 1ª sala | Y | J |
| Mover | direcional / Circle Pad | setas |
| Atacar | A | Enter |
| Sair (abre a confirmação) | B | Esc |

Os botões da tela de baixo também funcionam pelo toque. As ações `coop_*` estão no `engine.cfg`.

Atenção: os `ui_*` padrão do Godot 2.1 seguem o layout do Xbox. Por isso, no 3DS, `ui_accept` é o B e `ui_cancel` é o A.

## Diagnóstico

A tela de baixo mostra os contadores de `LocalWireless.get_stats()`:

```
rede id2: env 102 falha 0 rec 289 conf 2 ack 1 reenv 0 pend 0
```

| Campo | O que conta |
|---|---|
| `env` | pacotes enviados |
| `falha` | envios recusados pelo sistema |
| `rec` | pacotes recebidos (todos) |
| `conf` | pacotes confiáveis recebidos |
| `ack` | confirmações recebidas |
| `reenv` | reenvios |
| `pend` | pacotes esperando confirmação |

Se um teste entre consoles falhar, anote essa linha nos dois aparelhos.

No canto da tela de cima aparecem duas linhas de desempenho, atualizadas 4 vezes por segundo:

```
FPS 60 | quadro 2.1 ms (lóg 1.2 des 0.9 GPU 0.0) pior 6.9
rede 0.16 ms/quadro (pior 0.32) | 0.8 chamadas/quadro
```

| Campo | O que mede |
|---|---|
| `quadro` | tempo de trabalho do quadro, sem a espera do VBlank (60 FPS = até 16,7 ms) |
| `lóg` | scripts, física e rede |
| `des` | montar os comandos da GPU |
| `GPU` | espera pela GPU |
| `pior` | o pior quadro do último segundo |
| `rede` | tempo gasto dentro do `LocalWireless` por quadro, chamadas ao sistema incluídas (`get_stats`: `net_ms`, `net_max_ms`) |
| `chamadas` | pedidos ao serviço UDS por quadro (`calls_per_frame`) |

Para investigar FPS baixo, anote as duas linhas em três situações: no lobby (sem rede), como host sozinho e com os dois na sala (nos dois papéis).

## Testar no PC com duas janelas

```sh
cd misc/3ds-build/coop_test
COOP_AUTO=host COOP_BOT=1 COOP_WINPOS=0,0 godot -path . &
COOP_AUTO=join COOP_BOT=1 COOP_WINPOS=450,0 COOP_LOSS=0.2 COOP_LATENCY=60 godot -path . &
```

- `COOP_AUTO=host|join`: cria a sala ou entra na primeira que achar.
- `COOP_BOT=1`: o player anda e ataca sozinho.
- `COOP_LOSS` (0–1) e `COOP_LATENCY` (ms): simulam perda e atraso dos pacotes.
- `COOP_WINPOS`: posição da janela.

O PC usa UDP na porta 27850 e procura salas em 127.0.0.1. Para jogar entre dois PCs, adicione o IP do host antes de procurar: `LocalWireless.add_scan_address("192.168.0.10")`.

## Testar no 3DS / emulador

```sh
misc/3ds-build/emu_test/run3ds.sh misc/3ds-build/coop_test 380 <saída> roteiro.txt
```

No Old 3DS emulado roda a 60 FPS como host. Com dois consoles, um cria a sala e o outro procura e entra.
