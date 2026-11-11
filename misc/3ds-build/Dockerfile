# syntax=docker/dockerfile:1
# Ambiente de build do Godot 2.1 para Nintendo 3DS (template) + editor Linux (opcional).
FROM devkitpro/devkitarm:latest

# WITH_EDITOR=1 instala as libs X11/GL/áudio do Debian para compilar o editor (platform=x11).
ARG WITH_EDITOR=1

# SCons é Python puro: instalado a partir da wheel em vendor/ (a imagem não tem pip).
COPY vendor/SCons-4.8.1-py3-none-any.whl /tmp/
RUN python3 -m zipfile -e /tmp/SCons-4.8.1-py3-none-any.whl /opt/scons \
    && printf '#!/bin/sh\nPYTHONPATH=/opt/scons exec python3 -c "from SCons.Script.Main import main; main()" "$@"\n' > /usr/local/bin/scons \
    && chmod +x /usr/local/bin/scons && rm /tmp/*.whl

# CA extra opcional (só para redes com proxy TLS): docker build --secret id=ca,src=ca.crt
RUN --mount=type=secret,id=ca,required=false \
    set -eux; \
    if [ -s /run/secrets/ca ]; then \
        echo "Acquire::https::CAInfo \"/run/secrets/ca\";" > /etc/apt/apt.conf.d/99ca; \
    fi; \
    if [ "$WITH_EDITOR" = 1 ]; then \
        rm -f /etc/apt/sources.list.d/devkitpro.list; \
        apt-get update; \
        apt-get install -y --no-install-recommends \
            build-essential pkg-config \
            libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev \
            libgl1-mesa-dev libglu1-mesa-dev libasound2-dev libpulse-dev \
            libfreetype6-dev libssl-dev libudev-dev; \
        rm -rf /var/lib/apt/lists/*; \
    fi; \
    rm -f /etc/apt/apt.conf.d/99ca

ENV PATH=${DEVKITPRO}/tools/bin:${DEVKITARM}/bin:${PATH}
WORKDIR /src
