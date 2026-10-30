#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${GLFW_VERSION:-3.4}"
DEST_DIR="${ROOT_DIR}/external/glfw"

if pkg-config --exists glfw3 2>/dev/null; then
	echo "GLFW $(pkg-config --modversion glfw3) is already available through pkg-config."
	exit 0
fi

if [[ "${1:-}" == "--system" ]]; then
	if command -v apt-get >/dev/null 2>&1; then
		echo "Installing libglfw3-dev with apt..."
		sudo apt-get update
		sudo apt-get install -y libglfw3-dev
	else
		echo "Install the GLFW development package for your distribution." >&2
		exit 1
	fi
	echo "Verify the installation with: pkg-config --modversion glfw3"
	exit 0
fi

for tool in curl tar cmake make; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "Missing required tool: $tool" >&2
		echo "On Debian/Ubuntu: sudo apt-get install -y curl tar cmake build-essential" >&2
		exit 1
	fi
done

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

ARCHIVE="${TMP_DIR}/glfw.tar.gz"
URL="https://github.com/glfw/glfw/archive/refs/tags/${VERSION}.tar.gz"

echo "Downloading GLFW ${VERSION}..."
curl -fL "${URL}" -o "${ARCHIVE}"
tar -xzf "${ARCHIVE}" -C "${TMP_DIR}"

SOURCE_DIR="${TMP_DIR}/glfw-${VERSION}"
BUILD_DIR="${TMP_DIR}/build"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="${DEST_DIR}" \
	-DGLFW_BUILD_EXAMPLES=OFF \
	-DGLFW_BUILD_TESTS=OFF \
	-DGLFW_BUILD_DOCS=OFF

cmake --build "${BUILD_DIR}" --parallel
cmake --install "${BUILD_DIR}"

echo "GLFW installed in ${DEST_DIR}."
echo "The Makefile can use it through external/glfw/lib/pkgconfig/glfw3.pc or its vendored fallback."
