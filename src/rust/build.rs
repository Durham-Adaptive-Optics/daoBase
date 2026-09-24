// build.rs
//
// Generates the raw FFI bindings from daoBase/include/dao.h via bindgen
// (see wrapper.h), and tells cargo where to find and link against libdao.

use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rerun-if-env-changed=DAOROOT");

    // Prefer an installed daoBase (`waf install`, see ../../install.sh),
    // which sets $DAOROOT. Also add the in-tree waf build output, so this
    // crate builds against a checkout that has only been `waf build`t.
    if let Ok(daoroot) = env::var("DAOROOT") {
        println!("cargo:rustc-link-search=native={daoroot}/lib");
        println!("cargo:rustc-link-search=native={daoroot}/lib64");
    }
    println!("cargo:rustc-link-search=native=../../build/src");
    println!("cargo:rustc-link-lib=dylib=dao");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        // Layout-test assertions use `core::mem::offset_of!`, stable only
        // since Rust 1.77 - skip them so this builds on older toolchains
        // too (the struct layout is still generated correctly either way,
        // this only disables bindgen's own self-check of it).
        .layout_tests(false)
        .allowlist_type("IMAGE")
        .allowlist_type("IMAGE_METADATA")
        .allowlist_type("IMAGE_KEYWORD")
        .allowlist_function("daoShm.*")
        .allowlist_function("daoSem.*")
        .allowlist_function("daoSetLogLevel")
        .allowlist_function("daoGetLogLevel")
        .allowlist_var("_DATATYPE_.*")
        .allowlist_var("DAO_.*")
        .generate()
        .expect("Unable to generate bindings from dao.h - is libclang installed?");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
