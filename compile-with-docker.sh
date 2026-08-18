#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------
# Usage:
#   ./compile-with-docker.sh [Preset] [CMake options...]
# Examples:
#   ./compile-with-docker.sh
#   ./compile-with-docker.sh syrup
#   ./compile-with-docker.sh syrup -DDEV=ON
# Default preset: "syrup"
# Packed output: build/syrup/SYRUP_<version>_BD1AHN.bin
# After each build, CMake also replaces docs/firmware/syrup.bin
# (old docs/firmware/*.bin are deleted) for the web flasher.
# ---------------------------------------------

IMAGE=uvk1-uvk5v3
PRESET=${1:-syrup}
shift || true  # remove preset from arguments if present

# Any remaining args will be treated as CMake cache variables
EXTRA_ARGS=("$@")

# ---------------------------------------------
# Validate preset name
# ---------------------------------------------
if [[ ! "$PRESET" =~ ^(syrup)$ ]]; then
  echo "❌ Unknown preset: '$PRESET'"
  echo "Valid presets are: syrup"
  exit 1
fi

# ---------------------------------------------
# Build the Docker image (only needed once)
# ---------------------------------------------
if [[ "$(docker images -q $IMAGE)" == "" ]]; then
  echo "Building Docker image..."
  docker build -t "$IMAGE" .
fi

# ---------------------------------------------
# Clean existing CMake cache to ensure toolchain reload
# ---------------------------------------------
rm -rf build
export MSYS_NO_PATHCONV=1
# ---------------------------------------------
# Function to build one preset
# ---------------------------------------------
build_preset() {
  local preset="$1"
  echo ""
  echo "=== 🚀 Building preset: ${preset} ==="
  echo "---------------------------------------------"
  docker run --rm \
    -u $(id -u):$(id -g) \
    -it -v "$PWD":/src -w /src "$IMAGE" \
    bash -c 'which arm-none-eabi-gcc && arm-none-eabi-gcc --version &&
             cmake --preset "$1" "${@:2}" &&
             cmake --build --preset "$1" -j' \
    bash "${preset}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
  echo "✅ Done: ${preset}"
}

# Sync the packaged .bin into docs/firmware (CMake POST_BUILD also does this).
# Previous docs/firmware/*.bin are removed so the web flasher always gets this build.
copy_firmware_to_docs() {
  local preset="$1"
  local build_dir="build/${preset}"
  local src=""
  if [[ -d "$build_dir" ]]; then
    src=$(ls -1 "$build_dir"/SYRUP_*.bin 2>/dev/null | head -n 1 || true)
    if [[ -z "$src" ]]; then
      src=$(ls -1 "$build_dir"/*.bin 2>/dev/null | head -n 1 || true)
    fi
  fi
  if [[ -n "$src" && -f "$src" ]]; then
    mkdir -p docs/firmware
    rm -f docs/firmware/*.bin
    cp -f "$src" docs/firmware/syrup.bin
    echo "📁 已更新 docs/firmware/syrup.bin（来源: $src）"
  else
    echo "⚠️ 未找到 ${preset} 固件产物，跳过复制到 docs/firmware"
  fi
}

build_preset "$PRESET"
copy_firmware_to_docs "$PRESET"
