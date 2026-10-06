#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
framework="$root/apple_platform/vendor/MetalANGLE.xcframework/ios-arm64_x86_64-maccatalyst"
output="$root/artifacts/renderer-tests-mac"
mkdir -p "$output"
sdk="$(xcrun --sdk macosx --show-sdk-path)"
xcrun clang++ -std=c++17 -target "$(uname -m)-apple-ios15.1-macabi" \
  -isysroot "$sdk" -I "$framework/MetalANGLE.framework/Headers" \
  -F "$framework" -framework MetalANGLE -Wl,-rpath,"$framework" \
  shared/renderer/ViewerRenderer.cpp tests/ViewerRendererSmoke.cpp \
  -o "$output/ViewerRendererSmoke"
"$output/ViewerRendererSmoke" "$root" 2>&1 | tee "$output/smoke.log"
