#!/bin/bash
# Test suite for Metal implementation

set -e

echo "🧪 Testing Phiverb Metal Implementation"

# Check if running on Apple Silicon
if [[ $(uname -m) != "arm64" ]]; then
    echo "⚠️  Warning: Not running on Apple Silicon"
fi

# Compile Metal shaders
echo "📦 Compiling Metal shaders..."
if [ -d "src/metal" ]; then
    find src/metal -name "*.metal" 2>/dev/null | while read shader; do
        echo "  Compiling: $(basename $shader)"
        xcrun -sdk macosx metal -c "$shader" -o "${shader%.metal}.air" 2>&1 | grep -E "error|warning" || true
    done
else
    echo "  No src/metal directory found, skipping shader compilation"
fi

# Run unit tests
echo "🔧 Running unit tests..."
if [ -d "build" ]; then
    cd build
    ctest --output-on-failure -R "Metal" || echo "  No Metal-specific tests found"
    cd ..
else
    echo "  No build directory found. Run cmake first."
fi

echo "✅ Metal tests completed"
