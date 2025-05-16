// (zbytek projektu zůstává nezměněn)

/* ----- README.md ----- */
# Hackers OS

Minimalistický operační systém psaný v Rustu pro architekturu x86_64. Projekt běží v prostředí bez standardní knihovny (`no_std`) a bez operačního systému (`no_main`). Bootuje pomocí `bootloader` crate.

---

## 💻 Požadavky
- macOS (testováno na macOS 14+)
- Rust (nightly channel)
- QEMU
- `bootimage`
- LLVM (`lld` linker)

### Instalace nástrojů
```bash
rustup install nightly
rustup override set nightly
cargo install bootimage
brew install qemu llvm
```

Vytvoř `~/.cargo/config.toml`:
```toml
[target.x86_64-hackers_os]
linker = "lld"
```

---

## 🛠 Build a spuštění
```bash
make build     # Zkompiluje pro target
make image     # Vytvoří bootovací image pomocí bootimage
make run       # Spustí v QEMU
```

---

## 📂 Struktura
```
.
├── Cargo.toml
├── build.rs
├── linker.ld
├── Makefile
├── x86_64-hackers_os.json
├── README.md
└── src
    ├── lib.rs           # Entry point (_start)
    ├── memory.rs        # Frame allocator
    ├── interrupts.rs    # IDT, PIC, keyboard
    ├── serial.rs        # COM1 výstup
    └── vga_buffer.rs    # Textový výstup na obrazovku
```

---

## 🧪 Testování (volitelné)
Testování pomocí `#[test_case]` je podporováno, ale vyžaduje vlastní runner a QEMU výstup.

---

## 🧠 Další plány
- [ ] Paging a mapování paměti
- [ ] Heap allocator (`linked_list_allocator`)
- [ ] Shell s příkazy
- [ ] Podpora vícejádrovosti (APIC, SMP)
- [ ] VirtIO nebo fatfs loader

---

## 📜 Licence
MIT