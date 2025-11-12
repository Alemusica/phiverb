#!/bin/bash
# Performance benchmark for Phiverb

echo "⚡ Phiverb Performance Benchmark"
echo "================================"

# System info
if command -v system_profiler &> /dev/null; then
    echo "System: $(system_profiler SPHardwareDataType 2>/dev/null | grep 'Chip' | awk '{print $2, $3, $4}')"
    echo "Memory: $(system_profiler SPHardwareDataType 2>/dev/null | grep 'Memory' | awk '{print $2, $3}')"
else
    echo "System: $(uname -m)"
    echo "Memory: $(free -h 2>/dev/null | grep Mem | awk '{print $2}' || echo 'N/A')"
fi
echo ""

# Benchmark configurations
SCENES=("simple_cube.obj" "concert_hall.obj" "cathedral.obj")
METHODS=("raytracing" "waveguide" "hybrid")

# Check if build directory exists
if [ ! -d "build/bin" ]; then
    echo "❌ Build directory not found. Please run cmake and build first."
    exit 1
fi

# Check for wayverb CLI binary
if [ ! -x "build/bin/wayverb_cli" ]; then
    echo "⚠️  wayverb_cli not found. Skipping benchmarks."
    exit 0
fi

for scene in "${SCENES[@]}"; do
    # Check if scene exists
    if [ ! -f "data/scenes/$scene" ] && [ ! -f "tests/scenes/$scene" ]; then
        echo "⚠️  Scene not found: $scene (skipping)"
        continue
    fi
    
    echo "📐 Scene: $scene"
    for method in "${METHODS[@]}"; do
        echo -n "  $method: "
        
        # Run benchmark
        START=$(date +%s%N 2>/dev/null || date +%s)
        ./build/bin/wayverb_cli \
            --scene "data/scenes/$scene" \
            --method "$method" \
            --output "/tmp/benchmark_output.wav" \
            --quiet 2>/dev/null || echo "N/A"
        END=$(date +%s%N 2>/dev/null || date +%s)
        
        # Calculate time in milliseconds
        if command -v bc &> /dev/null; then
            ELAPSED=$(echo "scale=0; ($END - $START) / 1000000" | bc)
            echo "${ELAPSED}ms"
        else
            echo "completed"
        fi
    done
    echo ""
done

echo "✅ Benchmark completed"
