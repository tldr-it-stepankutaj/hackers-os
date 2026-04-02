#!/bin/bash
# Build companion apps as static aarch64-linux binaries
# for inclusion in HackersOS disk image.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
APPS_DIR="$PROJECT_DIR/apps"
DEPS_DIR="$PROJECT_DIR/build/deps"
REPO_ROOT="$(dirname "$PROJECT_DIR")"

# Detect cross-compiler prefix
if command -v aarch64-unknown-linux-gnu-gcc &>/dev/null; then
    CROSS=aarch64-unknown-linux-gnu
elif command -v aarch64-linux-gnu-gcc &>/dev/null; then
    CROSS=aarch64-linux-gnu
else
    echo "Error: No aarch64 cross-compiler found"
    echo "Install with: brew tap messense/macos-cross-toolchains && brew install aarch64-unknown-linux-gnu"
    exit 1
fi

echo "Cross-compiler: $CROSS"
echo ""

mkdir -p "$APPS_DIR" "$DEPS_DIR"

# ============================================================
# 1. dnsutils (Go) — pure Go, trivial
# ============================================================
build_dnsutils() {
    echo "=== Building dnsutils (securitydns) ==="
    local SRC="$REPO_ROOT/dnsutils"
    if [ ! -d "$SRC" ]; then
        echo "  [skip] $SRC not found"
        return
    fi
    cd "$SRC"
    CGO_ENABLED=0 GOOS=linux GOARCH=arm64 \
        go build -ldflags='-s -w' -o "$APPS_DIR/securitydns" ./cmd/main.go
    echo "  [ok] $(ls -lh "$APPS_DIR/securitydns" | awk '{print $5}') — $APPS_DIR/securitydns"
}

# ============================================================
# 2. Build static OpenSSL 3.x for aarch64
# ============================================================
build_openssl() {
    local OPENSSL_VERSION="3.3.1"
    local OPENSSL_DIR="$DEPS_DIR/openssl-$OPENSSL_VERSION"
    local OPENSSL_INSTALL="$DEPS_DIR/openssl-install"

    if [ -f "$OPENSSL_INSTALL/lib/libcrypto.a" ]; then
        echo "  [cached] OpenSSL $OPENSSL_VERSION already built"
        return
    fi

    echo "  Downloading OpenSSL $OPENSSL_VERSION..."
    cd "$DEPS_DIR"
    if [ ! -d "$OPENSSL_DIR" ]; then
        curl -sL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz" | tar xz
    fi

    echo "  Building OpenSSL for aarch64 (static)..."
    cd "$OPENSSL_DIR"
    ./Configure linux-aarch64 \
        --cross-compile-prefix="${CROSS}-" \
        --prefix="$OPENSSL_INSTALL" \
        no-shared no-dso no-engine no-tests no-ui-console \
        -static 2>&1 | tail -3

    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) 2>&1 | tail -3
    make install_sw 2>&1 | tail -3
    echo "  [ok] OpenSSL installed to $OPENSSL_INSTALL"
}

# ============================================================
# 3. Build static libcurl for aarch64
# ============================================================
build_curl() {
    local CURL_VERSION="8.7.1"
    local CURL_DIR="$DEPS_DIR/curl-$CURL_VERSION"
    local CURL_INSTALL="$DEPS_DIR/curl-install"
    local OPENSSL_INSTALL="$DEPS_DIR/openssl-install"

    if [ -f "$CURL_INSTALL/lib/libcurl.a" ]; then
        echo "  [cached] libcurl $CURL_VERSION already built"
        return
    fi

    echo "  Downloading libcurl $CURL_VERSION..."
    cd "$DEPS_DIR"
    if [ ! -d "$CURL_DIR" ]; then
        curl -sL "https://curl.se/download/curl-${CURL_VERSION}.tar.gz" | tar xz
    fi

    echo "  Building libcurl for aarch64 (static)..."
    cd "$CURL_DIR"

    CFLAGS="-I${OPENSSL_INSTALL}/include" \
    LDFLAGS="-L${OPENSSL_INSTALL}/lib" \
    CFLAGS="-I${OPENSSL_INSTALL}/include" \
    LDFLAGS="-L${OPENSSL_INSTALL}/lib" \
    LIBS="-lssl -lcrypto -lpthread -ldl" \
    ./configure \
        --host=${CROSS} \
        --prefix="$CURL_INSTALL" \
        --enable-static --disable-shared \
        --with-openssl="$OPENSSL_INSTALL" \
        --without-libpsl --without-brotli --without-zstd \
        --without-nghttp2 --without-libidn2 \
        --disable-ldap --disable-ldaps --disable-rtsp \
        --disable-dict --disable-telnet --disable-tftp \
        --disable-pop3 --disable-imap --disable-smb \
        --disable-smtp --disable-gopher --disable-mqtt \
        --disable-manual --disable-docs --disable-ntlm \
        2>&1 | tail -3

    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) 2>&1 | tail -3
    make install 2>&1 | tail -3
    echo "  [ok] libcurl installed to $CURL_INSTALL"
}

# ============================================================
# 4. network-scanner (C++)
# ============================================================
build_network_scanner() {
    echo "=== Building network-scanner (network-analyzer) ==="
    local SRC="$REPO_ROOT/network-scanner"
    if [ ! -d "$SRC" ]; then
        echo "  [skip] $SRC not found"
        return
    fi

    local OPENSSL_INSTALL="$DEPS_DIR/openssl-install"
    local CURL_INSTALL="$DEPS_DIR/curl-install"

    # Build deps if needed
    if [ ! -f "$CURL_INSTALL/lib/libcurl.a" ]; then
        build_openssl
        build_curl
    fi

    echo "  Cross-compiling network-scanner..."
    local BUILD="$DEPS_DIR/network-scanner-build"
    mkdir -p "$BUILD"

    cmake -B "$BUILD" -S "$SRC" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_C_COMPILER="${CROSS}-gcc" \
        -DCMAKE_CXX_COMPILER="${CROSS}-g++" \
        -DCMAKE_FIND_ROOT_PATH="$CURL_INSTALL;$OPENSSL_INSTALL" \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCURL_LIBRARY="$CURL_INSTALL/lib/libcurl.a" \
        -DCURL_INCLUDE_DIR="$CURL_INSTALL/include" \
        -DOPENSSL_ROOT_DIR="$OPENSSL_INSTALL" \
        -DOPENSSL_CRYPTO_LIBRARY="$OPENSSL_INSTALL/lib/libcrypto.a" \
        -DOPENSSL_SSL_LIBRARY="$OPENSSL_INSTALL/lib/libssl.a" \
        -DCMAKE_EXE_LINKER_FLAGS="-static -Wl,--allow-multiple-definition" \
        -DCMAKE_CXX_STANDARD_LIBRARIES="-L$CURL_INSTALL/lib -L$OPENSSL_INSTALL/lib -lcurl -lssl -lcrypto -lpthread -ldl" \
        -DCMAKE_CXX_FLAGS="-static" \
        -DBUILD_TESTS=OFF \
        2>&1 | tail -5

    cmake --build "$BUILD" --parallel 2>&1 | tail -10

    # Find the built binary
    local BIN=$(find "$BUILD" -name "network-analyzer" -type f 2>/dev/null | head -1)
    if [ -n "$BIN" ] && [ -f "$BIN" ]; then
        cp "$BIN" "$APPS_DIR/network-analyzer"
        echo "  [ok] $(ls -lh "$APPS_DIR/network-analyzer" | awk '{print $5}') — $APPS_DIR/network-analyzer"
    else
        echo "  [FAIL] Binary not found in $BUILD"
    fi
}

# ============================================================
# 5. jwt_inspector (C++)
# ============================================================
build_jwt_inspector() {
    echo "=== Building jwt_inspector ==="
    local SRC="$REPO_ROOT/jwt_inspector"
    if [ ! -d "$SRC" ]; then
        echo "  [skip] $SRC not found"
        return
    fi

    local OPENSSL_INSTALL="$DEPS_DIR/openssl-install"

    # Build OpenSSL if needed
    if [ ! -f "$OPENSSL_INSTALL/lib/libcrypto.a" ]; then
        build_openssl
    fi

    echo "  Cross-compiling jwt_inspector..."
    local BUILD="$DEPS_DIR/jwt-inspector-build"
    mkdir -p "$BUILD"

    cmake -B "$BUILD" -S "$SRC" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_C_COMPILER="${CROSS}-gcc" \
        -DCMAKE_CXX_COMPILER="${CROSS}-g++" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DOPENSSL_ROOT_DIR="$OPENSSL_INSTALL" \
        -DOPENSSL_CRYPTO_LIBRARY="$OPENSSL_INSTALL/lib/libcrypto.a" \
        -DOPENSSL_SSL_LIBRARY="$OPENSSL_INSTALL/lib/libssl.a" \
        -DOPENSSL_INCLUDE_DIR="$OPENSSL_INSTALL/include" \
        -DENABLE_OPENCL=OFF \
        -DCMAKE_EXE_LINKER_FLAGS="-static -Wl,--allow-multiple-definition" \
        -DCMAKE_CXX_STANDARD_LIBRARIES="-L$OPENSSL_INSTALL/lib -lssl -lcrypto -lpthread -ldl" \
        -DCMAKE_CXX_FLAGS="-static -I$OPENSSL_INSTALL/include" \
        2>&1 | tail -5

    cmake --build "$BUILD" --parallel 2>&1 | tail -10

    local BIN=$(find "$BUILD" -name "jwt_inspector" -type f ! -name "*.o" ! -name "*.cpp" 2>/dev/null | head -1)
    if [ -n "$BIN" ] && [ -f "$BIN" ]; then
        cp "$BIN" "$APPS_DIR/jwt_inspector"
        echo "  [ok] $(ls -lh "$APPS_DIR/jwt_inspector" | awk '{print $5}') — $APPS_DIR/jwt_inspector"
    else
        echo "  [FAIL] Binary not found in $BUILD"
    fi
}

# ============================================================
# Main
# ============================================================
echo "========================================"
echo "  Building HackersOS Companion Apps"
echo "  Target: aarch64-linux (static)"
echo "========================================"
echo ""

build_dnsutils
echo ""
build_network_scanner
echo ""
build_jwt_inspector

echo ""
echo "========================================"
echo "  Results:"
echo "========================================"
ls -lh "$APPS_DIR"/ 2>/dev/null
echo ""
echo "Run 'make disk-image' to create a FAT32 image with these apps."
