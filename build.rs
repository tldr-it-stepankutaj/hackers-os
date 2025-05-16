// build.rs
use std::process::Command;
use std::env;
use std::path::Path;

fn main() {
    // The bootloader crate will handle creating a bootable disk image
    let out_dir = env::var("OUT_DIR").unwrap();
    let kernel = Path::new(&out_dir).join("kernel");

    // Tell cargo to rerun this script if any of these files change
    println!("cargo:rerun-if-changed=src/");
    println!("cargo:rerun-if-changed=Cargo.toml");
    println!("cargo:rerun-if-changed=build.rs");
}