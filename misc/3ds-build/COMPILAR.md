# Como compilar (Windows + WSL)

Dois programas saem deste repositório:

| O quê | Para quê | Onde compila |
|---|---|---|
| **Editor Godot 2.1** (`bin/godot.x11.opt.tools.64`) | criar e testar o jogo no PC | direto no WSL (Ubuntu) |
| **Template 3DS** (`bin/godot.3ds.elf` → `.3dsx`) | rodar o jogo no 3DS | WSL com Docker **ou** devkitPro instalado no WSL |

Todos os comandos abaixo são digitados no terminal do **Ubuntu (WSL)**, não no PowerShell.

## 0. Preparar o WSL (uma vez)

No PowerShell do Windows, confira se o WSL está atualizado e com interface gráfica (WSLg):

```powershell
wsl --update
wsl --version        # precisa mostrar "Versão do WSLg"
```

Abra o Ubuntu e instale as ferramentas:

```bash
sudo apt update
sudo apt install -y git build-essential pkg-config scons \
    libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libasound2-dev libpulse-dev \
    libfreetype6-dev libssl-dev libudev-dev
```

Clone o repositório **dentro do Linux** (em `~`, não em `/mnt/c/...`: compilar a partir do disco do
Windows é muitas vezes mais lento):

```bash
cd ~
git clone https://github.com/RetroMoi/<nome-do-repo>.git godot-3ds
cd godot-3ds
```

Para atualizar depois: `git pull`.

## 1. Editor

```bash
scons -j$(nproc) platform=x11 tools=yes target=release_debug
```

A primeira compilação leva de 10 a 40 minutos, conforme o PC; as seguintes só recompilam o que mudou.
Para abrir o editor:

```bash
bin/godot.x11.opt.tools.64
```

A janela abre no Windows via WSLg. Abra (ou importe) a pasta `misc/3ds-build/test_project` para ver
o projeto de teste. Projetos podem ficar no disco do Windows (`/mnt/c/Users/<você>/...`).

Configuração recomendada para projetos 3DS (Cena → Configurações do Projeto → display):
`width=400`, `height=240`, `stretch_mode=2d`, `stretch_aspect=keep`.

## 2. Template 3DS e `.3dsx`

### Opção A — com Docker (mais simples)

Instale o **Docker Desktop** no Windows e ative em *Settings → Resources → WSL integration* o seu
Ubuntu. Depois, no Ubuntu:

```bash
WITH_EDITOR=0 misc/3ds-build/build.sh template                    # gera bin/godot.3ds.elf
PROJECT=misc/3ds-build/test_project misc/3ds-build/build.sh package  # gera bin/test_project.3dsx
```

`PROJECT` aceita qualquer pasta de projeto **dentro do repositório** (o container só enxerga o
repositório). Para um projeto seu, copie ou crie a pasta dentro dele, por exemplo `jogos/meu_jogo`.

### Opção B — devkitPro instalado no WSL (sem Docker)

```bash
wget https://apt.devkitpro.org/install-devkitpro-pacman
chmod +x ./install-devkitpro-pacman
sudo ./install-devkitpro-pacman
sudo dkp-pacman -S 3ds-dev 3ds-zlib 3ds-bzip2 3ds-libpng 3ds-freetype
# feche e reabra o terminal para carregar DEVKITPRO/DEVKITARM
```

Compilar e empacotar:

```bash
scons -j$(nproc) platform=3ds target=release tools=no disable_3d=yes
export PATH=$DEVKITPRO/tools/bin:$PATH
smdhtool --create "Meu Jogo" "Descrição" "Autor" misc/3ds-build/smdh_icon.png bin/jogo.smdh
3dsxtool bin/godot.3ds.elf bin/jogo.3dsx --smdh=bin/jogo.smdh --romfs=caminho/do/projeto
```

## 3. Exportar pelo editor (Exportar → Nintendo 3DS)

O editor gera o `.3dsx` sozinho, sem Docker nem devkitPro, a partir de um **template**: o motor
compilado para 3DS. Ele procura o template em `~/.godot/templates/3ds_release.3dsx`.

1. **Instale o template** (uma vez, e de novo sempre que o motor do 3DS mudar):
   - se compilou com a Opção A, o `build.sh template` já instala sozinho;
   - ou copie um `godot.3dsx` pronto:
     `mkdir -p ~/.godot/templates && cp godot.3dsx ~/.godot/templates/3ds_release.3dsx`
2. No editor, abra o projeto e vá em **Cena → Exportar** (ou **Projeto → Exportar**), escolha
   **Nintendo 3DS** e preencha as opções:
   - `application/title`, `description`, `author`: o que aparece no Homebrew Launcher
     (vazio = nome do projeto / "Made with Godot Engine" / "Unknown");
   - `application/icon`: PNG do ícone (vazio = ícone do projeto), redimensionado para 48x48;
   - `custom_template/release`: outro template `.3dsx`, se quiser usar um específico.
3. Clique em **Exportar** e salve como `meu_jogo.3dsx`.

Também funciona pela linha de comando (útil para scripts):

```bash
bin/godot.x11.opt.tools.64 -path caminho/do/projeto -export "Nintendo 3DS" /caminho/saida/jogo.3dsx
```

### Exportar como `.cia` (instalável no menu HOME)

Na exportação, troque `binary/format` para **cia**. O `.cia` aparece no menu HOME com ícone e banner
(a imagem grande que aparece na tela de cima ao selecionar o jogo) e é instalado com o **FBI**.

Precisa de mais duas coisas:

1. **Template ELF** em `~/.godot/templates/3ds_release.elf` (o `build.sh template` instala junto; ou
   copie um `3ds_release.elf` pronto para lá).
2. **Ferramentas `makerom` e `bannertool`.** No WSL, compile uma vez:

   ```bash
   sudo apt install -y cmake
   cd ~ && git clone --depth 1 https://github.com/3DSGuy/Project_CTR.git
   cd Project_CTR/makerom && make deps && make && sudo cp bin/makerom /usr/local/bin/
   cd ~ && git clone --depth 1 https://github.com/carstene1ns/3ds-bannertool.git
   cd 3ds-bannertool && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
   sudo cp build/bannertool /usr/local/bin/
   ```

   Em `/usr/local/bin` o editor as encontra sozinho. Se ficarem em outro lugar (ou no Windows, com
   `makerom.exe`/`bannertool.exe`), informe os caminhos em **Configurações do Editor → 3ds**.

Opções do CIA na exportação:

| Opção | Para quê | Vazio = |
|---|---|---|
| `cia/unique_id` | identifica o jogo instalado (hex, 0x00300–0xF7FFF). **Cada jogo precisa do seu**, senão um substitui o outro | gerado a partir do nome do projeto |
| `cia/product_code` | código do produto, formato `CTR-P-XXXX` | `CTR-P-GDOT` |
| `cia/banner_image` | PNG **256x128** do banner | ícone do projeto sobre fundo azul |
| `cia/banner_audio` | WAV curto (até ~3 s) tocado ao selecionar | silêncio |
| `cia/custom_rsf` | RSF próprio para o makerom (avançado) | `misc/3ds-build/cia/app.rsf` embutido |

## Duas telas (tela de baixo e toque)

Em **Configurações do Projeto → display** marque `3ds_dual_screen` e use `width=400`, `height=480`.
O jogo passa a ter uma área de 400x480:

```
+------------------400------------------+  y 0
|            tela de cima 400x240        |
+----+-----------------------------+----+  y 240
|    |  tela de baixo 320x240      |    |
|    |  (toque)                    |    |
+----+-----------------------------+----+  y 480
    x 40                         x 360
```

- O toque chega como `SCREEN_TOUCH`/`SCREEN_DRAG` nas coordenadas dessa área **e** como clique do botão
  esquerdo do mouse, então Button, Slider, ScrollContainer etc. funcionam tocando.
- As faixas x 0–40 e 360–400 abaixo de y 240 não aparecem no console: o editor as sombreia e, ao rodar no
  PC, elas ficam pretas.
- HUD fixo na tela de baixo com câmera andando no mundo: coloque a interface num `CanvasLayer`.
- Exemplo: `misc/3ds-build/dual_screen_test` (no pacote do Windows: `exemplo_duas_telas`).

## 4. Testar

- **Azahar**: Arquivo → Carregar arquivo → `bin/<nome>.3dsx` (no Windows, a pasta do WSL aparece em
  `\\wsl$\Ubuntu\home\<você>\godot-3ds\bin`).
- **Console**: copie o `.3dsx` para `/3ds/` no cartão SD e abra pelo Homebrew Launcher. Um `.cia`
  se instala pelo **FBI** (SD → arquivo → *Install CIA*) e aparece no menu HOME; remova pelo FBI → *Titles*.
- **Azahar** com `.cia`: Arquivo → Instalar CIA.

## Problemas comuns

- `scons: command not found` → faltou o `apt install` do passo 0.
- Erro sobre `pkg-config x11`/`xcursor`/... → falta algum pacote `lib*-dev` do passo 0.
- Editor não abre janela (`cannot open display`) → atualize o WSL (`wsl --update`) e reinicie
  (`wsl --shutdown` no PowerShell).
- `permission denied` no Docker → abra o Docker Desktop e confira a integração com o WSL.
