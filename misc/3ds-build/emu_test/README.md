# Teste automático no emulador (sem tela)

Roda um `.3dsx` no core libretro do Azahar com o renderizador por software, aperta botões
segundo um roteiro, salva capturas de tela e mede o som emitido. Serve para validar mudanças
do renderizador, do áudio e da lógica sem um console. (Os tempos mostrados pelo jogo vêm do
relógio emulado: servem para comparar versões, não como medida do hardware.)

## Preparar (uma vez)

```sh
git clone --depth 1 https://github.com/azahar-emu/azahar && cd azahar
git submodule update --init --recursive --depth 1
cmake -B build-lr -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_LIBRETRO=ON \
  -DENABLE_OPENGL=OFF -DENABLE_VULKAN=OFF -DENABLE_TESTS=OFF -DENABLE_WEB_SERVICE=OFF \
  -DENABLE_ROOM=OFF -DENABLE_SCRIPTING=OFF -DCITRA_WARNINGS_AS_ERRORS=OFF
ninja -C build-lr          # gera build-lr/bin/Release/azahar_libretro.so
gcc -O2 -o lr_harness lr_harness.c -I<azahar>/externals/libretro-common/libretro-common/include -ldl
```

Coloque `lr_harness` e `azahar_libretro.so` na mesma pasta de `run3ds.sh`.

## Usar

```sh
# empacota o projeto com bin/godot.3ds.elf e roda 480 quadros
./run3ds.sh <pasta do projeto> 480 <saída> roteiro.txt
LR_OLD3DS=1 ...   # emula um 3DS antigo
LR_AUDIO=1 ...    # imprime o pico de áudio de cada quadro com som
```

Roteiro (um comando por linha): `shot N` (captura após o quadro N), `press N BOTÃO`,
`release N BOTÃO` (A B X Y L R START SELECT UP DOWN LEFT RIGHT).
