FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-aarch64 \
    mtools \
    dosfstools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

RUN cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=aarch64-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

CMD ["qemu-system-aarch64", \
     "-M", "virt", \
     "-cpu", "cortex-a72", \
     "-m", "256M", \
     "-nographic", \
     "-kernel", "build/kernel.bin", \
     "-serial", "mon:stdio"]
