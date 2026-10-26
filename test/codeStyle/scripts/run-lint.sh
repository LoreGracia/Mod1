#!/usr/bin/env bash
set -uo pipefail

read -ra LINT_DIRS <<< "${LINT_DIRS:-src inc Wrapper}"
MODE="${MODE:-check}"      # check | fix | summary

# ── Colores ─────────────────────────────────────────────────────────
if [ -t 1 ] || [ "${FORCE_COLOR:-0}" = "1" ]; then
	RED=$'\033[31m'; GREEN=$'\033[32m'; YELLOW=$'\033[33m'
	CYAN=$'\033[36m'; BOLD=$'\033[1m'; DIM=$'\033[2m'; RESET=$'\033[m'
else
	RED=; GREEN=; YELLOW=; CYAN=; BOLD=; DIM=; RESET=
fi

# ── Recoger archivos ────────────────────────────────────────────────
declare -a files=()
declare -A count_by_dir=()

for d in "${LINT_DIRS[@]}"; do
	if [ ! -d "/src/$d" ]; then
		echo "${YELLOW}⚠  /src/$d no existe (se omite)${RESET}"
		continue
	fi
	count_by_dir[$d]=0
	while IFS= read -r -d '' f; do
		files+=("$f")
		count_by_dir[$d]=$(( ${count_by_dir[$d]} + 1 ))
	done < <(find "/src/$d" -type f \
		\( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.cc' -o -name '*.cxx' \) \
		-print0)
done

if [ ${#files[@]} -eq 0 ]; then
	echo "No se encontraron archivos C++ en: ${LINT_DIRS[*]}"
	exit 0
fi

echo "${BOLD}Analizando:${RESET} ${LINT_DIRS[*]}"
for d in "${LINT_DIRS[@]}"; do
	[ -n "${count_by_dir[$d]:-}" ] || continue
	printf "  %-12s %3d archivos\n" "$d" "${count_by_dir[$d]}"
done
printf "  ${BOLD}%-12s %3d archivos${RESET}\n" "Total" "${#files[@]}"
echo

# ── Modo fix: aplicar clang-format -i y salir ───────────────────────
if [ "$MODE" = "fix" ]; then
	echo "${BOLD}=== Modo fix: aplicando clang-format -i ===${RESET}"
	changed=0
	for f in "${files[@]}"; do
		before=$(md5sum "$f" | cut -d' ' -f1)
		clang-format -i "$f"
		after=$(md5sum "$f" | cut -d' ' -f1)
		if [ "$before" != "$after" ]; then
			echo "  ${GREEN}✎${RESET} $f"
			changed=$((changed + 1))
		fi
	done
	echo
	if [ $changed -eq 0 ]; then
		echo "${GREEN}✅ Nada que reformatear${RESET}"
	else
		echo "${GREEN}✅ ${changed} archivos reformateados${RESET}"
	fi
	echo "${DIM}Nota: clang-tidy warnings requieren fix manual.${RESET}"
	exit 0
fi

# ── clang-format: recopilar fallos ──────────────────────────────────
fmt_fail_files=()
for f in "${files[@]}"; do
	if ! clang-format --dry-run --Werror "$f" >/dev/null 2>&1; then
		fmt_fail_files+=("$f")
	fi
done

# ── clang-tidy: recopilar fallos ────────────────────────────────────
if [ -f /src/compile_commands.json ]; then
	TIDY_DB="-p /src"
else
	TIDY_DB="-- -std=c++17 -I/src/inc -I/src/Wrapper -isystem /src/external/glad/include -isystem /src/external/glm"
fi

tidy_fail_files=()
for f in "${files[@]}"; do
	case "$f" in *.hpp|*.h) continue ;; esac
	if ! clang-tidy $TIDY_DB "$f" --quiet >/tmp/tidy.log 2>&1; then
		if grep -qE 'warning:|error:' /tmp/tidy.log; then
			tidy_fail_files+=("$f")
		fi
	fi
done

# ── Modo summary: solo contadores ───────────────────────────────────
if [ "$MODE" = "summary" ]; then
	echo "${BOLD}=== Resumen ===${RESET}"
	printf "  %-30s %s\n" "Archivos analizados:" "${#files[@]}"
	printf "  %-30s %s\n" "Con errores de formato:" "${#fmt_fail_files[@]}"
	printf "  %-30s %s\n" "Con warnings tidy:" "${#tidy_fail_files[@]}"
	if [ ${#fmt_fail_files[@]} -eq 0 ] && [ ${#tidy_fail_files[@]} -eq 0 ]; then
		echo "${GREEN}${BOLD}✅ Todo OK${RESET}"
		exit 0
	fi
	exit 1
fi

# ── Modo check: detalle por archivo ─────────────────────────────────
echo "${BOLD}=== clang-format (estilo) ===${RESET}"
if [ ${#fmt_fail_files[@]} -eq 0 ]; then
	echo "${GREEN}✅ Todos los archivos cumplen el formato${RESET}"
else
	echo "${RED}❌ ${#fmt_fail_files[@]} de ${#files[@]} archivos con errores de formato${RESET}"
	echo
	echo "${DIM}Formato: '-' = línea actual, '+' = línea esperada por clang-format${RESET}"
	echo
	for f in "${fmt_fail_files[@]}"; do
		echo "${CYAN}── $f${RESET}"
		diff -u "$f" <(clang-format "$f") 2>/dev/null \
			| awk -v g="$GREEN" -v r="$RED" -v x="$RESET" '
				/^\+\+\+|^---/ { next }
				/^@@/        { print; next }
				/^\+/        { printf "%s%s%s\n", g, $0, x; next }
				/^-/         { printf "%s%s%s\n", r, $0, x; next }
				{ print }
			' | head -30
		echo
	done
	echo "${DIM}Para corregir automáticamente: make lint-fix${RESET}"
fi
echo

echo "${BOLD}=== clang-tidy (análisis) ===${RESET}"
if [ ! -f /src/compile_commands.json ]; then
	echo "${YELLOW}⚠  Sin compile_commands.json (flags por defecto)${RESET}"
fi
if [ ${#tidy_fail_files[@]} -eq 0 ]; then
	echo "${GREEN}✅ Sin warnings de clang-tidy${RESET}"
else
	echo "${RED}❌ ${#tidy_fail_files[@]} archivos con warnings de clang-tidy${RESET}"
	echo
	for f in "${tidy_fail_files[@]}"; do
		echo "${CYAN}── $f${RESET}"
		clang-tidy $TIDY_DB "$f" --quiet 2>&1 \
			| grep -E 'warning:|error:' | head -15
		echo
	done
fi
echo

# ── Resumen final ───────────────────────────────────────────────────
echo "${BOLD}=== Resumen ===${RESET}"
printf "  %-28s %s\n" "Archivos:" "${#files[@]}"
printf "  %-28s %s\n" "Formato (clang-format):" "${#fmt_fail_files[@]}"
printf "  %-28s %s\n" "Tidy (clang-tidy):" "${#tidy_fail_files[@]}"
echo
if [ ${#fmt_fail_files[@]} -eq 0 ] && [ ${#tidy_fail_files[@]} -eq 0 ]; then
	echo "${GREEN}${BOLD}✅ Todo OK${RESET}"
	exit 0
fi
echo "${RED}${BOLD}❌ Se encontraron problemas${RESET}"
exit 1
