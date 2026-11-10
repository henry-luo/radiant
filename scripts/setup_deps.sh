#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INPUT_ROOT="${LAMBDA_INPUT_ROOT:-${INPUT_ROOT:-$ROOT/../lambda-input}}"
platform="${1:?platform required}"
shift
skip_input=0
for option in "$@"; do
    case "$option" in --dry-run) export SETUP_DRY_RUN=1 ;; --skip-input) skip_input=1 ;; *) echo "Unknown option: $option" >&2; exit 2 ;; esac
done
source "$INPUT_ROOT/scripts/setup_packages.sh"
if [ "$skip_input" = 0 ]; then
    if [ "${SETUP_DRY_RUN:-0}" = 1 ]; then bash "$INPUT_ROOT/scripts/setup_deps.sh" "$platform" --dry-run
    else bash "$INPUT_ROOT/scripts/setup_deps.sh" "$platform"; fi
fi
case "$platform" in
    mac) setup_packages mac glfw fribidi ;;
    linux) setup_packages linux libwebkit2gtk-4.1-dev libglfw3-dev libfribidi-dev gettext libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev ;;
    windows) setup_packages windows @toolchain@-glfw ;;
    *) echo "Unknown setup platform: $platform" >&2; exit 2 ;;
esac
