#!/usr/bin/env bash
set -euo pipefail

# ── Directorios a analizar ──────────────────────────────────────────
# Por defecto: src, inc, Wrapper, test. external/ NUNCA se incluye.
read -ra LINT_DIRS <<< "${LINT_DIRS:-src inc Wrapper test}"

echo "Analizando: ${LINT_DIRS[*]}"
echo

# ── Recoger archivos de TODOS los directorios ───────────────────────
files=()
for d in "${LINT_DIRS[@]}"; do
    if [ ! -d "/src/$d" ]; then
        echo "⚠️  /src/$d no existe (se omite)"
        continue
    fi
    while IFS= read -r -d '' f; do
        files+=("$f")
    done < <(find "/src/$d" -type f \
        \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.cc' -o -name '*.cxx' \) \
        -print0)
done

if [ ${#files[@]} -eq 0 ]; then
    echo "No se encontraron archivos C++ en: ${LINT_DIRS[*]}"
    exit 0
fi

# ── Desglose por directorio (para VER que Wrapper se analiza) ───────
echo "Archivos por directorio:"
for d in "${LINT_DIRS[@]}"; do
    count=0
    for f in "${files[@]}"; do
        case "$f" in /src/"$d"/*) count=$((count + 1)) ;; esac
    done
    printf "  %-12s %3d\n" "$d" "$count"
done
echo "Total: ${#files[@]} archivos"
echo

# ... resto igual (clang-format, clang-tidy, exit code)