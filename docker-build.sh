#!/bin/bash
# Skript pro sestavení OS pomocí Dockeru

# Sestavte Docker image, pokud neexistuje
docker image inspect hackers-os-dev >/dev/null 2>&1 || docker build -t hackers-os-dev .

# Spusťte kontejner s připojeným adresářem projektu
docker run -it --rm -v "$(pwd):/app" hackers-os-dev bash -c '
# Vytvoření build adresáře
mkdir -p build

# Sestavení assembly souborů
echo "Sestavení assembly souborů..."
for asm_file in src/*.asm; do
    base_name=$(basename "$asm_file" .asm)
    echo "  $asm_file -> build/$base_name.o"
    nasm -f elf32 "$asm_file" -o "build/$base_name.o"
done

# Sestavení C++ souborů
echo "Sestavení C++ souborů..."
for cpp_file in src/*.cpp; do
    base_name=$(basename "$cpp_file" .cpp)
    echo "  $cpp_file -> build/$base_name.o"
    g++ -c -m32 -ffreestanding -nostdlib -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -I./src/include -I./src "$cpp_file" -o "build/$base_name.o"
done

# Sestavení knihovny
echo "Linkování..."
OBJ_FILES=$(find build -name "*.o")
ld -m elf_i386 -T linker.ld -nostdlib -o build/kernel.bin $OBJ_FILES
'

# Spuštění v QEMU na hostiteli (macOS), pokud je k dispozici
if command -v qemu-system-i386 &> /dev/null; then
    echo "Spuštění v QEMU na hostiteli..."
    qemu-system-i386 -kernel build/kernel.bin
else
    echo "Pro spuštění v QEMU použijte Docker:"
    echo "docker run -it --rm -v \"$(pwd):/app\" hackers-os-dev qemu-system-i386 -kernel build/kernel.bin"
fi
