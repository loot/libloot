FROM mcr.microsoft.com/devcontainers/rust:2-1-trixie

RUN rustup target add x86_64-pc-windows-gnu

# Wine 10.19+ is needed to fix symlink behaviour.
# Without this some tests fail.
# <https://gitlab.winehq.org/wine/wine/-/wikis/Debian-Ubuntu>
RUN mkdir -pm755 /etc/apt/keyrings \
    && wget -O - https://dl.winehq.org/wine-builds/winehq.key | gpg --dearmor -o /etc/apt/keyrings/winehq-archive.key - \
    && dpkg --add-architecture i386 \
    && wget -NP /etc/apt/sources.list.d/ https://dl.winehq.org/wine-builds/debian/dists/trixie/winehq-trixie.sources

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    g++-mingw-w64-x86-64 \
    winehq-stable \
    && rm -rf /var/lib/apt/lists/*

# Run the Windows tests through Wine.
# Include the MinGW DLLs in Wine's search path so that running the C++ tests works.
# Set the locale to a UTF-8 locale so that the Unicode filename tests pass.
ENV CARGO_TARGET_X86_64_PC_WINDOWS_GNU_RUNNER=wine \
    WINEPATH=/usr/lib/gcc/x86_64-w64-mingw32/14-win32/ \
    LC_ALL=en_US.utf8
