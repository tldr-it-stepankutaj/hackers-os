#!/bin/bash
# Script pro ruční sestavení C++ OS na macOS bez cross-compileru

# Nastavení cest k nástrojům
NASM="/opt/homebrew/bin/nasm"
CXX="clang++"  # Použijeme standardní Apple clang
LD="ld"        # Standardní linker
QEMU="/opt/homebrew/bin/qemu-system-i386"

# Vytvoření build adresáře
mkdir -p build

# Sestavení assembly souborů
echo "Sestavení assembly souborů..."
for asm_file in src/*.asm; do
    base_name=$(basename "$asm_file" .asm)
    echo "  $asm_file -> build/$base_name.o"
    $NASM -f macho32 "$asm_file" -o "build/$base_name.o"  # Použijeme macho32 formát pro macOS
done

# Sestavení C++ souborů
echo "Sestavení C++ souborů..."
for cpp_file in src/*.cpp; do
    base_name=$(basename "$cpp_file" .cpp)
    echo "  $cpp_file -> build/$base_name.o"
    $CXX -c -m32 -msse -msse2 -ffreestanding -fno-pie -fno-stack-protector -fno-rtti -fno-exceptions -I./src/include -I./src "$cpp_file" -o "build/$base_name.o"
done

# Sestavení knihovny
echo "Linkování..."
OBJ_FILES=$(find build -name "*.o")
$LD -macosx_version_min 10.12 -static -o build/kernel.bin $OBJ_FILES -lSystem

# Spuštění v QEMU
echo "Spuštění v QEMU..."
$QEMU -kernel build/kernel.bin
