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

build_preset "$PRESET"
