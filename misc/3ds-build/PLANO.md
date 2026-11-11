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
- Render nativo 400x240 (remover supersampling 480x800 que gasta ~3 MB de VRAM).
- Texturas sem cópia na RAM; formatos nativos (RGB565, RGBA4, L8/A8, ETC1).
- `main_pack` lido de `romfs:/data.pck`; `user://` no SD; hooks de sleep/HOME (APT).
- Entrada: Circle Pad, C-Stick, touch; remover SELECT = sair; overlay de memória livre.
- [x] Medição real de memória: singleton `Platform3DS.get_memory_info()` (heap/linear/VRAM/região do app,
      com picos, `is_new_3ds`) e `OS.get_static_memory_*` reportando o heap real.
- [x] Correção crítica: `-fno-delete-null-pointer-checks` (GCC 6+ removia os `if (!this)` de
      `Object::cast_to` → crash com ponteiro nulo). Saída do Godot vai para o log do emulador (svcOutputDebugString).
- Reduzir binário: desligar módulos não usados, `-Os`, `--gc-sections`, LTO.

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
`EditorExportPlatform3DS` em C++: assets → `data.pck` → RomFS → SMDH (título/autor/ícone 48x48) → `.3dsx`.
Opcional: `.cia` via makerom + bannertool; "Run on device" via 3dslink.

## Fase 3 — Assets e validações no editor
Conversão para formatos nativos já em tiles (reaproveitar módulo ETC1), fontes dinâmicas → bitmap,
áudio → PCM16/ADPCM (OGG só para música). Erros/avisos: textura >1024, não-pot2, shader de canvas,
sombras Light2D, orçamento de RAM/VRAM por cena. Preview 400x240 das duas telas.

## Fase 4 — Desempenho
Batching no canvas, ring buffer de vértices, atlas, física 2D a 30 Hz.

## Fase 5 — Extras
Tela de baixo como Viewport secundário; 3D estéreo por parallax.

## Riscos
Desempenho do GDScript 2.x no Old 3DS; efeitos limitados sem fragment shaders; manter o editor 2.1 compilando em toolchains modernas.
