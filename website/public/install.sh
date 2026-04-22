#!/bin/sh
set -e

# Toka Language Installer
# This script installs the Toka language toolchain (tokac, toka, tokafmt + stdlib).

echo "Installing Toka Language..."

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

if [ "$OS" = "darwin" ]; then
  OS="macos"
elif echo "$OS" | grep -qE "(mingw|msys|cygwin)"; then
  OS="windows"
fi

if [ "$ARCH" = "x86_64" ]; then
  ARCH="x64"
elif [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
  ARCH="arm64"
fi

# We use 'latest' endpoint or we let user specify version.
# For now, it curls latest.
RELEASE_URL="https://github.com/zhyi-dp/tokalang/releases/latest/download"

# To specify a specific version for beta, use the argument passed or latest
VERSION=${1:-"latest"}
if [ "$VERSION" = "latest" ]; then
  # Grab latest release tag via GitHub API
  echo "Fetching latest version..."
  VERSION=$(curl -sSL "https://api.github.com/repos/zhyi-dp/tokalang/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
  if [ -z "$VERSION" ]; then
    # Fallback if API rate limited or no latest release marked
    VERSION="v0.8.0-beta"
    echo "Warning: Could not determine latest version. Defaulting to $VERSION"
  fi
fi

TARBALL="toka-${VERSION}-${OS}-${ARCH}.tar.gz"
DOWNLOAD_URL="https://github.com/zhyi-dp/tokalang/releases/download/${VERSION}/${TARBALL}"

TOKA_DIR="$HOME/.toka"
TMP_DIR=$(mktemp -d)

# Download
echo "Downloading $TARBALL from $DOWNLOAD_URL..."
curl -# -sL -o "${TMP_DIR}/${TARBALL}" "$DOWNLOAD_URL"

# Extract
rm -rf "${TOKA_DIR}"
mkdir -p "${TOKA_DIR}"
echo "Extracting..."
tar -xzf "${TMP_DIR}/${TARBALL}" -C "$TMP_DIR"

# Move files to ~/.toka
# The tarball unzips as an inner directory named toka-VERSION-OS-ARCH.
INNER_DIR="${TMP_DIR}/toka-${VERSION}-${OS}-${ARCH}"
cp -a "${INNER_DIR}/"* "${TOKA_DIR}/"
rm -rf "$TMP_DIR"

echo "Toka Language has been installed to ${TOKA_DIR}."

# Add to path
BIN_DIR="${TOKA_DIR}/bin"
export PATH="$BIN_DIR:$PATH"

PROFILE_FILE=""
if [ -n "$BASH_VERSION" ]; then
  if [ -f "$HOME/.bash_profile" ]; then PROFILE_FILE="$HOME/.bash_profile"; 
  elif [ -f "$HOME/.bashrc" ]; then PROFILE_FILE="$HOME/.bashrc"; fi
elif [ -n "$ZSH_VERSION" ] || [ "$(basename "$SHELL")" = "zsh" ]; then
  PROFILE_FILE="$HOME/.zshrc"
fi

# Fallback profile search
if [ -z "$PROFILE_FILE" ]; then
  if [ -f "$HOME/.zshrc" ]; then PROFILE_FILE="$HOME/.zshrc"
  elif [ -f "$HOME/.bash_profile" ]; then PROFILE_FILE="$HOME/.bash_profile"
  elif [ -f "$HOME/.bashrc" ]; then PROFILE_FILE="$HOME/.bashrc"
  fi
fi

if [ -n "$PROFILE_FILE" ]; then
  if ! grep -q 'export PATH="\$HOME/.toka/bin:\$PATH"' "$PROFILE_FILE"; then
    echo "export PATH=\"\$HOME/.toka/bin:\$PATH\"" >> "$PROFILE_FILE"
    echo "export TOKA_LIB=\"\$HOME/.toka/lib\"" >> "$PROFILE_FILE"
    echo "Added \$HOME/.toka/bin to PATH and set TOKA_LIB in $PROFILE_FILE."
    echo "Please restart your shell or run: source $PROFILE_FILE"
  fi
else
  echo "Please add $BIN_DIR to your PATH manually."
fi

echo ""
echo "Welcome to Toka! Run 'tokac --help' to get started."
