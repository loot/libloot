fn main() {
    cxx_build::bridge("src/lib.rs")
        .std("c++17")
        .flag_if_supported("/Zc:__cplusplus")
        .flag_if_supported("/permissive-")
        .compile("libloot-cpp");

    // From <https://github.com/dtolnay/cxx/issues/880#issuecomment-2521375384>
    if std::env::var("TARGET").is_ok_and(|s| s.contains("windows-msvc")) {
        // MSVC compiler suite
        if std::env::var("CFLAGS").is_ok_and(|s| s.contains("/MDd")) {
            // debug runtime flag is set

            // Don't link the default CRT
            println!("cargo::rustc-link-arg=/nodefaultlib:msvcrt");
            // Link the debug CRT instead
            println!("cargo::rustc-link-arg=/defaultlib:msvcrtd");
        }
    }
}
