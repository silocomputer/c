#!/bin/sh
# Install c from GitHub Releases into ~/.local/bin (or PREFIX).
#
# One-liner:
#   curl -fsSL https://github.com/silocomputer/c/releases/latest/download/install.sh | sh -
#
# Optional: PREFIX=/usr/local … | sh -
# Optional: C_GITHUB_REPO=other/fork … | sh -

set -e

GITHUB_REPO="${C_GITHUB_REPO:-silocomputer/c}"

PREFIX="${PREFIX:-$HOME/.local}"
BIN_DIR="${PREFIX}/bin"

os=$(uname -s | tr '[:upper:]' '[:lower:]')
arch=$(uname -m)
case "$arch" in
  x86_64|amd64) arch=amd64 ;;
  aarch64|arm64) arch=arm64 ;;
  *)
    echo "c: unsupported arch: $arch" >&2
    exit 1
    ;;
esac

case "$os" in
  darwin|linux) ;;
  *)
    echo "c: unsupported OS: $os" >&2
    exit 1
    ;;
esac

asset="c-${os}-${arch}"
url="https://github.com/${GITHUB_REPO}/releases/latest/download/${asset}"

mkdir -p "$BIN_DIR"
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

echo "c: downloading $url"
curl -fsSL "$url" -o "$tmp"
chmod +x "$tmp"
mv "$tmp" "${BIN_DIR}/c"
trap - EXIT

echo "c: installed ${BIN_DIR}/c"
"${BIN_DIR}/c" version || true

case ":$PATH:" in
  *":${BIN_DIR}:"*) ;;
  *)
    echo "c: add to PATH: export PATH=\"${BIN_DIR}:\$PATH\"" >&2
    ;;
esac
