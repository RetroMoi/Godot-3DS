# Godot 2.1 → Nintendo 3DS (jogos 2D) — Plano

Base: fork de **Homebrodot/Godot**, branch `main/2.1` (já contém `platform/3ds`, `drivers/3ds`, rasterizer citro3d).
Meta de hardware: limites do **Old 3DS/2DS** (ARM11 268 MHz, ~64 MB RAM para o app, 6 MB VRAM); testar primeiro no New 3DS e no emulador (Azahar).

## Fase 0 — Ambiente e primeira build  *(em andamento)*
- [x] Imagem Docker com devkitARM (`devkitpro/devkitarm`), libctru, citro3d, portlibs e SCons 4.8.
- [x] `misc/3ds-build/build.sh`: template 3DS compila (`scons platform=3ds target=release tools=no disable_3d=yes`).
      Ajuste para GCC 16: `-Wno-error=incompatible-pointer-types` etc. em `CFLAGS` (já em `platform/3ds/detect.py`).
- [x] Empacotamento `.3dsx` + SMDH (3dsxtool/smdhtool).
      Medição: código (`.text`) = **13,4 MB** (~21% da RAM do Old 3DS) → reduzir na Fase 1/4.
- [ ] Compilar o editor (`platform=x11 tools=yes`) — corrigir Python 3/GCC novos.
- [x] Projeto de teste `misc/3ds-build/test_project` (Sprite girando + Label com FPS/memória/botões) e
      empacotamento com RomFS: `build.sh package` → `bin/test_project.3dsx`.
- [x] Azahar 2126.2: roda a **60 FPS** estáveis (sprite girando + Label).
      Obs.: "memória estática" aparece 0 KB porque o build release não rastreia memória
      (`DEBUG_MEMORY_ENABLED` desligado) → medir pela libctru (`osGetMemRegionUsed`) na Fase 1.
- [ ] Rodar no console (Luma3DS), de preferência Old 3DS; anotar FPS e memória.

## Fase 1 — Template correto
- [x] Render nativo 400x240: alvo 240x400 + transferência sem escala, projeção/recorte do canvas em
      400x240, video mode 400x240. Azahar (Old 3DS): VRAM 2,9 → 0,7 MB; quadro 5,55 → 4,80 ms.
- Texturas sem cópia na RAM; formatos nativos (RGB565, RGBA4, L8/A8, ETC1).
- `main_pack` lido de `romfs:/data.pck` (Fase 2).
- [~] `user://` → `sdmc:/3ds/<nome do jogo>/`; sleep/HOME → NOTIFICATION_WM_FOCUS_OUT/IN; fechar pelo
      HOME → NOTIFICATION_WM_QUIT_REQUEST. Aguardando teste no console.
- [x] Entrada: Circle Pad → eixos 0/1, C-Stick → eixos 2/3 (New 3DS), touch da tela de baixo →
      SCREEN_TOUCH/SCREEN_DRAG (pixels 320x240), SELECT deixou de fechar o app (sair = HOME ou quit()).
      Testado no New 3DS: todos os botões, sticks, ZL/ZR e toque ok (toque começa em ~4-5 px nas bordas,
      limite normal do painel); HOME abre/fecha 5x sem travar.
- [x] Medição real de memória: singleton `Platform3DS.get_memory_info()` (heap/linear/VRAM/região do app,
      com picos, `is_new_3ds`) e `OS.get_static_memory_*` reportando o heap real.
- [x] Correção crítica: `-fno-delete-null-pointer-checks` (GCC 6+ removia os `if (!this)` de
      `Object::cast_to` → crash com ponteiro nulo). Saída do Godot vai para o log do emulador (svcOutputDebugString).
- [x] Áudio: driver NDSP com membros inicializados e fallback para driver mudo quando `ndspInit` falha
      (sem `dspfirm.cdc`). Antes, `mix_rate` com lixo de pilha gerava alocação gigante no reverb → crash.
- Linha de base (Azahar, modo New 3DS, cena de teste vazia): 60 FPS, quadro 5,4 ms;
  heap 6,3 / 78,9 MB; linear 2,6 / 32 MB; VRAM 2,9 / 6 MB (≈ supersampling 800x480).
  `app_region_size` reportado (96 MB) é menor que o usado (124 MB) → não confiável; usar heap+linear+código.
- Azahar em modo Old 3DS dá 96 MB ao app (mais que o console real, ~64 MB via Homebrew Launcher):
  heap 6,3 / 50,9 MB, 60 FPS, quadro 6,0 ms.
- [x] Rebalancear heap x linear: opção `linear_heap_mb` (padrão 12, era 32 da libctru) → ~20 MB a mais de heap.
      Rever o valor quando as texturas forem para formatos nativos (Fase 3).
- [x] 1º teste em New 3DS real: svcBreak antes do main (heap de ~99 MB > área de heap de 96 MB).
      Corrigido com `__system_allocateHeaps` próprio: heap limitado a 96 MB, excedente vai para a linear.
- [x] New 3DS real: 60 FPS; heap 6,4 / 96,0 MB; linear 2,7 / 14,9 MB; VRAM 0,7 / 6,0 MB; app 124 MB.
- [x] Reduzir binário: módulos mpc/opus/webp/gridmap/ik/cscript desligados (13,45 → 13,11 MB de código);
      opção `optimize`: max (-O3) 13,11 MB · speed (-O2) 11,85 MB · size (-Os) 8,26 MB.
      Benchmark `misc/3ds-build/bench_project` no New 3DS, 200 sprites (script ms/quadro, FPS):
        max   804 MHz 18,92 ms 29,9 FPS | 268 MHz 72,21 ms 9,5 FPS
        speed 804 MHz 18,74 ms 29,9 FPS | 268 MHz 71,38 ms 9,6 FPS
        size  804 MHz 20,16 ms 29,9 FPS | 268 MHz 80,20 ms 8,4 FPS
      → padrão `speed` (-O2): mesma velocidade do -O3 com 1,3 MB a menos. `size` fica como opção
      para jogos que precisem de RAM. `--gc-sections` já vem do 3dsx.specs. LTO: a avaliar.
      Alerta para a Fase 4: a 268 MHz o GDScript domina o quadro (≈72 de ≈105 ms).

## Fase 1.5 — "GDScript 2.1+" e backports leves
Princípio: o que roda no PC (parser, editor, exportação) é barato; o que roda no console precisa caber em RAM/CPU.
Scripts exportados em modo **compilado** (bytecode) para o 3DS não compilar nada ao iniciar.
- Açúcar sintático (custo zero no console): `@export`/`@onready` como sinônimos, `super.metodo()`,
  `is` como alias de `extends`, `await x` → `yield`, aceitar anotações de tipo `: T` / `-> T` (ignoradas na VM).
- `move_and_slide()` / `is_on_floor()` em C++ no `KinematicBody2D`.
- Depois, se fizer falta: `match` (traduzido para if/elif), checagem de tipos só no editor,
  state machine de animação simples, `Tween` criado por código.
- Fora de escopo: lambdas/`Callable`/`Signal` de primeira classe, Audio Buses, AnimationTree/TileMap do 4.x.

## Fase 2 — Exporter real
- [x] `EditorExportPlatform3DS` (platform/3ds/export/export.cpp): template `.3dsx` + `data.pck` → RomFS → SMDH
      (título/descrição/autor/ícone 48x48 e 24x24) → `.3dsx`, sem devkitPro. RomFS/SMDH/3DSX validados byte a
      byte contra 3dsxtool/smdhtool (exceto a redução do ícone 24x24). Template em
      `~/.godot/templates/3ds_release.3dsx` (instalado pelo `build.sh template`). Testado: export headless
      (`-export "Nintendo 3DS"`) gera pck com script compilado (.gdc), cena binária e engine.cfb.
      Testado: exportação pelo editor (WSL) → .3dsx e .cia rodando no New 3DS.
- [x] `.cia` pelo editor: `binary/format = cia` chama bannertool + makerom (caminhos em Configurações do
      Editor → 3ds, ou PATH). Unique ID derivado do nome do projeto (ou definido), código do produto, banner
      PNG 256x128 + WAV (padrão: ícone sobre fundo azul + silêncio), RSF embutido (`misc/3ds-build/cia/app.rsf`)
      ou próprio, template `3ds_release.elf` (sem símbolos de depuração). RSF precisou mapear a memória do DSP
      e a VRAM (sem isso, ndspInit crashava no CIA). Testado no New 3DS: instala, banner aparece, roda.
- Opcional: "Run on device" via 3dslink; template de debug.

## Fase 3 — Assets e validações no editor
Conversão para formatos nativos já em tiles (reaproveitar módulo ETC1), fontes dinâmicas → bitmap,
áudio → PCM16/ADPCM (OGG só para música). Erros/avisos: textura >1024, não-pot2, shader de canvas,
sombras Light2D, orçamento de RAM/VRAM por cena. Preview 400x240 das duas telas.

## Bugs de plataforma corrigidos
- [x] `Semaphore3ds` era um esqueleto (`wait()` voltava na hora). Com a rede ligada, a thread de
      resolução de nomes do `IP` ficava num laço na prioridade da thread principal: tela preta em
      qualquer jogo. Agora usa `LightSemaphore` da libctru.

## Renderizador 3DS — pendências conhecidas
- [x] Texturas fora de potência de 2 (ex.: splash 400x240 → 512x256) saíam espremidas: UV usava o tamanho
      alocado como área da imagem; NinePatch/StyleBox tinham o erro inverso. Corrigido.
- [x] Linhas, círculos e polígonos (`canvas_draw_line/primitive/polygon` estavam vazios): o PICA200 não tem
      primitiva de linha, então linha vira quad fino; cor por vértice (gradiente) com loader de cor só
      quando preciso; UV de polígono ajustado ao padding. Cena de teste: `misc/3ds-build/draw_test`
      (validada no Azahar: igual ao PC, 60 FPS).
- [x] Recorte (clip: Label clip_text/autowrap, ScrollContainer) estava espelhado na vertical.
- [x] Texturas: filtro (FILTER) e repetição (REPEAT/tile) respeitados; tile em textura não potência de 2
      feito à mão (um quad por repetição, num só draw); padding preenchido com a borda (sem lixo ao filtrar).
      Textura com REPEAT e tamanho não potência de 2 é redimensionada para potência de 2 (como o GLES2
      faz sem NPOT): para repetição pixel-perfect, use 16/32/64/128...
- [x] Vazamentos: texturas liberadas nunca devolviam a memória linear (sem `C3D_TexDelete`); buffers de
      vértices eram liberados antes de a GPU terminar o quadro. Agora tudo é liberado no `begin_frame`.
- [ ] StyleBox/NinePatch: cada pedaço é um draw (9 por caixa); juntar em um só (Fase 4).
- [ ] Cada textura guarda também uma cópia RGBA em RAM (`Texture::image`): dobra o uso de memória das
      texturas. Remover a cópia (refazer `texture_get_data` lendo da textura da GPU) — Fase 1.5.
- [ ] `draw_primitive` com 1 ponto respeita `width` (quadrado); no PC (GLES2) o ponto tem sempre 1 px.
- [x] Tela de baixo e toque: ver Fase 5.

## Fase 4 — Desempenho
0. Antes de otimizar, medir (adiado da Fase 1): cronômetro por etapa do quadro (lógica, física, montagem
   do desenho, espera de GPU, vsync) + contadores de draw calls/vértices/trocas de textura via Platform3DS;
   benchmark em cenários isolados (só desenho, sprites por GDScript, por nós nativos, só matemática,
   limite de sprites a 60/30 FPS) com turbo on/off automático, gravando CSV em user://.
Batching no canvas, ring buffer de vértices, atlas, física 2D a 30 Hz.

## Fase 5 — Tela de baixo e toque
Escolhido: **área única 400x480** (`display/3ds_dual_screen = true`, width=400, height=480). A tela de cima
mostra y 0–240; a de baixo, x 40–360 e y 240–480 (320x240 centrada).
- [x] Renderizador: a cena é desenhada uma vez por tela (projeção, recorte e descarte por tela); itens que
      não aparecem numa tela não são desenhados nela. Splash só na tela de cima.
- [x] Toque nas coordenadas da área 400x480 (SCREEN_TOUCH/DRAG) e também como botão esquerdo do mouse:
      Button, Slider, ScrollContainer (arrastar rola) etc. funcionam pelo toque.
- [x] Editor: guias das duas telas na vista 2D, faixas fora da tela de baixo sombreadas; ao rodar no PC
      essas faixas ficam pretas, como no console. Projeto de teste: `misc/3ds-build/dual_screen_test`
      (validado no Azahar: botões, slider, lista rolável e desenho por toque).
- [ ] Teclado do sistema (swkbd) ao focar LineEdit/TextEdit (`OS::show_virtual_keyboard`).
- [ ] Viewport separado para a tela de baixo (mundos independentes nas duas telas), se algum jogo pedir.
- [ ] 3D estéreo por parallax (opcional).

## Fase 6 — Rede
- [x] Jogo local sem fio (UDS, até 16 consoles): singleton `LocalWireless`. Sala por jogo (ID e senha
  vêm de `application/name`); `host(max_jogadores, info, modo)`, `start_scan(modo)` em thread +
  `is_scanning()`/`get_rooms()`, `join(i)`, `leave()`, `get_status()`, `get_my_id()`, `get_players()`,
  `send(RawArray, para)`/`send_var(v, para)` (para 0 = todos), `get_packet_count()`, `get_packet()`/
  `get_var()`, `get_packet_sender()`. Pacotes até 1478 bytes. O RSF já libera `nwm::UDS`.
  Teste: `misc/3ds-build/local_wireless_test` — validado com dois Azahar numa sala multijogador
  (sincronização em tempo real) e entre dois consoles reais (New 3DS XL host a 60 FPS, Old 3DS XL
  cliente a 45 FPS: investigar custo por quadro no Old 3DS — chamadas IPC de status repetidas). No Azahar, cada instância precisa de nome de usuário, ID do console
  e MAC próprios (segunda cópia portátil com pasta `user` + Sistema → Regenerar).
- [~] Wi-Fi/Internet: sockets do Godot 2.1 ligados no 3DS (estavam fora por `UNIX_ENABLED`): `IP`
  (resolve_hostname, get_local_addresses via gethostid), `StreamPeerTCP`, `TCP_Server`, `PacketPeerUDP` e,
  por cima, `HTTPClient`. Só IPv4 (o 3DS não tem IPv6). O serviço SOC (1 MB de RAM) só é iniciado no
  primeiro uso. Sem HTTPS por enquanto (módulo openssl desligado; o SSL do sistema é antigo).
  Teste: `misc/3ds-build/network_test`. Não usar Wi-Fi e LocalWireless ao mesmo tempo.

## Fase 7 — StreetPass (experimental)
CECD pouco documentado e sem API pronta na libctru; só CIA; ID de StreetPass no SMDH. Fazer por último.

**Ordem combinada:** renderizador (linhas/polígonos) → Fase 5 → Fase 6 → Fase 7, com Fases 1.5/3/4 intercaladas
conforme a necessidade dos jogos.

## Editor (UI) — limpeza e usabilidade
- [~] Configurações do Projeto: a caixinha de "salvar no engine.cfg" (que parecia duplicar o valor nas
      propriedades booleanas) virou um botão de reverter (valor padrão + parar de salvar, com desfazer).
      `Globals` guarda o padrão de cada `GLOBAL_DEF`.
- [~] Exportação/Configurações: plataformas sem uso aqui ocultas (Android, BlackBerry 10, HTML5, iOS, PSP,
      Windows Universal) — lista `hidden_editor_platforms` no SConstruct. Ficam 3DS, Windows, Linux e macOS.
- [~] 3D oculto por padrão (aba 3D e nós Spatial no Criar Nó), já que o template do 3DS não tem 3D;
      Configurações do Editor → 3ds → Show 3D traz de volta.
- [~] Inspector: ↑/↓ no campo numérico (Shift ×10, Alt ×0,1), arraste horizontal (como no Godot 4) ou
      vertical, Shift = arraste fino, Ctrl = inteiros; ↑/↓ também nos campos de Vector2/Rect2/etc.
      Corrigido bug do 2.1 em que o valor decimal digitado no popup era descartado.
- [~] Vector2/Rect2/Vector3 com campos inline (linhas x/y, x/y/w/h, x/y/z coloridas) abaixo da propriedade,
      editáveis direto (arraste, digitação, ↑/↓); campos decimais sem limite usam passo 1 no teclado/arraste.
- [~] Campos numéricos no estilo `EditorSpinSlider` do Godot 4 (Tree::set_spin_slider_mode, só no Inspector):
      clicar e soltar edita no lugar sem popup/slider; arraste horizontal linear (Shift fino, Ctrl inteiro,
      Esc/botão direito cancela); setas só em inteiros; barrinha de slider para decimais com faixa;
      ↑/↓ com a regra do Godot 4 (Ctrl ×100, Shift ×10, Alt ×0,1).
- CI ignora pushes que só alteram arquivos `.md`.

## Riscos
Desempenho do GDScript 2.x no Old 3DS; efeitos limitados sem fragment shaders; manter o editor 2.1 compilando em toolchains modernas.
