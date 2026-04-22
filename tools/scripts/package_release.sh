#!/usr/bin/env bash
set -e

# Usage: ./package_release.sh [version]
VERSION=${1:-"v0.8.0-beta-nova"}
OS=${OS:-""}
ARCH=${ARCH:-""}
UNAME_S=$(uname -s)
UNAME_M=$(uname -m)

if [ -z "$OS" ]; then
    if [ "$UNAME_S" = "Darwin" ]; then
        OS="macos"
    elif [ "$UNAME_S" = "Linux" ]; then
        OS="linux"
    elif [[ "$UNAME_S" == MINGW* || "$UNAME_S" == CYGWIN* || "$UNAME_S" == MSYS* ]]; then
        OS="windows"
    else
        echo "Unsupported OS: $UNAME_S"
        exit 1
    fi
fi

if [ -z "$ARCH" ]; then
    if [ "$UNAME_M" = "x86_64" ]; then
        ARCH="x64"
    elif [ "$UNAME_M" = "arm64" ] || [ "$UNAME_M" = "aarch64" ]; then
        ARCH="arm64"
    else
        echo "Unsupported Arch: $UNAME_M"
        exit 1
    fi
fi

PACKAGE_NAME="toka-${VERSION}-${OS}-${ARCH}"
PACKAGE_DIR="build/${PACKAGE_NAME}"

echo "Packaging ${PACKAGE_NAME}..."

# Clean old directory
rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/bin"
mkdir -p "${PACKAGE_DIR}/lib"

# Copy binaries
if [ -f build/bin/tokac ] || [ -f build/bin/tokac.exe ]; then
    cp -a build/bin/toka build/bin/toka.exe "${PACKAGE_DIR}/bin/" 2>/dev/null || true
    cp -a build/bin/tokac build/bin/tokac.exe "${PACKAGE_DIR}/bin/" 2>/dev/null || true
    cp -a build/bin/tokafmt build/bin/tokafmt.exe "${PACKAGE_DIR}/bin/" 2>/dev/null || true
else
    echo "Error: tokac binary not found in build/bin/. Please build the compiler first."
    exit 1
fi

# Copy standard library
cp -a lib/* "${PACKAGE_DIR}/lib/"

# Copy meta files
cp README.md "${PACKAGE_DIR}/" || true
cp LICENSE "${PACKAGE_DIR}/" || true

# Archive
cd build
tar -czvf "${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}"
cd ..

echo ""
echo "✅ Packaged successfully: build/${PACKAGE_NAME}.tar.gz"
