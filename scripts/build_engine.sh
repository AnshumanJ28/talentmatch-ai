#!/bin/bash
# scripts/build_engine.sh
# Build the TalentMatch C++ engine on Linux / macOS.
#
# Usage:
#   chmod +x scripts/build_engine.sh
#   ./scripts/build_engine.sh
#
# Optional:
#   ./scripts/build_engine.sh --xgboost    # enable XGBoost ML ranking
#   ./scripts/build_engine.sh --clean      # clean build directory first

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/cpp_core/build"
ENABLE_XGBOOST="OFF"
CLEAN_BUILD=0

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --xgboost) ENABLE_XGBOOST="ON" ;;
        --clean)   CLEAN_BUILD=1 ;;
    esac
done

echo "========================================"
echo " TalentMatch Core — C++ Build"
echo "========================================"
echo " Project root  : $PROJECT_ROOT"
echo " Build dir     : $BUILD_DIR"
echo " XGBoost       : $ENABLE_XGBOOST"
echo "========================================"
echo ""

# Check dependencies
command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found. Install: apt-get install cmake"; exit 1; }
command -v g++  >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1 || {
    echo "ERROR: No C++ compiler found. Install: apt-get install build-essential"
    exit 1
}

# Clean if requested
if [ "$CLEAN_BUILD" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure
echo "Configuring..."
cmake -S "$PROJECT_ROOT/cpp_core" \
      -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_XGBOOST="$ENABLE_XGBOOST"

# Build
echo ""
echo "Building..."
cmake --build "$BUILD_DIR" --config Release -j"$(nproc 2>/dev/null || echo 4)"

# Report
echo ""
echo "========================================"
LIB_PATH=$(find "$BUILD_DIR" -name "talentmatch.so" -o -name "talentmatch.dylib" 2>/dev/null | head -1)
if [ -n "$LIB_PATH" ]; then
    echo " Build succeeded!"
    echo " Library: $LIB_PATH"
    echo ""
    echo " To use, set:"
    echo "   export TALENTMATCH_LIB_PATH=$LIB_PATH"
    echo ""
    echo " Or run the API:"
    echo "   uvicorn api:app --host 0.0.0.0 --port 8000"
else
    echo " WARNING: Library not found in $BUILD_DIR"
    echo " Check build output above for errors."
fi
echo "========================================"
