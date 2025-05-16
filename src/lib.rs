#![no_std]
#![no_main]
#![feature(abi_x86_interrupt)]

pub mod vga_buffer;
pub mod serial;
pub mod memory;
pub mod interrupts;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    println!("Hackers OS booting...");
    init();
    loop {}
}

pub fn init() {
    interrupts::init();
    memory::init();
}