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

## 3. Testar

- **Azahar**: Arquivo → Carregar arquivo → `bin/<nome>.3dsx` (no Windows, a pasta do WSL aparece em
  `\\wsl$\Ubuntu\home\<você>\godot-3ds\bin`).
- **Console**: copie o `.3dsx` para `/3ds/` no cartão SD e abra pelo Homebrew Launcher.

## Problemas comuns

- `scons: command not found` → faltou o `apt install` do passo 0.
- Erro sobre `pkg-config x11`/`xcursor`/... → falta algum pacote `lib*-dev` do passo 0.
- Editor não abre janela (`cannot open display`) → atualize o WSL (`wsl --update`) e reinicie
  (`wsl --shutdown` no PowerShell).
- `permission denied` no Docker → abra o Docker Desktop e confira a integração com o WSL.
