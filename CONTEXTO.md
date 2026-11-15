# Contexto do projeto: Godot 2.1 para Nintendo 3DS + porte do ARPG (v3)

Documento de passagem de contexto, para continuar o trabalho com qualquer assistente de IA.
Estado em 14/11/2026 (engine em `9535e74`). Substitui a v2 (`contexto_godot3ds_arpg_v2.md`, fora do repositório).
Idioma de trabalho: **português do Brasil** (responder sempre em PT-BR).

**Onde fica:** este arquivo é o `CONTEXTO.md` na raiz do repositório **Godot-3DS** (`main/2.1`).
Atualizar e fazer commit dele a cada etapa relevante. O CI ignora pushes só de `.md`, então isso
não dispara build. O jogo tem o seu próprio `CONTEXTO.md` no ARPG-3DS, mantido pelo Open.

---

## 0. Divisão de trabalho (desde a v1)

- **Engine (Godot-3DS):** trabalhada com o Claude (Claude Code).
- **Jogo (ARPG-3DS):** desenvolvido pelo usuário com o **OpenCode**, que o usuário chama de **"Open"**. Para o Claude, o repositório do jogo é **somente leitura**: ler, testar, medir e apontar problemas (arquivo, linha, sugestão). Quem altera é sempre o Open; o Claude não faz commits nem oferece editar o ARPG-3DS.
- Quando o Open apontar um gargalo, a pergunta é sempre: **é do jogo (scripts, cenas, conversor) ou da engine?** Se for da engine, o Claude corrige.

---

## 1. Visão geral

São dois projetos ligados:

| Projeto | Repositório | Branch | O que é |
|---|---|---|---|
| **Godot-3DS** (engine + editor) | `RetroMoi/Godot-3DS` | `main/2.1` | Fork do Godot 2.1 (base: Homebrodot/Godot) que exporta jogos 2D para Nintendo 3DS (`.3dsx` e `.cia`). |
| **ARPG-3DS** (jogo) | `RetroMoi/ARPG-3DS` | `main` | Porte do ARPG do usuário (original em Godot 4, repo `RetroMoi/ARPG`) para Godot 2.1/3DS. É o caso real que guia e testa a engine. |

**Metas de hardware:** 60 FPS no New 3DS e no Old 3DS.
- Old 3DS: ARM11 a 268 MHz, sem cache L2, cerca de 64 MB de RAM para o app.
- New 3DS: 804 MHz, com cache L2.

**Onde se testa:** no emulador **Azahar** e nos consoles reais do usuário (New 3DS e Old 3DS, com Luma3DS).

---

## 2. Regras e preferências do usuário

- Responder em **português**.
- **Commits:**
  - Terminar a mensagem com as linhas de atribuição pedidas pelo ambiente.
  - **Nunca** colocar identificador de modelo de IA em commits, PRs, comentários de código ou arquivos do repositório.
  - No ARPG-3DS, o autor é `RetroMoi`: `git -c user.name="RetroMoi" -c user.email="noreply@users.noreply.github.com" commit`.
- **PRs e push:**
  - Não criar PR sem pedido explícito.
  - Fazer push dos commits com `git push -u origin <branch>`.
  - O usuário às vezes sobe arquivos pelo GitHub web, então sempre fazer `git fetch` e rebase antes do push.
- **Porte:** não precisa ser 1:1 com o Godot 4. Projetar para o 3DS desde o início, com orçamento de desempenho.
- **Método:**
  - Usar a melhor técnica, sem exagero.
  - **Medir antes de otimizar.**
  - Dizer o que foi validado (PC, emulador, console) e o que não foi.
- Quando o usuário disser "só vamos debater", **não alterar nada**.
- **Contexto:** manter este `CONTEXTO.md` (raiz do Godot-3DS) atualizado e versionado.
- Não ajudar com pirataria ou DRM, nem recriar jogos comerciais.
- O e-mail do usuário é privado.

---

## 3. Godot-3DS: arquitetura e estado

### 3.1 Estrutura relevante

- **`platform/3ds/`:**
  - `os_3ds.*`: OS, laço principal, entrada, heaps, gancho do APT.
  - `platform_3ds.*`: singleton `Platform3DS`.
  - `local_wireless_3ds.*`: singleton `LocalWireless`, sobre UDS.
  - `network_3ds.*`: Wi-Fi e sockets.
  - `detect.py`: flags de build; define `_3DS` e `ARM11`; desliga módulos.
  - `export/`: exportador do editor.
- **`drivers/3ds/`:**
  - `citro3d/rasterizer_citro3d.*`: renderizador 2D sobre citro3d/PICA200.
  - `citro3d/shaders/2d.pica`: shader de vértice 2D.
  - `audio_driver_3ds.*`: NDSP.
  - `audio_server_3ds.*`: efeitos sonoros no DSP.
  - `thread_3ds.*`: threads, mutex `LightLock`, semáforo `LightSemaphore`.
- **`scene/2d/`:**
  - `tile_map.*`: Y-sort em faixas, `y_sort_origin`, `clip_uv`, `get_collision_shapes`.
  - `collision_grid_2d.*`: colisão de mundo sem física.
- **`misc/3ds-build/`:**
  - `PLANO.md`: plano por fases.
  - `COMPILAR.md`: guia de build para Windows/WSL.
  - `build.sh` e `Dockerfile`.
  - Projetos de teste: `test_project`, `draw_test`, `dual_screen_test`, `local_wireless_test`, `network_test`, `bench_project`.
  - `cia/`: RSF e makerom.
  - `emu_test/`: harness de teste no emulador.
- **`.github/workflows/godot-3ds.yml`:** CI que gera os templates 3DS, os editores Windows/Linux e o pacote portátil. Ignora pushes que só mexem em `.md`.

### 3.2 Plano (PLANO.md), em resumo

| Fase | Conteúdo | Estado |
|---|---|---|
| 0–2 | Ambiente Docker/devkitARM, template, exportador `.3dsx`/`.cia` pelo editor | Feito |
| 1.5 | "GDScript 2.1+": açúcar sintático e backports leves | Planejado |
| 3 | Assets e validações no editor | Planejado |
| 4 | Desempenho | Muito avançado (ver 3.4) |
| 5 | Duas telas + toque | Validado |
| 6 | Rede: LocalWireless (UDS) | Validado em 2 consoles |
| 6 | Rede: Wi-Fi (IP, TCP, UDP, HTTPClient) | Implementado; teste do usuário pendente |
| 7 | StreetPass | Experimental, por último |

### 3.3 Editor (UI): feito

- Nas Configurações do Projeto, a caixinha de persistência virou botão de reverter.
- Plataformas sem uso ficam ocultas; sobram 3DS, Windows, Linux e macOS.
- O 3D vem oculto por padrão (Configurações do Editor → 3ds → Show 3D).
- Inspector no estilo do Godot 4:
  - campos numéricos tipo `EditorSpinSlider`;
  - Vector2, Rect2 e Vector3 com campos na mesma linha.
- Guias das duas telas no canvas do editor.
- **Layout padrão dos docks:** Cena (em cima) e Arquivos (embaixo) à esquerda; Inspetor e Node (sinais/grupos) em abas à direita, na altura toda. Vale para projetos sem layout salvo e para Editor → Layout do Editor → Padrão. Em telas com menos de 1200 px de largura, fica a coluna única original.
  - O layout que o usuário ajusta fica por projeto em `editor_data/`. Desde o ARPG-3DS `4155e91`, o `sync-engine.ps1` preserva o `editor_data/` (só `templates/` e `tmp/` vêm do pacote novo; um `editor_data.keep` interrompido é restaurado na execução seguinte). Testado pelo Open com o run #31 (`9535e746ce`): preferências mantidas, templates trocados, exportação com `build 9535e746ce, editor build 9535e746ce`.
- **Filtro de imagem do projeto** (`image_loader/filter`, `gen_mipmaps`, `repeat`, nas Configurações do Projeto): é a configuração padrão de todos os gráficos. Vale para os PNGs carregados direto, para o **splash de boot** e para o padrão de "Filter" nos diálogos de importação de textura (2D, atlas, grande, 3D). Desligado = nearest, para pixel art (commit `fd4f4a6`).

### 3.4 Engine/renderizador 3DS: pontos técnicos

**citro3d/PICA200:**
- Sem shaders de fragmento; só combinadores TEV.
- Floats de 24 bits.
- Framebuffers girados (`Mtx_OrthoTilt`).
- Buffer de comandos por quadro.

**Duas telas:**
- Área de jogo de 400x480 (`display/3ds_dual_screen`).
- Tela de cima: y de 0 a 240. Tela de baixo: x de 40 a 360, y de 240 a 480.
- Tela por camada: `Platform3DS.set_canvas_layer_screens(layer, SCREEN_TOP|SCREEN_BOTTOM|SCREEN_BOTH)`.
- O toque na tela de baixo vira mouse e `SCREEN_TOUCH`.

**Batching 2D:**
- Quadrados com a mesma textura e cor saem num único desenho.
- Cache de estado de textura.
- Buffer de comandos de 1 MB, com guarda em 95%.
- Anel de vértices de 1 MB.

**Pixel snap:** itens sem rotação são desenhados em pixel inteiro. Com escala inteira, a origem alinha à grade da escala.

**Geometria guardada (`6ee70a6`):**
- Itens só de retângulos guardam os vértices entre quadros.
- Esses vértices só são remontados quando os comandos do item mudam (`commands_serial` + `rasterizer_cache`).
- Formato dos vértices: 8 bytes em ponto fixo de 16 bits, mais o uniform `vscale`.
- Os quadrados usam um buffer de índices compartilhado.
- No emulador, o desenho caiu de 5,6 para 2,2 ms.

**Fim de quadro (`2c8d70c`):**
- Flush só dos vértices do quadro, seguido de `C3D_FrameEnd(... | GX_CMDLIST_FLUSH)`.
- Troca de buffers só quando a GPU termina, via `C3D_FrameDrawOn`. Isso corrigiu os rasgos ao rolar a tela no Old 3DS.

**Laço principal (`e58cbd7`):**
- `physics/max_steps_per_frame` vale 2 no 3DS e 8 no PC.
- Sem física 3D nem som 3D por quadro (`disable_3d`).
- Sem os wrappers multi-thread (WrapMT) no 3DS.

**Áudio (`a2ba33e`):**
- `AudioServer3DS` toca cada voz num canal do DSP (canais 0 e 2–23).
- ADPCM cai no mixer de software.
- O mixer de software (música) roda numa thread no **núcleo 1** (`APT_SetAppCpuTimeLimit(30)`).

**TileMap:**
- Y-sort em faixas por linha (quadrante 16x1), com pré-culling.
- `cell/y_sort_origin` desloca só o ponto de ordenação. Corrigiu o player piscando na frente e atrás das árvores.
- Com `collision/layers = 0` e `collision/mask = 0`, o TileMap não cria corpos de física.
- `get_collision_shapes()` exporta as formas na mesma posição dos corpos.
- **`cell/clip_uv`** (padrão: ligado; commit `dcb1fec`):
  - Cada tile tem o UV preso ao seu retângulo no atlas, com meio texel de margem.
  - Corrige as linhas dos tiles vizinhos no **PC** em escala fracionária (stretch 1,6 × zoom 2), que apareciam ao correr na vertical.
  - Implementação: `CANVAS_RECT_CLIP_UV = 32`; `draw_rect_region(..., transpose, clip_uv)`; no GLES2, `USE_CLIP_RECT_UV` + `clamp` no shader.
  - No 3DS não muda nada (escala inteira).

**`CollisionGrid2D` (`2fc6efd`, `6b3b272`):**
- API: `add_tilemap`, `add_rect`, `build`, `move(box, motion, corner_slide)`, `collides`, `segment_blocked`, `get_rects_in`.
- Bate ponto a ponto com a física antiga: 33.600 de 33.600 pontos.

**Estatísticas:**
- `Platform3DS.get_frame_stats()` → `frame_ms`, `logic_ms`, `draw_ms`, `frame_end_ms`, `gpu_wait_ms`, `vblank_ms`.
- Pior quadro do último segundo: `frame_max_ms`, `logic_max_ms`, `draw_max_ms`.
- Contadores: `draw_calls`, `vertices`, `items`, `cmdbuf_percent`.
- **O que é "lógica":** o tempo do quadro menos GPU, desenho e VBlank. Inclui scripts, física, animação, áudio de software e o HUD. **O Y-sort conta como desenho**, não como lógica.

**Build id (`031d489`):**
- O build grava o commit em `core/version_hash.gen.h` (de `BUILD_HASH`, `GITHUB_SHA` ou `git`; só reescreve se mudar).
- O template do 3DS embute o id (`Platform3DS.get_build_id()`, legível pelos scripts).
- O exportador imprime qual template usou e **recusa** template sem id ou de outro build, dizendo onde o achou. Um template custom no preset passa direto.

**Outros:**
- `Semaphore3ds` real.
- Heap linear de 12 MB.
- `user://` fica no cartão SD.

### 3.5 Gotchas do Godot 2.1

- **O que não existe:** `is_action_just_pressed`, `create_timer`/`await`, `match`, tipagem estática.
- **Nomes e APIs diferentes:**
  - `StringArray` não tem `find`.
  - `ColorRect` se chama **`ColorFrame`** (`set_frame_color`).
- **`Vector2.angle()`** é `atan2(x, y)`; melhor evitar.
- **Formas de colisão:**
  - `CollisionShape2D` só serve ao editor; as formas ficam no pai.
  - Na cápsula, a altura não inclui as pontas.
  - A forma não tem `disabled`: usar `monitorable`/`monitoring`.
  - `ConvexPolygonShape2D` exige os pontos na ordem certa.
- **Tempo:** `_process` recebe o delta escalado. Para tempo real, usar `OS.get_ticks_msec()`.
- **Camera2D:** vem com zona morta ligada.
- **Entrada:**
  - `Input.is_action_pressed("nome")` converte String em StringName a cada chamada; evitar por quadro.
  - Para injetar ações: `InputEvent.set_as_action` + `Input.parse_input_event`.
- **Joystick do 3DS:**
  - Rosto e ombros: 0=B, 1=A, 2=Y, 3=X, 4=L, 5=R, 6=ZL, 7=ZR.
  - Sistema: 10=SELECT, 11=START.
  - D-pad: 12–15.
- **`TileMap.cell_size`** é `Size2i`; cuidado ao misturar com Vector2.
- **Autoload de script:** `[autoload] Nome="*res://caminho.gd"`.
- **Não existe `is_instance_valid`.** Para valor que pode ser `null`: `if x != null:`. Para nó que pode ter sido liberado (`queue_free`), guardar `weakref(no)` e usar `ref.get_ref()` com checagem. Chamar método num **objeto já liberado** ainda derruba o release (só valores não-objeto viram erro de script, ver 5.6).

### 3.6 Build

**Template 3DS (Docker):**

```
docker run --rm -v $PWD:/src godot-3ds-build bash -c "scons -j4 platform=3ds target=release tools=no disable_3d=yes optimize=speed"
```

Se o daemon do Docker cair:

```
setsid nohup dockerd >/tmp/dockerd.log 2>&1 < /dev/null & disown
```

**Editor Linux:**

```
scons -j4 platform=x11 tools=yes target=release_debug
```

**Empacotar:** `smdhtool` + `3dsxtool ... --romfs=<projeto>`, ou o `build.sh package`, ou o exportador do editor.

**Versão de PC sem editor (para perfilar a VM):** `scons -j4 platform=x11 tools=no target=release` → `bin/godot.x11.opt.64`.
- Perfil: `valgrind --tool=callgrind --toggle-collect='GDInstance::call_multilevel*'`, dividindo pelo número de chamadas.
- Memória: `valgrind` (memcheck) sob `xvfb-run`.

**Pacote portátil do Windows (CI):**
- O marcador do modo portátil é o arquivo visível `_sc_` (commit `7c0fed9`). O `upload-artifact@v4` descarta arquivos ocultos, e o `._sc_` sumia.
- Sem o marcador, o editor usa `%APPDATA%\Godot` (e templates velhos que estiverem lá).

### 3.7 Teste no emulador sem tela (`misc/3ds-build/emu_test`)

- **O que é:** o core libretro do Azahar com renderizador por software, mais `lr_harness.c` e `run3ds.sh`.
- **Como rodar:** `./run3ds.sh <projeto> <quadros> <saída> roteiro.txt`.
  - O roteiro aceita `shot N`, `press N BOTÃO` e `release N BOTÃO`.
  - Variáveis: `LR_OLD3DS=1`, `LR_AUDIO=1`.
- **Ressalva:** os tempos são do relógio emulado. Servem para comparar versões; não substituem o console.
- O renderizador por software do Azahar **não aplica filtro bilinear**: diferenças de filtro só aparecem no PC (GLES2) ou no console.
- Para ver erros de script do jogo: `LR_VERBOSE=1` mostra o `OutputDebugString` (ex.: "Identifier not found").
- Rodar um `.3dsx` pronto: `lr_harness azahar_libretro.so jogo.3dsx <quadros> <saída> [roteiro]`.

---

## 4. ARPG-3DS: estado do porte (agora com o Open)

### 4.1 Estrutura

- `proj/`: o projeto Godot 2.1. `tools/`: os conversores do projeto Godot 4.
- `tools/convert_map.py`:
  - gera um atlas único;
  - converte colisões em retângulos e acerta a orientação dos polígonos;
  - aplica `y_sort_origin = 4` nas árvores;
  - zera as camadas de colisão.
- `tools/convert_anims.py` e `tools/build_player.py`.
- `proj/player/`: `player.gd` e os estados.
- `proj/fx/screen_fade.gd`: autoload `ScreenFade`.
- `proj/scene/`: `world_map.tscn`, `test_player.tscn/.gd` (cena principal), `diag`, `map_viewer`.

### 4.2 Decisões de design

- Resolução nativa de 400x240 na tela de cima. A tela de baixo fica para menu e HUD.
- **Escala do mundo: decidido 2x** (200x120 ampliado; 1x fica como opção).
- Inimigos: teto sugerido de 8–16 por área; IA amortizada (`amort2`) quando precisar.
- Splash de boot: `res://asset/ui/splash.png` (200x120), nearest via `image_loader/filter=false`.

### 4.3 Player no 2.1

- **Processamento:** um único `_fixed_process`; entrada por eventos (`_input`); timers por contador.
- **Movimento:**
  - Pela `CollisionGrid2D`, com a caixa dos pés em `Rect2(-4,-6,8,6)` e contorno de quinas de 3 px.
  - Linha de visão por `segment_blocked`.
- **Controles:**
  - 3DS: A ataca, B pula, Y/R corre, SELECT dá 10 de dano (teste), START alterna a escala.
  - PC: setas, Z, X, C, Backspace, Esc.
- **`--autotest`:** roteiro automático no PC.
- **Morte:**
  - Sequência: arremesso, tentativa de levantar e queda em câmera lenta.
  - Depois, o `ScreenFade.reload_scene(1.2, 0.5, 1.0)` faz a transição: escurece, segura no preto, recarrega e clareia.
  - Os tempos são reais. O fade é um `ColorFrame` de 400x480 com alfa.
  - A escala de cinza do Godot 4 não foi portada (precisaria de efeito via TEV na engine).
  - **Por que o fade não aparecia antes:** a falha era do jogo. A morte recarregava a cena direto, sem fade.

### 4.4 Medições do usuário (console)

- **New 3DS:** 60 FPS cravados.
- **Old 3DS:**
  - Chegou a ~58 FPS depois do flush/troca de buffers, do limite de física e da colisão em retângulos.
  - Artefatos de rolagem: corrigidos.
  - Queda para 20 FPS no pulo atrás das árvores: corrigida.
- Bancada de 32 inimigos com a engine otimizada: ver 4.4b.
- **Pendente:** validar o fade da morte e o splash nearest no console; no PC, o `clip_uv`.

### 4.4b Bancada de 32 inimigos no Old 3DS (engine do run #26, medido pelo Open)

| Modo | 1x | 2x |
|---|---|---|
| `static` | 60 FPS (pior 15,4 ms) | 60 FPS (14,3 ms) |
| `ai` (todo step) | 30 FPS (29,1 ms) | 30 FPS |
| `amort2` | 32–55, oscila (20,5 ms) | **59 FPS** (18,8 ms) |
| `amort4` | **59 FPS** (19,0 ms) | 59 FPS (16,7 ms) |

- Contra o run #25: 1x `amort4` foi de 30 para 59 FPS; no 2x, o `amort2` já dá 59 FPS.
- O painel do 3DS roda a ~59,83 Hz, então "59 FPS" equivale a 60 cravado.
- Receita de produção: 2x + `amort2`. Para pior quadro ≤ 16,7 ms, `amort4`.

### 4.5 Números de referência no emulador

| Medida | Valor |
|---|---|
| Lógica | 0,3–0,4 ms |
| Desenho | ~2,1–2,2 ms |
| Draw calls perto do caminho | ~18 |

Custos conhecidos fora do jogo em si:
- O HUD de estatísticas de teste custa ~4 ms a cada 0,5 s, ao reformatar o texto.
- A primeira execução de cada animação custa ~2 ms.
- Recarregar a cena em texto é lento.

---

## 5. Debates e investigações

### 5.1 Y-sort das árvores: o que é mais barato? (sem decisão; nada alterado)

**Hipótese do Open:** a lógica estaria pesada por causa do Y-sort das árvores.

**Posição do Claude:** o Y-sort é custo de **desenho** (VisualServer), não de lógica. Hoje ele custa por quadro:
1. percorrer todas as faixas do mapa para descartar as que estão fora da tela (cresce com o mapa);
2. ordenar as faixas visíveis (barato);
3. uma chamada de desenho por faixa visível.

Também gasta memória e tempo de carga, com centenas de itens.

**Opções:**

| Opção | Ideia | Avaliação |
|---|---|---|
| **A** | Booleano "acima ou abaixo do player" por árvore | Descartada: é Y-sort manual e pior. Tiles não são nós, e com vários inimigos não há "acima de quem". |
| **B** | Camadas: base das árvores e da casa no chão (sempre atrás); copa e telhado numa camada de cima (sempre na frente); TileMaps sem Y-sort; Y-sort só num nó com player, inimigos e objetos | Mais leve e previsível; o custo não cresce com o mapa. Trabalho no conversor (lado do Open). |
| **C** | Engine: guardar as faixas estáticas já ordenadas, achar as linhas visíveis por busca binária e misturar com os poucos itens móveis | Visual exato e jogo sem mudança. O custo passa a depender só do que está na tela. Trabalho do Claude, na engine. |

**Limites da opção B:**
- No pulo na frente de uma árvore, a cabeça entra na copa e passa por trás dela.
- Se o z do pulo for ajustado para isso, o pulo atrás da árvore passa a cobrir a copa.
- As árvores 2x2 e a casa 3x3 precisam ser cortadas no conversor.

**Recomendação do Claude:** B para simplicidade; C para manter o visual exato. **Medir antes:** a mesma cena com e sem o Y-sort das árvores, no emulador e de preferência no Old 3DS. Diferença de ~0,2 ms não vale a mudança; ~2 ms vale.

**Pergunta em aberto ao usuário:** quanto vale o detalhe do pulo na frente das árvores?

### 5.2 Lógica dos inimigos: diagnóstico e otimização da VM (feito)
- Bancada do Open (`proj/scene/bench_enemies.tscn` + `bench_enemy.gd`, 32 inimigos): o gargalo é o GDScript por quadro, mas o custo estava no **caminho de chamada** (método nativo, `.x/.y`, `update()`, cópias de Variant), não nas contas.
- Medição: callgrind numa build x11 `target=release` (instruções x86 por `_fixed_process`) + emulador Old 3DS. No console o script custa ~4–5x o do emulador (cache pequeno, sem L2), o desenho ~2x.
- Itens feitos na engine (commits `6715f5e`, `5006a33`, `0992e35`, `24d1d5d`):
  1. Cache de método por ponto de chamada na VM (operando novo em OPCODE_CALL; até 2 tipos por site; métodos do script têm prioridade; GDScript/GDNativeClass e `free` no caminho genérico; objetos só em builds release).
  2. Setters de Sprite/AnimatedSprite/CanvasItem sem efeito quando o valor não muda; `update()` e afins com StringName pronto.
  3. Operadores int/float/Vector2 sem Variant temporário; cópia direta de Variants de tipos por valor.
  4. `.x/.y/.width/.height`, `.pos/.size/.end`, `.z`, `.r/.g/.b/.a` por StringName.
- Resultado (modo ai): 19.276 → 11.961 instruções x86 por inimigo/step (−38%); emulador O3DS: lógica 3,2 → 1,8 ms (32 inimigos). mathonly 8.054 → 5.633; noop 1.124 → 969.
- Validação: teste de semântica (`vm_semantics.gd`, 2.924 resultados idênticos ao binário antigo) + autotest do jogo (mesma sequência de estados).
- Bugs pré-existentes do 2.1 achados (não corrigidos): `int % 0` com bool, e `bool / 0`, derrubam o programa (SIGFPE).
- Próximos possíveis: constantes de classe resolvidas na carga; entrada de função mais barata; custo de desenho por item (~0,1 ms por sprite no console).
- Lado do jogo: constantes inteiras no lugar de `mode == "texto"`, `physics/max_steps_per_frame=1`, amortizar a IA.

### 5.3 "Crash do run #26" (resolvido: não era a VM)
- Sintoma: cenas com Label davam svcBreak/PANIC em ~1,5 s (Azahar e console).
- Causa: o `0bench.3dsx` foi montado com um template antigo (~11/11, sem CollisionGrid2D, Platform3DS.get_frame_stats, LocalWireless), não com o do run #26. O script da bancada usa CollisionGrid2D → falha de compilação → crash. O ELF do run #26 usado para simbolizar não correspondia ao binário (daí a "FreeType").
- Provas: segmentos do .3dsx iguais aos templates de 11/11; strings das classes novas ausentes; o data.pck deles roda normal com o template do run #26.
- Correção (commit `031d489`): o build grava o commit (`core/version_hash.gen.h`, de BUILD_HASH/GITHUB_SHA/git); o template embute (`Platform3DS.get_build_id()`), e o exportador imprime o template usado e recusa template sem id ou de outro build (template custom no preset passa direto).
- Origem do template velho (confirmada): o artefato `Godot3DS-windows` vinha sem o `._sc_`, porque o `upload-artifact@v4` descarta arquivos ocultos. O editor saía do modo portátil e usava `%APPDATA%\Godot\templates`. Corrigido com o marcador visível `_sc_` (`7c0fed9`); o `sync-engine.ps1` do jogo também foi ajustado pelo Open.
- **Lição:** antes de bissectar a engine, conferir se o `.3dsx` contém o template certo. Use o build id, as strings das classes novas e o tamanho dos segmentos.

### 5.4 Splash de boot borrado (resolvido, `fd4f4a6`)
- O `VisualServerRaster::set_boot_image` alocava a textura com `TEXTURE_FLAG_FILTER` fixo. No 3DS isso vira `GPU_LINEAR`, e a pixel art saía borrada.
- Agora o `set_boot_image(..., p_use_filter)` recebe o filtro, e o `main.cpp` passa o `image_loader/filter` (splash custom e padrão).
- No mesmo commit:
  - os padrões dos diálogos de importação de textura seguem a mesma configuração;
  - corrigido o `image_loader/repeat` invertido na importação do diálogo de exportação.
- Validado no PC: com o filtro desligado, 5 cores (nítido); ligado, 46 cores (borrado). Falta ver no console ou no Azahar com renderizador de hardware.

### 5.6 Data abort em `Variant::call_ptr` (resolvido: engine `9535e74`, jogo `9bf1d84`)
- **Dump do console:** *data abort*, leitura em `0x1C`, PC em `Variant::call_ptr`, LR em `GDFunction::call` (simbolizado com o ELF do run #26).
- **Causa:** um script do jogo chamou um método num valor que não é objeto, quase certamente `null`. No release, `call_ptr` não checava se o método existe para o tipo e usava o resultado nulo da busca no mapa (`0x1C` é o deslocamento do valor no nó).
- **Correção na engine (proposta pelo Open):** a checagem do método passou a valer no release (`Nonexistent function 'x' in base 'Nil'`). A checagem de argumentos de menos em métodos built-in também (`"texto".substr(1)` → `Expected 2 arguments`).
- **Testes:** no PC, o release antigo dava *segfault* e o novo mostra os erros e segue; o mesmo no template do 3DS pelo emulador; a semântica da VM continua idêntica (2.924 resultados).
- **Causa no jogo (achada pelo Open):** `struggle_frames` no `state_death.gd`. O `player.tscn` tinha salvo `struggle_frames = null` no nó `Death`, e a morte chamava `.size()` e `[0]` em Nil.
  - Corrigido no ARPG-3DS (`9bf1d84`): a cena tem `IntArray( 50, 51 )`; o `init()` preenche 50/51 se vier null ou vazio; o default `IntArray([...])` do export foi removido.
  - Validado no Old 3DS: a sequência de morte completa roda sem crash.
- **Por que o editor salvou `null`** (comportamento original do 2.1, não regressão; o parser não mudou desde `dcb1fec`):
  - O default de um `export` só é registrado no editor quando a expressão vira constante na análise (`gd_parser.cpp`, ramo do export).
  - Num inicializador de variável, o literal `[50, 51]` não é reduzido a constante (só em contexto `const`), então `IntArray([50, 51])` não vira constante.
  - O editor fica com default `null` e salva `null` na cena; em tempo de execução, sem a cena, o inicializador funciona.
  - **Regra:** não usar construtor com literal (`IntArray([...])`, `Vector2Array([...])` etc.) como default de `export`; definir o valor na cena ou preencher no `init()`/`_ready()`.
  - **Possível melhoria na engine (não pedida):** calcular só o default do editor reduzindo uma cópia da expressão a constante, sem mudar o inicializador (arrays não podem ser constantes compartilhadas entre instâncias).

### 5.5 Receita recomendada para inimigos (lado do jogo)
  - a mesma do player: `CollisionGrid2D`, entrada sem polling e timers por contador;
  - IA a cada poucos quadros;
  - `VisibilityEnabler2D` para dormir fora da tela;
  - sem `is_action_pressed` nem buscas de nó por quadro;
  - evitar Area2D/física em massa.

---

## 6. Diagnóstico: crash `mcu` no menu da Rosalina (não é da engine)

**O que apareceu** (só na tela de baixo, enquanto o usuário estava no menu da Rosalina para tirar screenshot):

```
An error occurred (ErrDisp)
Error type: generic
Process ID: 11   Process name: mcu   Title ID: 0004013000001f02
Address: 0x0010366c   Error code: 0xd8606408
```

**Análise:**
- **Quem caiu:** o módulo de sistema **mcu** (bateria, carregador, Power, volume, slider 3D, tampa). Não foi o jogo; o endereço fica no código do próprio mcu.
- **O código `0xD8606408`:** vem do `sm` (gerenciador de serviços, reimplementado pela Luma em `sysmodules/sm/source/notifications.c`).
  - Ele é devolvido quando a fila de notificações do processo de destino está cheia (**16 pendentes**) e o envio não foi marcado para ignorar o estouro (bit 2 das flags).
  - Quando isso acontece, o mcu aborta.
- **Causa provável:** com o menu da Rosalina aberto, o sistema fica pausado e os assinantes dos eventos do mcu (módulos do sistema) não consomem as notificações. Eventos de bateria, carregador, volume ou slider se acumulam até estourar a fila.
- **Por que não é a engine:**
  - Ela não usa os serviços mcu nem ptm.
  - Ela não assina notificações do `srv`; usa só o gancho padrão do APT (`aptHook`).
  - A thread de áudio no núcleo 1 fica pausada junto com o app.
- **Como confirmar:** abrir a Rosalina no menu Home ou em outro homebrew, deixar aberta (carregando, mexendo no volume) e ver se o erro se repete.
- **Como evitar:**
  - Tirar o screenshot e sair rápido do menu.
  - Não mexer no volume nem no slider 3D com o menu aberto.
  - Manter a Luma atualizada.
- **Quando seria da engine:** se aparecer um ErrDisp com o **nome/título do nosso processo**. Nesse caso, pedir a foto com o endereço para localizar a função no `.elf`.

---

## 7. Pendências e próximos passos

**Do usuário / do Open:**
- Atualizar a engine (`sync-engine.ps1`) para o run com `9535e74` e conferir que a exportação usa o template certo (o console mostra "3DS export template: ... (build X, editor build X)").
- Ver o novo layout padrão dos docks (Cena/Arquivos à esquerda, Inspetor/Node à direita) após atualizar.
- Ver o splash nearest no console ou no Azahar com renderizador de hardware.
- No PC, validar o `clip_uv`.

**Decisões em aberto:**
- Y-sort das árvores: B, C ou nada (5.1). Medir antes.

**Bugs antigos do 2.1:**
- Corrigidos em release (antes só checados em debug): método inexistente num tipo que não é objeto (ex.: método em `null`) dava *data abort* em `Variant::call_ptr` (FAR 0x1C); chamar método built-in com argumentos de menos (ex.: `substr(1)`) lia `default_args` fora do vetor. Agora os dois viram erro de script ("Nonexistent function 'x' in base 'Nil'"; "Expected 2 arguments") e o jogo segue.
- Ainda não corrigidos: `bool / 0` e `bool % 0` derrubam o programa (SIGFPE).
- Em release, o erro de script aparece na saída de depuração (no Azahar, no log; no harness, com `LR_VERBOSE=1`). A linha mostrada é a do início da função; no PC (editor/debug) aparece a linha exata.

**Itens possíveis da engine (ainda não pedidos):**
- VM: constantes de classe resolvidas na carga; entrada de função mais barata (~970 instruções vazia).
- Custo de desenho por item (~0,1 ms por sprite no console).
- Cenas em formato binário no empacotamento do 3DS, para carregar mais rápido.
- Pré-aquecer os caches das animações.
- Efeitos de tela via TEV (escala de cinza, dia/noite por recoloração).
- Fontes bitmap.
- Banner/ícone do CIA.
- Opção C do Y-sort, se escolhida.

**Rede e Fase 7:**
- Testes de Wi-Fi pelo usuário.
- StreetPass.

---

## 8. Histórico de commits relevantes

**Godot-3DS** (`main/2.1`, mais recentes primeiro):

| Commit | Mudança |
|---|---|
| `9535e74` | release: método inexistente e argumentos de menos viram erro de script em vez de crash |
| `179fd19` | `CONTEXTO.md`: repositório do jogo é somente leitura para o lado da engine |
| `448d9c7` | editor: layout padrão (Cena/Arquivos à esquerda, Inspetor/Node à direita) |
| `84d21eb` | `CONTEXTO.md` passa a morar na raiz do repositório |
| `fd4f4a6` | splash e importação seguem `image_loader/filter`; `repeat` invertido corrigido |
| `7c0fed9` | CI: marcador portátil visível `_sc_` |
| `031d489` | exportador recusa template de outro build (build id) |
| `24d1d5d` | `.x/.y/.pos/.size` por StringName |
| `0992e35` | operadores rápidos e cópia direta de Variant |
| `5006a33` | setters sem efeito quando o valor não muda |
| `6715f5e` | cache de método por ponto de chamada na VM |
| `dcb1fec` | TileMap `clip_uv`: sem linhas de tiles vizinhos do atlas no PC |
| `6b3b272` | `CollisionGrid2D.build()` e pior quadro no `Platform3DS` |
| `2fc6efd` | `CollisionGrid2D`; TileMap sem corpos com camadas 0 |
| `4f8cdec` | Harness de teste no emulador |
| `a2ba33e` | Áudio no DSP e mixer no núcleo 1 |
| `6ee70a6` | Geometria guardada |
| `e58cbd7` | Limite de passos de física; sem trabalho 3D/MT por quadro |
| `2c8d70c` | Flush mínimo e troca de buffers ao fim da GPU |
| `8865a84` | `cell/y_sort_origin` |
| `92c5b3b` | Pixel snap na grade da escala |
| `1be20a3` | Pixel snap |
| `5a5bcc5` | TileMap Y-sort rápido, culling, estatísticas, telas por camada |
| `75cf8c4` | Batching 2D |
| `ed3c638` | Semáforo real |
| `8048240`, `e98de43` | LocalWireless |
| `16e8d7f` | Wi-Fi |
| `db40fe0` | Duas telas |
| `c206124` | Linhas, polígonos etc. |
| `8af9263`, `f3d20a2`, `c9fac0a` | Inspector/editor |
| `6bf907a` | CI |
| `79f1abe`, `7703f31` | Exportador `.cia` / `.3dsx` |

**ARPG-3DS** (`main`, mais recentes primeiro). Daqui em diante, os commits do jogo tendem a vir do Open.

| Commit | Mudança |
|---|---|
| `4155e91` | `sync-engine.ps1` preserva `editor_data/` (exceto templates e tmp) |
| `9bf1d84` | Morte: `struggle_frames` null corrigido (crash no release do 3DS) |
| `6664442` | Morcego: combate e perseguição (APIs do 2.1, áreas de golpe) |
| `fa42624` | Morcego: pisca ao morrer, HurtBox desligada na morte |
| `936e0f4` | Morcego: base de inimigo, patrulha/perseguição, explosão |
| `5831d1c` | `CONTEXTO.md` do jogo com os números do run #26 e a decisão do 2x |
| `139df06` | `sync-engine` usa o marcador visível `_sc_` |
| `1aeb36e`, `eb718bb` | Bancada: zoom 1x/2x, modos `amort*` |
| `13c7710` | Fade da morte (`ScreenFade`) |
| `2813537` | Entrada por eventos e notas de desempenho |
| `bef2c95` | Player com `CollisionGrid2D` |
| `f0c4a1a` | Upload do usuário (`explosion-9-frames.png`) |
| `6083cef` | Atlas |
| `35e0c9b` | Colisão em retângulos |
| `59ff8aa` | Y-sort origin nas árvores |
| `e1ab977` | Player inicial |
