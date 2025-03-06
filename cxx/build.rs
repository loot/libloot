fn main() {
    cxx_build::bridge("src/lib.rs")
        .std("c++17")
        .compile("libloot-cxx");

    println!("cargo:rerun-if-changed=src/lib.rs");
}
