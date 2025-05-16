#!/bin/bash
# Script pro ruční sestavení C++ OS

# Nastavení cest k nástrojům
NASM="/opt/homebrew/bin/nasm"
CXX="/opt/homebrew/bin/i386-elf-g++"
LD="/opt/homebrew/bin/i386-elf-ld"
QEMU="/opt/homebrew/bin/qemu-system-i386"

# Vytvoření build adresáře
mkdir -p build

# Sestavení assembly souborů
echo "Sestavení assembly souborů..."
for asm_file in src/*.asm; do
    base_name=$(basename "$asm_file" .asm)
    echo "  $asm_file -> build/$base_name.o"
    $NASM -f elf32 "$asm_file" -o "build/$base_name.o"
done

# Sestavení C++ souborů
echo "Sestavení C++ souborů..."
for cpp_file in src/*.cpp; do
    base_name=$(basename "$cpp_file" .cpp)
    echo "  $cpp_file -> build/$base_name.o"
    $CXX -c -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -I./src/include -I./src "$cpp_file" -o "build/$base_name.o"
done

# Sestavení knihovny
echo "Linkování..."
OBJ_FILES=$(find build -name "*.o")
$LD -m elf_i386 -T linker.ld -nostdlib -o build/kernel.bin $OBJ_FILES -lgcc

# Spuštění v QEMU
echo "Spuštění v QEMU..."
$QEMU -kernel build/kernel.bin
