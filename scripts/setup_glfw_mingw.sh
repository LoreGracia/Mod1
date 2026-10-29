#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${GLFW_VERSION:-3.4}"
DEST_DIR="${ROOT_DIR}/external/glfw"
MODE="${1:-binary}"

if [[ "${MODE}" == "--source" ]]; then
	for tool in curl tar cmake mingw32-make; do
		if ! command -v "$tool" >/dev/null 2>&1; then
			echo "Missing required tool: $tool" >&2
			echo "Run this script from an MSYS2 MinGW-w64 shell." >&2
			exit 1
		fi
	done

	TMP_DIR="$(mktemp -d)"
	trap 'rm -rf "${TMP_DIR}"' EXIT

	ARCHIVE="${TMP_DIR}/glfw.tar.gz"
	URL="https://github.com/glfw/glfw/archive/refs/tags/${VERSION}.tar.gz"

	echo "Downloading GLFW ${VERSION} sources..."
	curl -fL "${URL}" -o "${ARCHIVE}"
	tar -xzf "${ARCHIVE}" -C "${TMP_DIR}"

	cmake -S "${TMP_DIR}/glfw-${VERSION}" -B "${TMP_DIR}/build" \
		-G "MinGW Makefiles" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="${DEST_DIR}" \
		-DGLFW_BUILD_EXAMPLES=OFF \
		-DGLFW_BUILD_TESTS=OFF \
		-DGLFW_BUILD_DOCS=OFF

	cmake --build "${TMP_DIR}/build" --parallel
	cmake --install "${TMP_DIR}/build"
else
	for tool in curl unzip; do
		if ! command -v "$tool" >/dev/null 2>&1; then
			echo "Missing required tool: $tool" >&2
			echo "On MSYS2: pacman -S curl unzip" >&2
			exit 1
		fi
	done

	ARCH="${GLFW_ARCH:-64}"
	case "${ARCH}" in
		64) PACKAGE="glfw-${VERSION}.bin.WIN64.zip" ;;
		32) PACKAGE="glfw-${VERSION}.bin.WIN32.zip" ;;
		*)
			echo "GLFW_ARCH must be 32 or 64." >&2
			exit 1
			;;
	esac

	TMP_DIR="$(mktemp -d)"
	trap 'rm -rf "${TMP_DIR}"' EXIT

	URL="https://github.com/glfw/glfw/releases/download/${VERSION}/${PACKAGE}"

	echo "Downloading ${PACKAGE}..."
	curl -fL "${URL}" -o "${TMP_DIR}/glfw.zip"
	unzip -q "${TMP_DIR}/glfw.zip" -d "${TMP_DIR}"

	PACKAGE_DIR="${TMP_DIR}/glfw-${VERSION}.bin.WIN${ARCH}"

	rm -rf "${DEST_DIR}/include" "${DEST_DIR}/lib-mingw-w64"
	mkdir -p "${DEST_DIR}"
	cp -R "${PACKAGE_DIR}/include" "${DEST_DIR}/include"
	cp -R "${PACKAGE_DIR}/lib-mingw-w64" "${DEST_DIR}/lib-mingw-w64"
fi

echo "GLFW ${VERSION} vendored in ${DEST_DIR}."
echo "Headers: ${DEST_DIR}/include/GLFW/glfw3.h"
echo "Libraries: ${DEST_DIR}/lib-mingw-w64"
