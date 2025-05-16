FROM ubuntu:22.04

# Instalace vývojových nástrojů
RUN apt-get update && apt-get install -y \
    build-essential \
    nasm \
    gcc-multilib \
    g++-multilib \
    qemu-system-x86 \
    gdb \
    vim \
    make

WORKDIR /app
VOLUME /app

CMD ["/bin/bash"]
