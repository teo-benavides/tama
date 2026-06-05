#!/usr/bin/env bash
# Build libtama for every requested platform/target combination.
#
# Usage:
#   ./build.sh [OPTIONS]
#
# Options:
#   -p, --platform  Comma-separated platforms to build (windows,linux,web). Default: all three.
#   -t, --target    Comma-separated targets to build (debug,release). Default: both.
#   -j, --jobs      Parallel compile jobs passed to scons. Default: nproc/4.
#   -h, --help      Show this help.
#
# Web builds require the Emscripten SDK to be activated in the current shell:
#   source /path/to/emsdk/emsdk_env.sh
#
# Examples:
#   ./build.sh                         # build everything
#   ./build.sh -p linux                # linux debug + release only
#   ./build.sh -p windows,web -t release

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NATIVE_DIR="$SCRIPT_DIR/addons/tama/native"

# ── defaults ───────────────────────────────────────────────────────────────
PLATFORMS="windows,linux,web"
TARGETS="debug,release"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

# ── argument parsing ────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--platform)  PLATFORMS="$2"; shift 2 ;;
        -t|--target)    TARGETS="$2";   shift 2 ;;
        -j|--jobs)      JOBS="$2";      shift 2 ;;
        -h|--help)
            sed -n '2,/^set /p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ── map friendly names to scons values ─────────────────────────────────────
scons_target() {
    case $1 in
        debug)   echo "template_debug" ;;
        release) echo "template_release" ;;
        *)       echo "$1" ;;
    esac
}

# ── collect build matrix ────────────────────────────────────────────────────
IFS=',' read -ra PLATFORM_LIST <<< "$PLATFORMS"
IFS=',' read -ra TARGET_LIST   <<< "$TARGETS"

PASS=()
FAIL=()

# ── run builds ──────────────────────────────────────────────────────────────
cd "$NATIVE_DIR"

for platform in "${PLATFORM_LIST[@]}"; do
    platform="${platform// /}"
    for target in "${TARGET_LIST[@]}"; do
        target="${target// /}"
        scons_t=$(scons_target "$target")
        label="$platform/$target"

        echo ""
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "  Building: $label"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

        if scons platform="$platform" target="$scons_t" -j"$JOBS"; then
            PASS+=("$label")
        else
            FAIL+=("$label")
        fi
    done
done

# ── summary ─────────────────────────────────────────────────────────────────
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Build summary"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for l in "${PASS[@]}"; do echo "  ✓  $l"; done
for l in "${FAIL[@]}"; do echo "  ✗  $l"; done

[[ ${#FAIL[@]} -eq 0 ]]
