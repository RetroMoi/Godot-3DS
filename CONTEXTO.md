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
  - `local_wireless_3ds.*`: transporte UDS (`LocalWirelessUDS`) do singleton `LocalWireless`.
- **`core/io/local_wireless*`:** o `LocalWireless` genérico (canal confiável, sinais, simulação de perda/latência) e o transporte UDP do PC (5.12).
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

**Actions do CI:** `checkout@v5`, `upload-artifact@v7` e `download-artifact@v8`, que rodam em Node 24 (as v4 eram Node 20, que o GitHub está aposentando). As v5/v6 de upload e a v6 de download ainda são Node 20. Defaults conferidos: artefato zipado, `include-hidden-files` e uma pasta por artefato no download. Run verde com o `_sc_` no pacote. Resta só um aviso informativo: `ubuntu-latest` passa para Ubuntu 26 a partir de 19/10/2026.

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
- **Binários:** `lr_harness` e `azahar_libretro.so` ficam em `/home/user/tools3ds/` (fonte do core em `/home/user/azahar-emu/azahar`). Para usar o `run3ds.sh`, copiá-los para a pasta dele. Os dois são ignorados pelo git (`*.so` e `misc/3ds-build/emu_test/lr_harness`, commit `9d85be4`).
- **Exportação de ponta a ponta (como no editor):**
  1. Gerar o template do build atual: `3dsxtool bin/godot.3ds.elf bin/tmpl_test.3dsx` (no Docker). O template instalado em `~/.godot/templates` é antigo e é recusado pela checagem de build.
  2. No `export.cfg` do projeto, em `[platform:Nintendo 3DS]`, apontar `custom_template/release` para ele (e ligar opções como `debug/visible_collision_shapes=true`).
  3. `godot -path <proj> -export "Nintendo 3DS" saida.3dsx` (com Xvfb).
  4. Rodar com `lr_harness` (criar antes `<saída>/system/Azahar/sdmc/3ds/dspfirm.cdc`, 4 KB de zeros).
  5. Apagar `bin/tmpl_test.3dsx` depois.

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

### 5.7 Editor de TileSet no estilo do Godot 4 (em andamento: fases 1, 2 e 3 feitas)
**Diagnóstico do 2.1:**
- O TileSet é uma lista de tiles soltos (textura + região), sem atlas, grade ou grupos.
- A edição era no Inspetor (`0/name`, `0/region`... digitados), por "Converter para TileSet" a partir de uma cena de Sprites, ou pelo menu "Tile Set" (só adicionar/remover).
- O TileMap pinta um tile por vez, com a paleta em lista.
- Não existiam blocos, aleatório/probabilidade, z por tile, terrenos/autotile, padrões nem dados customizados.

**Decisões do usuário:**
- "Prioridade" = **z de desenho por tile**.
- Autotile entra no plano, se o custo se justificar.

**Premissa:** tudo é do editor. O mapa salvo continua sendo uma grade de ids, e o jogo, o TileMap e o renderizador do 3DS não mudam. A exceção é o z por tile (fase 3), que mexe no TileMap e precisa de medição.

**Fases:**
1. **Feita (`35fa8fd`):**
   - Painel inferior **TileSet**, que aparece também para o TileSet do TileMap selecionado.
     - Atlas à esquerda ("Adicionar atlas…", ou texturas já usadas com grade estimada, marcadas com "*"), com tamanho do tile, margem e separação.
     - Grade no centro: tiles contornados, células sem tile escurecidas, zoom com Ctrl+roda, rolagem com a roda, arrasto com o botão do meio ou Espaço.
     - Modos **Selecionar** e **Criar/Apagar**: clique cria; arrastar sobre células vazias cria tile de várias células; botão direito remove; "Criar tiles nas células não vazias".
     - Propriedades do tile num editor de propriedades (proxy `TileSetEditorTileProxy`).
     - Tudo com desfazer.
   - Painel inferior **TileMap**: os atlas em grade; clicar escolhe o tile, sincronizado com a lista lateral.
   - O TileSet ganhou `atlas/<n>/texture|tile_size|margin|separation` (só do editor) e os métodos `atlas_*`.
2. **Feita (`30caa95`):**
   - Paleta do TileMap: arrastar sobre os tiles do atlas monta um **bloco**. O lápis carimba o bloco na célula; linha, retângulo e balde repetem o padrão a partir da âncora; Ctrl+clique põe ou tira tiles do pincel.
   - Botão **Random** (o "dado"): um sorteio por célula, ponderado pela **Probability** de cada tile (propriedade nova do tile, 0 a 100, padrão 1).
     - O sorteio usa um hash (finalizador do murmur3) da célula e de uma semente por traço, então a prévia bate com o resultado.
     - **Scattering** deixa uma fração das células sem mexer.
   - **Grupos aleatórios** salvos no TileSet (`random_group/<n>/name|tiles`): "Save…" guarda o pincel com um nome e já pinta com ele; escolher na lista liga o Random com o grupo; "Delete" remove. Tudo com desfazer.
   - O pincel sobrevive a mudanças no TileSet (só saem os tiles removidos) e é zerado ao editar um TileMap com outro TileSet.
   - Testado no editor (bloco, aleatório variado, salvar/escolher grupo) e salvando/carregando em `.tres` e `.res`.
   - **Ajustes pedidos pelo usuário (`7f9d563`):**
     - Prévia do tile no painel TileSet: célula e vizinhas, cruz vermelha na origem da célula (o Tile Origin do TileMap editado, com seletor), textura deslocada pelo Tex Offset (contorno amarelo e ponto no canto) e a shape de colisão deslocada pelo Shape Offset (azul).
     - A shape de colisão é editada no próprio painel: as propriedades dela (extents, radius...) aparecem em "Shape Settings", logo abaixo de "Shape", sem abrir outra página. Outros recursos (material, navegação, oclusor) abrem no Inspetor.
     - Setas para cima/baixo funcionavam uma vez só: o painel se reconstruía no "changed" do TileSet causado pela própria edição. Também passaram a funcionar nos SpinBox (Shift = 10 passos).
3. **Feita:**
   - **z por tile (`c5e14b7`):** `<id>/z_index` (-4096 a 4096; gravado só quando diferente de 0) e `tile_set_z_index`/`tile_get_z_index`. O TileMap cria um item de canvas novo no quadrante quando o z muda de um tile para o seguinte (como já fazia com material), com esse z relativo ao TileMap. Mapas sem z desenham exatamente como antes. Como o z de CanvasItem, vale acima do Y-sort: um tile com z 1 fica sobre irmãos de z 0 (ex.: copa sobre o player). Testado no PC e no emulador.
   - **Dados customizados (`021cb82`):** camadas como no Godot 4.
     - `custom_data_layer_count` e `custom_data_layer/<n>/name|type` (bool, int, float, String, Vector2, Color), editadas no Inspetor do TileSet. O menu do painel tem "Custom Data Layers (Inspector)...".
     - Cada tile ganha um campo tipado por camada (`<id>/custom_data/<nome>`, seção "Custom Data").
     - Os valores ficam num dicionário por tile (`<id>/custom_data`, gravado só se não vazio). O valor padrão do tipo não é gravado. Renomear uma camada renomeia em todos os tiles.
     - API: `tile_set/get_custom_data`, `find_custom_data_layer` etc., e `TileMap.get_cell_custom_data(x, y, camada)`.
     - Testado em `.tres` e `.res`.
   - **Colisão visual (`f9aff17`):** na prévia do tile, arrastar a shape move o Shape Offset; a alça muda o extents do retângulo (canto) ou o raio do círculo (lado direito). Pixels inteiros, uma ação de desfazer por arrasto. Tile sem shape ganha "+ Rect Shape" (retângulo cobrindo a célula, conforme o Tile Origin).
   - **No ambiente de teste, o clique simulado em Cena > Desfazer às vezes não dispara; o Ctrl+Z com o mouse sobre o mapa funciona.**
4. Padrões.
5. Terrenos/autotile (só no editor).

**Testes de editor sem tela:**
- Xvfb + `python-xlib` (XTest) para clicar e teclar, com capturas de tela via `import`.
- Os plugins de editor via `addons/` não carregaram nesse ambiente.
- O editor abre uma cena com `godot -e cena.tscn`, com o caminho relativo dentro do projeto.
- O script `edrun.sh` (scratchpad) aceita `EDARGS="-r 1440x860"` para abrir o editor numa janela maior (o padrão de 1024x600 aperta o painel inferior).
- Cuidado: o script mata o editor com `pkill -f godot.x11.opt.tools`. Se o mesmo comando do shell tiver esse texto, o próprio shell é morto (saída 144).

**Repositório:** o `.gitignore` tem `editor/*`, então arquivos **novos** em `editor/` precisam de `git add -f`.

### 5.8 "Colisões visíveis" não aparecia (resolvido na engine `88e9e69`; falta um ajuste no jogo)
**Causa (no jogo):** `player.gd` (`_ready`, ~linha 95) e `enemy.gd` (`_ready`, ~linha 47) dão `hide()` nos CollisionShape2D para o servidor visual não percorrê-los. Escondidos, a opção Depurar > Colisões Visíveis não tem o que mostrar. Além disso:
- a colisão do mundo (árvores) é a `CollisionGrid2D`, que não tinha desenho de depuração;
- no 3DS a opção não tinha como ser ligada: o template é `target=release` (sem `DEBUG_ENABLED`, a flag era sempre falsa) e não recebe linha de comando.

**Correção na engine (`88e9e69`):**
- CollisionShape2D/CollisionPolygon2D: com o jogo rodando sem a opção, o item de desenho sai sozinho da lista do servidor visual (a visibilidade do nó não muda; fica se o nó tiver filhos que desenham). Assim o `hide()` no jogo fica desnecessário.
- `CollisionGrid2D`: com a opção ligada, desenha os obstáculos (preenchimento + contorno, acima de tudo) no canvas do primeiro TileMap adicionado.
- A flag existe em todos os builds. No PC continua valendo o menu Depurar. No 3DS, desde `195cad5`, ela é uma opção **Debug > Visible Collision Shapes da exportação do Nintendo 3DS** (o exportador grava `debug/visible_collision_shapes` no `engine.cfb` exportado). Não é mais configuração de projeto; se a chave ainda estiver no `engine.cfg`, continua valendo e deve ser apagada.
- Viewport: os contatos 3D de depuração não são criados com 3D desligado.
- Testado no PC com uma cópia do jogo sem os `hide()`: aparecem a grade das árvores, as formas do player, o raio de detecção do morcego e a área de ataque. Por `-debugcol` e pela configuração de projeto.

**Para o Open:** remover os dois laços de `hide()` (player.gd e enemy.gd). Não há perda de desempenho com a engine nova.

**PANIC no 3DS com a opção ligada (relatado pelo Open; corrigido em `195cad5`):**
- Causa: o desenho de depuração da `CollisionGrid2D` levava 5 draws separados por obstáculo (o preenchimento e 4 linhas de outra cor quebram o lote), para o mapa inteiro, todo quadro. Com 861 obstáculos, eram ~4.300 draws, o que enchia o command buffer do citro3d. No emulador local o rasterizador cortava o quadro (tela cinza, "GPU command buffer full"); no Azahar do Open deu PANIC em `GPUCMD_Add`.
- Correção: obstáculos em blocos de 256 px (um item por bloco, descartado fora da tela); em cada bloco, os preenchimentos e depois os contornos (retângulos finos), formando dois lotes. O cache do TileMap no citro3d passou a checar o espaço antes de cada draw, não uma vez por item.
- Resultado no emulador (cena do jogo exportada com a opção): 60 FPS, 35 a 44 draws por quadro, sem buffer cheio.
- `C3D_FrameSplit`/`GPUCMD_Split` não ajudariam: dividir só manda a GPU começar antes; o espaço total do buffer no quadro continua o mesmo.
- Nesta versão do citro3d, `C3D_FixedAttribSet` só marca o estado como sujo, sem escrever no buffer.

### 5.9 Crossfade dia/noite: `ImageTexture.create_from_blend` (pedido do Open; feito em `bc450f7`)
**Pedido:** o `world/day_night.gd` do jogo troca o atlas do mapa entre 5 variantes (96x320, mesmo layout) com fade para preto. O Open quer um crossfade em N passos (8; útil de 6 a 12), com a mistura na CPU, sem TEV nem segunda textura no renderizador. O contexto do jogo registra isso na seção 15.12.

**API:** `var tex = ImageTexture.new()`; `tex.create_from_blend(from, to, t)`.
- `t` vai de 0 (`from`) a 1 (`to`), limitado a 0..1.
- Os dois precisam ser `ImageTexture` do mesmo tamanho; senão, erro claro.
- A mistura usa alfa pré-multiplicado (sem halo nas bordas transparentes) e sai em RGBA. As flags vêm de `from`.
- Chamado de novo no mesmo `ImageTexture` com o mesmo tamanho e flags, reaproveita a textura na GPU (mesmo RID, sem nova alocação).
- Devolve a própria textura.
- Lê os pixels com `get_data()`: o 3DS (RGBA) e o PC (GLES2 com cópia em RAM) guardam a imagem de toda textura.

**Medições:**
- PC: ~2 ms por passo, com upload.
- 3DS emulado (Old 3DS), antes da otimização: 12,7 ms por passo, sendo 8 ms só no upload (`texture_tile_sw` calculava o endereço Morton pixel a pixel na textura POT de 128x512).
- Otimizações: tabela Morton e 8 pixels por linha de bloco no `texture_tile_sw`, que acelera **todo upload de textura** no 3DS; mistura de dois canais por multiplicação nos pixels opacos.
- Resultado: upload 2,1 ms, passo 6,3 ms. Cabe num quadro (o jogo usa ~3,6 ms dos 16,7).
- O atlas só tem pixels opacos (26.547) ou totalmente transparentes (4.173); nenhum semitransparente.
- A troca no `texture_tile_sw` foi conferida: um quadro do jogo sai idêntico ao de antes (só o texto de tempo do HUD muda).

**Dica para o jogo:** usando sempre o mesmo `ImageTexture` misturado, depois do primeiro passo (que aplica a textura nos tiles e chama `update()`) os passos seguintes só trocam os pixels, com o mesmo RID. Não precisa chamar `tile_set_texture` nem `update()` de novo, o que poupa a reconstrução dos quadrantes do TileMap. No fim, aplicar a variante destino e liberar a misturada.

### 5.10 Troca de paleta: `PaletteTexture` (substitui o blend de imagens no dia/noite; `ed89634`, `fbe1b9e`)
**Ideia (do usuário):** os 5 atlas do mapa são PNGs indexados com **o mesmo mapa de índices** (0 pixels diferentes) e paletas de 44 cores na mesma ordem (índice 0 transparente). Os `.hex` do jogo (commit `98c1202`, formato Lospec: uma cor `RRGGBB` por linha, linha N = índice N) batem cor por cor com as paletas dos PNGs. Misturar as paletas dá o mesmo resultado, pixel a pixel, que misturar as imagens.

**Engine:**
- **`PaletteTexture`** (`scene/resources/palette_texture.*`) é uma `Texture`, usável como textura dos tiles.
  - Guarda no próprio recurso o mapa de índices e as paletas nomeadas, com as fontes.
  - API: `set_palette`, `blend_palettes(de, para, t)` (pré-multiplicado onde o alfa difere), `set_colors`, `get/set_palette_colors`, `remove_palette`, `get_palette_names`, `get_data`, `create_from_sources(png, [paletas])`, `get_last_error`.
  - O RID da textura na GPU nunca muda.
- **Fontes das paletas:** `.hex` (`RRGGBB` ou `RRGGBBAA`; sem alfa, o alfa de cada índice vem do PNG) ou PNG indexado com o mesmo mapa de índices (a ferramenta acusa quantos pixels diferem). O nome vem do sufixo: `outside_atlas-dusk.hex` vira `dusk`, e o arquivo sem sufixo vira `base` e substitui a paleta do próprio PNG.
- **Leitura do PNG:** o leitor expandia para RGBA todo PNG com paleta que tivesse tRNS. Agora há uma opção que mantém os índices (`Image::_png_load_indexed_func`). O carregamento normal das texturas não mudou.
- **VisualServer:** `texture_set_palette_indices`/`texture_set_palette` e `FEATURE_PALETTE_TEXTURE`.
  - No 3DS, o rasterizador guarda os índices já na ordem em blocos da GPU e escreve `paleta[índice]` direto na memória da textura. Depois da primeira passada, com nearest e clamp, reescreve só os blocos da imagem.
  - No PC, o próprio recurso monta a imagem RGBA (o GLES2 não mudou).
- **Editor:**
  - **Importar > Palette Texture:** PNG indexado + `.hex`/PNGs, gravado em **`.res`** (só o binário guarda os metadados de reimportação), com as fontes em `res://`.
  - **Menu do painel TileSet > "Use Palette Texture...":** troca nos tiles (e na grade do atlas) a textura PNG de origem pelo `PaletteTexture`, com desfazer.

**Testes:**
- Cada paleta reproduz exatamente o PNG da variante (diferença 0). A mistura bate com `create_from_blend` (até 1/255).
- `.tres`/`.res` salvam e carregam. As mensagens de erro saem certas (PNG não indexado, `.hex` com cores de menos).
- **Custo (Old 3DS emulado):** `blend_palettes` 0,76 ms, `set_palette` 0,73 ms, contra 6,3 ms do `create_from_blend`. No PC, ~0,04 ms.
- **Jogo inteiro** (cópia no scratchpad, TileSet usando o `.res` e ciclo de paletas a cada quadro): 59–60 FPS; quadro com lógica 1,6–1,8 ms (mistura incluída) + desenho 2,3 ms. Memória: um mapa de índices (~30 KB) e uma textura, em vez de 5 PNGs e 5 texturas.

**Para o jogo (Open):** ver as pendências.

**Defeito do 2.1 visto nos testes:** `Image.convert(...)` chamado pelo GDScript numa imagem que já está no formato pedido trava. Evitar no jogo e nos testes.

### 5.11 Recursos do 3DS para o jogo: levantamento e decisões (debate, sem código ainda)
**O que existe hoje na engine/libctru:**
- **Co-op local:** o `LocalWireless` (UDS) está pronto e validado em 2 consoles. Era só 3DS e sem garantia de entrega; isso mudou em `ca45bd0` (ver 5.12).
  - Sala com até 16 jogadores; pacotes para todos ou para um; `send_var`.
  - O anúncio da sala leva até 200 bytes de `info`, que já aparecem em `get_rooms()`.
- **Giroscópio/acelerômetro:** o libctru tem `hidAccelRead`/`hidGyroRead` (é preciso ligar os sensores, e eles gastam um pouco). O `Input` do 2.1 já tem `get_accelerometer()`/`get_gyroscope()`. Falta o `os_3ds` ligar e alimentar a cada quadro, convertendo para g e rad/s.
- **Play Coins:** nada na engine. Ficam no `gamecoin.dat` (dados extras compartilhados `0xF000000B`, até 300). Falta confirmar a permissão no RSF do CIA.
- **Pedômetro:** `PTMU_GetTotalStepCount` existe no libctru.
- **StreetPass:** o libctru não tem o serviço CECD; seria implementar as chamadas na mão (3dbrew). Exige CIA instalado com Title ID próprio. Sem emulação; teste só com 2 consoles.
- **Infravermelho:** só o canal antigo (`iru`, bytes crus). O `ir:USER`, que os jogos usam, não existe no libctru. Alcance de centímetros e sem emulação. O C-stick e o ZL/ZR do New 3DS já funcionam.
- O libctru também tem `nfc` (amiibo), `boss` (SpotPass) e `news`: ideias para o futuro.

**Decisões do usuário:**
- **Co-op:** um console é o **host** e define as regras. É o primeiro a ser feito.
- **O jogo sai em `.cia`.**
  - O ID único do CIA precisa ser definido de vez, na faixa de homebrew `0xF0000`–`0xFFFFF`. Hoje o export usa `arpg-3ds`. Save, StreetPass e instalação ficam presos a ele.
  - Ver onde o save fica no CIA.
- **Play Coins:** desbloqueio de itens ou redução de espera (em horas); o design fica para depois. Só gastar moedas, com confirmação, nunca criar. Atenção: o relógio do console pode ser adiantado pelo jogador.
- **Sensores:** talvez um minigame de levar uma bola de A a B desviando de obstáculos (acelerômetro como gravidade, calibrar a posição neutra, 3D desligado); fica para depois.
- **Ordem:** co-op, depois sensores, depois Play Coins (quando o design fechar), por último StreetPass (com o CIA estável).

### 5.12 Co-op local: regras e framework (engine `ca45bd0` → `fd2e3d7`, validado em 2 consoles; o jogo é com o Open)
**Regras do usuário:**
- O host define as regras.
- Sem fogo amigo. Cada jogador tem a própria vida.
- Sem coleira: o mapa é pequeno, para não forçar todos os inimigos a ficarem ativos.
- Drops individuais.
- Quem morre renasce no ponto de origem do mapa.
- Qualquer um pode sair. Antes aparece uma confirmação dizendo que sair encerra a sessão multiplayer.
- **Decididos depois do teste em consoles:**
  - **Dia/noite:** vale a hora do host. Ele manda a hora no welcome e a cada troca de fase; o convidado guarda a diferença.
  - **START:** abre o prompt de sair da sala, sem pausar o jogo.
  - **Morte no co-op:** sem animação de morte.
    - O player toca a animação de dano e solta a explosão dos morcegos (`fx/explosion.tscn`).
    - Depois renasce no ponto de origem (`host-pos`/`guest-pos`), com a vida cheia e piscando invencível (`start_invulnerability()`).
    - Sem câmera lenta e sem recarregar a cena.
  - **Sprites:** o host é o player 1 e o convidado usa o `player2.png`, nas duas telas.
  - **Players entre si:** um não pode bater no outro, mas eles colidem e um pode pular por cima do outro.
    - Tiles e morcegos não mudam: o pulo não atravessa parede, e com morcego continua o que já existe (esquiva por altura e stomp).
    - A colisão é feita no script, porque a `CollisionGrid2D` só conhece os tiles.
    - Cada console empurra só o próprio player para fora da marionete.
    - A colisão é ignorada quando um dos dois está alto o bastante (altura no pacote rápido).
  - **Morcegos por spawn (substitui as instâncias fixas do teste):**
    - Cada mapa mantém X morcegos vivos; quando um morre, nasce outro.
    - Só o host (ou o jogo solo) faz o spawn. Os IDs são dados pelo host e avisados por evento confiável; o welcome leva a lista atual.
    - Aprovados: `respawn_delay` de 3 s; nascer a pelo menos 64 px dos players.
    - Animação nova `spawn` (aprovada): `Sprite` 0,1 → 1,15 (0,35 s) → 1,0 (0,5 s), opacidade 0 → 1 nos primeiros 0,1 s; HitBox, HurtBox e DetectRadius desligados e a IA parada; ao terminar, começa a patrulha.
  - **Alvo do morcego:** segue o primeiro jogador detectado e ignora o outro que entrar no raio nesse meio tempo.
    - Ao sair do modo agressivo (desistência, alvo sumiu), volta a considerar os dois e varre o raio (`get_overlapping_bodies`).
    - Exceção (confirmada pelo usuário): quem bate no morcego vira o alvo dele, mesmo que ele estivesse seguindo o outro.

**Quem decide o quê:**
- Cada console decide o movimento e a vida do próprio player, sem atraso nos controles. O dano por contato com inimigo é detectado no próprio console.
- O host decide o mundo: IA e vida dos inimigos, acertos, renascimento dos inimigos e drops.
  - O convidado manda "acertei o inimigo N"; o host aplica e responde com a vida nova.
  - No convidado os inimigos são marionetes.
- Estado rápido 20x/s pelo canal sem garantia: posição, vida e flags do player; no host, também a lista de inimigos. Cada pacote leva um número de sequência, e os velhos são descartados.
- Eventos pelo canal confiável: hello/welcome com a versão do protocolo (versão diferente é recusada), acerto, vida do inimigo, drop.
- As marionetes são desenhadas cerca de 100 ms no passado, interpolando entre dois estados.

**Engine (`ca45bd0`):**
- O `LocalWireless` virou genérico (`core/io/local_wireless.*`), com transportes trocáveis: UDS no 3DS, UDP no PC (`core/io/local_wireless_udp.*`).
  - No PC, a porta é a 27850. O scan procura em 127.0.0.1 e nos endereços dados com `add_scan_address(ip)`. O host repassa os pacotes entre convidados.
  - Assim, duas janelas do mesmo projeto no PC jogam juntas.
- Canal confiável: `send_reliable` e `send_var_reliable`.
  - Entrega garantida e em ordem por remetente, com reenvio a cada 150 ms (até 60 tentativas).
  - `was_packet_reliable()` e `get_pending_reliable_count()`.
- Sinais: `player_joined(id)`, `player_left(id)` e `disconnected()`.
- Testes: `set_simulated_loss(0..1)` e `set_simulated_latency(ms)`.
- O `poll` roda sozinho no início de cada quadro.
- Medido no PC com 3 janelas, 30% de perda e 50 ms de latência: confiável 50/50, em ordem; convidado→convidado 30/30; `disconnected` quando o host sai.

**Correção (pós-teste em 2 consoles):** o convidado ficava preso em "Entrando...".
- O `poll()` zerava o estado de um jogador quando ele "entrava" na máscara. Isso apagava o hello confiável enviado logo após o `join()`, antes de o sistema listar o host. Se a primeira cópia se perdesse (comum no UDS logo após conectar), não havia reenvio.
- Agora só a saída zera o estado.
- Reproduzido e validado no PC: com a primeira cópia perdida de propósito, e com 50% de perda.
- O `coop_test` também desiste depois de 10 s sem "welcome" e volta ao lobby com aviso.

**Segundo teste em 2 consoles (com `ceb9cef`):** o convidado agora volta ao lobby com "O host não respondeu.". Nenhum hello ou welcome atravessou em 60 reenvios (9 s).
- Leitura do código: as chamadas UDS são as mesmas da versão validada. O teste antigo (`local_wireless_test`) só usava broadcast (`send_var(v)`, destino 0), e foi só ele que passou em 2 consoles.
- O co-op depende de envio para um nó só (`udsSendTo` com NetworkNodeID): hello para o host, welcome e ACKs para o convidado. Esse caminho nunca foi visto funcionando em hardware. O libctru chama a flag `UDS_SENDFLAG_Default` de "desconhecida", e o erro não fatal 0xC86113F0 faz o sistema pular o quadro em silêncio (antes contado como sucesso).
- Causa provável (inferida, sem console aqui): o envio para um nó específico não chega.
- Mudança:
  - o transporte UDS manda tudo em broadcast, com o destino no 1º byte; quem recebe descarta o que não é para si;
  - pacote máximo 1.477 bytes no UDS (1.474 úteis; `MAX_PACKET_SIZE` acompanha);
  - o pulo não fatal agora conta como falha.
- Diagnóstico novo: `LocalWireless.get_stats()` (sent, send_failed, received, received_reliable, acks_received, resent, gave_up, pending). O `coop_test` mostra os contadores na tela de baixo e na mensagem de tempo esgotado.
- Validado: PC com 2 janelas e 30% de perda; host no emulador do Old 3DS (200 envios em broadcast, 0 falhas, 60 FPS).
- Como ler os contadores se falhar de novo:
  - convidado com `rec 0`: não chega nada do host, nem os pacotes rápidos;
  - convidado com `rec` > 0 e `conf 0`: os rápidos chegam e o welcome não;
  - `falha` alto: o sistema recusa os envios.

**Correção (`f2337f2`):** `StreamPeerBuffer::get_partial_data` (do 2.1) não tinha o `return` final. Em build otimizado, toda leitura (`get_u8`, `get_16`...) derrubava o jogo. Afeta qualquer uso de `StreamPeerBuffer`, não só o co-op.

**Referência (`misc/3ds-build/coop_test`, ver o README):**
- O que tem: caverna com 2 jogadores, sala, inimigos do host, ataque, dano, renascimento e prompt de saída.
- Ações `coop_*` no padrão Nintendo (A confirma, B volta). Atenção: os `ui_*` padrão do 2.1 seguem o layout do Xbox, então no 3DS `ui_accept` é o B e `ui_cancel` é o A. O jogo do Open já mapeia as próprias ações.
- Teste no PC: `COOP_AUTO=host|join`, `COOP_BOT=1`, `COOP_LOSS`, `COOP_LATENCY` e `COOP_WINPOS`.
- Validado:
  - no PC, duas janelas com 20% de perda e 60 ms de latência: os dois players e os inimigos sincronizados, renascimento e saída com aviso ao outro;
  - no emulador do Old 3DS, como host: 60 FPS, A cria a sala, direcional move, B abre a confirmação.
- **Terceiro teste em 2 consoles (com `fd2e3d7`): tudo funcionou, zero erros.** Isso confirma o diagnóstico: no UDS, só o broadcast chega. Os dois primeiros testes tinham falhado (o 1º travou em "Entrando..."; o 2º deu "O host não respondeu.").
- **Ponto aberto: o Old 3DS roda o `coop_test` a 45 FPS.**
  - O mesmo número apareceu no teste antigo (`local_wireless_test`, Old 3DS como cliente) e não mudou com o `8048240`.
  - O papel do Old 3DS neste teste (host ou convidado) não foi anotado.
  - A cena é mínima (uns retângulos e um Label), e no emulador o host roda a 60. Por isso a suspeita é o custo da rede por quadro no console (chamadas ao serviço UDS: puxar e enviar pacotes, status), não uma limitação do aparelho.
  - Não foi medido. Próximo passo da engine: medir o tempo gasto no `LocalWireless` por quadro (contador em `get_stats`) e mostrar o `Platform3DS.get_frame_stats()` no `coop_test`; o usuário roda de novo no Old 3DS.

**Preparação do jogo (`2ede41b`, do usuário) e pontos para o Open:**
- O que o commit traz:
  - cena `scene/multiplayer-cave.tscn` (22x15 tiles, cabe na tela de cima sem câmera), com `host-pos` e `guest-pos` e 4 morcegos (`Bat`, `Bat1`, `Bat2`, `Bat4`);
  - botões novos: ataque no X, `talk` no A, pulo no B, corrida no Y/R;
  - janela de teste no PC de 800x960;
  - `debug/visible_collision_shapes` no `export.cfg` (lugar certo).
- **Morte quebra o co-op como está:**
  - `state_death.gd` usa `OS.set_time_scale` (0,5 e 0,2), que deixa lento o jogo inteiro daquele console: no host, os morcegos e o envio de estado também.
  - Depois chama `ScreenFade.reload_scene` → `reload_current_scene()`, que recarrega o mapa: no host, apagaria inimigos e estado do mundo.
  - No co-op, a morte tem de ser local: animação, fade só na tela de quem morreu e respawn no `host-pos`/`guest-pos`, sem recarregar a cena e sem time scale global.
- **Menus:** o `engine.cfg` do jogo não redefine `ui_accept`/`ui_cancel`, então botões com foco (lobby, lista de salas, prompt de saída) confirmam com o B no 3DS. Sugerir `ui_accept` com `jbutton(0, 1)` e `ui_cancel` com `jbutton(0, 0)`.
- **Dia/noite:** segue o relógio de cada console (`OS.get_time()`), e os relógios podem diferir. No co-op, o host manda a hora dele no welcome (e periodicamente, ou na troca de fase), e o convidado usa a hora do host.
- **Grupo `player`:** com o player remoto (marionete) no grupo, código como `get_nodes_in_group("player")[0]` (ex.: `test_player.gd`) pode pegar a marionete. Guardar a referência do player local.
- **IDs dos morcegos:** os nomes dos nós são iguais nos dois consoles. Converter para índices pequenos (ordem dos filhos) para os pacotes, em vez de mandar strings.
- **Pausa (START):** no co-op não pode pausar a árvore. Abrir um menu local sem parar o mundo, ou não ter pausa.
- **HOME, fechar a tampa, sair do alcance:** tratar o sinal `disconnected` (aviso e volta ao título ou ao modo solo). No host, `player_left` remove a marionete e o jogo segue.

### 5.13 Desempenho no Old 3DS e tamanho do binário (investigação em andamento)
**Situação:** o Open está implementando o co-op no jogo (mensagem final entregue). Enquanto isso, a engine investiga o desempenho no Old 3DS.

**Perguntas do usuário:**
- O `.3dsx` nunca fica abaixo de ~12 MB, mesmo com um projeto minúsculo, enquanto alguns apps têm poucos KB. A engine inclui coisas demais?
- Isso pode ter relação com o desempenho, especialmente no multiplayer (o `coop_test` a 45 FPS no Old 3DS)?

**Plano:**
1. Tamanho: medir o que compõe o binário (código, dados, RomFS; por pasta e módulo) e ver o que pode sair sem perder recurso usado pelo jogo.
2. Relação com o desempenho: separar o que o tamanho afeta (RAM, carga, cache de instruções) do custo por quadro.
3. Os 45 FPS:
   - medir no console o tempo gasto na rede por quadro (chamadas ao serviço UDS) e o detalhamento do quadro (`Platform3DS.get_frame_stats`);
   - comparar lobby (sem rede) x sala, host x convidado.

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
- Atualizar a engine (`sync-engine.ps1`) para o run mais recente (com `9d85be4`, que inclui `195cad5` e `7f9d563`) e conferir que a exportação usa o template certo (o console mostra "3DS export template: ... (build X, editor build X)").
- Ver o novo layout padrão dos docks (Cena/Arquivos à esquerda, Inspetor/Node à direita) após atualizar.
- Ver o splash nearest no console ou no Azahar com renderizador de hardware.
- No PC, validar o `clip_uv`.
- Testar no console a exportação com "Visible Collision Shapes" (só testado no emulador) e, no editor, os ajustes do painel TileSet (prévia, Shape Settings, setas).
- Open: migrar o dia/noite para o `PaletteTexture` (5.10). Importar > Palette Texture com `outside_atlas.png` e os 5 `.hex`, gerando `res://tileset/outside_palettes.res`; no painel TileSet, "Use Palette Texture..."; salvar o TileSet. No `day_night.gd`: tirar os `preload` dos PNGs, o pré-cálculo e o `set_data`; a transição chama `blend_palettes(de, para, t)` a cada quadro e termina com `set_palette(destino)`. Os nomes das paletas: `base` (dia), `mrng`, `dusk`, `night`, `latenight`. O `create_from_blend` (5.9) fica como alternativa para arte sem paleta.
- Open: remover os `hide()` dos CollisionShape2D em `player.gd` e `enemy.gd`, e apagar `debug/visible_collision_shapes` do `engine.cfg`, se tiver sido posta lá (5.8). Para testar no 3DS, usar a opção na tela de exportação.

- Open: co-op no jogo com os gráficos reais, seguindo 5.12 (mensagem + complemento entregues ao usuário). Testar no PC com duas janelas antes do console.
- Engine: medir por que o Old 3DS fica a 45 FPS no `coop_test` (custo da rede por quadro?) e o que compõe os ~12 MB do `.3dsx` (5.13).

**Engine, próximo passo (combinado com o usuário):**
- Editor de TileSet, fase 4 (padrões) e depois fase 5 (terrenos/autotile, só no editor) (5.7). A fase 3 está feita.
- Investigar: no 3DS, o que passa de y=240 na tela de cima reaparece como 1 linha no topo (visto num teste com uma faixa de tiles cruzando a borda; anterior às mudanças de hoje). Ver se acontece no jogo.

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
| `f2337f2` | `coop_test` (referência de co-op) e correção de leitura do `StreamPeerBuffer` (faltava `return`) |
| `ca45bd0` | `LocalWireless` genérico: canal confiável, sinais de entrada/saída, simulação, transporte UDP no PC |
| `fbe1b9e` | editor: Importar > Palette Texture; "Use Palette Texture..." no painel TileSet |
| `ed89634` | `PaletteTexture`: índices + paletas nomeadas; troca/mistura de paleta sem reenviar imagem (3DS: escrita direta na textura) |
| `19ad51e` | CI: actions em Node 24 (`checkout@v5`, `upload-artifact@v7`, `download-artifact@v8`); some o aviso de Node 20 |
| `f9aff17` | editor de TileSet: shape de colisão editada arrastando na prévia; "+ Rect Shape" |
| `021cb82` | TileSet: camadas de dados customizados; `TileMap.get_cell_custom_data` |
| `c5e14b7` | TileSet: z de desenho por tile |
| `bc450f7` | `ImageTexture.create_from_blend` (crossfade dia/noite) e upload de textura ~4x mais rápido no 3DS |
| `9d85be4` | `.gitignore`: binário do harness do emulador |
| `7f9d563` | editor de TileSet: prévia do tile (célula, origem, tex offset, shape), shape editada no próprio painel, setas nos campos |
| `195cad5` | 3DS: colisões visíveis sem encher o command buffer; opção movida para a exportação |
| `88e9e69` | colisões visíveis: formas escondidas pela engine, desenho da `CollisionGrid2D`, opção `debug/visible_collision_shapes` (3DS) |
| `30caa95` | editor: pincel de bloco, aleatório com probabilidade, espalhamento e grupos aleatórios |
| `35fa8fd` | editor: TileSet com atlas (painel inferior) e paleta em grade no TileMap |
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
