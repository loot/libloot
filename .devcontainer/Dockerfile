FROM mcr.microsoft.com/devcontainers/rust:2-1-trixie

RUN rustup target add x86_64-pc-windows-gnu \
    && rustup component add llvm-tools-preview

# Wine 10.19+ is needed to fix symlink behaviour.
# Without this some tests fail.
# <https://gitlab.winehq.org/wine/wine/-/wikis/Debian-Ubuntu>
RUN mkdir -pm755 /etc/apt/keyrings \
    && wget -O - https://dl.winehq.org/wine-builds/winehq.key | gpg --dearmor -o /etc/apt/keyrings/winehq-archive.key - \
    && dpkg --add-architecture i386 \
    && wget -NP /etc/apt/sources.list.d/ https://dl.winehq.org/wine-builds/debian/dists/trixie/winehq-trixie.sources

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    doxygen \
    g++-mingw-w64-x86-64 \
    nodejs \
    npm \
    winehq-stable \
    && rm -rf /var/lib/apt/lists/*

USER vscode
WORKDIR /home/vscode

RUN cargo install cargo-vet --version 0.10.2

RUN curl --proto '=https' --tlsv1.2 -sSfLO https://github.com/taiki-e/cargo-llvm-cov/releases/download/v0.8.2/cargo-llvm-cov-x86_64-unknown-linux-gnu.tar.gz \
    && echo "6bd0fba0e70f8399fb4f279e4e00992860edaa9ff7e460cb9b5f924e6018c7eb cargo-llvm-cov-x86_64-unknown-linux-gnu.tar.gz" | sha256sum -c \
    && tar xzf cargo-llvm-cov-x86_64-unknown-linux-gnu.tar.gz -C "/usr/local/cargo/bin" \
    && rm cargo-llvm-cov-x86_64-unknown-linux-gnu.tar.gz

RUN curl --proto '=https' --tlsv1.2 -sSfLO https://github.com/astral-sh/uv/releases/download/0.9.28/uv-x86_64-unknown-linux-gnu.tar.gz \
    && echo "66ad1822dd9cf96694b95c24f25bc05cff417a65351464da01682a91796d1f2b uv-x86_64-unknown-linux-gnu.tar.gz" | sha256sum -c \
    && mkdir -p /home/vscode/.local/bin \
    && tar -xzf uv-x86_64-unknown-linux-gnu.tar.gz -C /home/vscode/.local/bin --strip-components=1 \
    && rm uv-x86_64-unknown-linux-gnu.tar.gz

# Run the Windows tests through Wine.
# Include the MinGW DLLs in Wine's search path so that running the C++ tests works.
# Set the locale to a UTF-8 locale so that the Unicode filename tests pass.
ENV CARGO_TARGET_X86_64_PC_WINDOWS_GNU_RUNNER=wine \
    WINEPATH=/usr/lib/gcc/x86_64-w64-mingw32/14-win32/ \
    LC_ALL=en_US.utf8
