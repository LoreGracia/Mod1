#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/external/glad"

if ! command -v glad >/dev/null 2>&1; then
	echo "GLAD 2.x CLI not found." >&2
	echo "Install it with: python3 -m venv /tmp/gladenv && /tmp/gladenv/bin/pip install glad2" >&2
	echo "Or generate the files at https://glad.sh with: api=gl:core=3.3, extensions empty, loader disabled."
	exit 1
fi

if ! glad --version 2>&1 | grep -q '^2\.'; then
	echo "This script requires GLAD 2.x (the glad2 Python package)." >&2
	exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

glad --api 'gl:core=3.3' --extensions '' --out-path "${TMP_DIR}" --reproducible c

mkdir -p "${OUT_DIR}/include/glad" "${OUT_DIR}/include/KHR" "${OUT_DIR}/src"
cp "${TMP_DIR}/include/glad/gl.h" "${OUT_DIR}/include/glad/gl.h"
cp "${TMP_DIR}/include/KHR/khrplatform.h" "${OUT_DIR}/include/KHR/khrplatform.h"
cp "${TMP_DIR}/src/gl.c" "${OUT_DIR}/src/gl.c"

rm -f "${OUT_DIR}/include/glad/glad_glx.h" "${OUT_DIR}/src/glx.c"

echo "GLAD regenerated in ${OUT_DIR} (gl:core=3.3, core profile, no loader, no extensions)."
