#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
xcodebuild -workspace ios/ReactOpenGLLab.xcworkspace \
  -scheme ReactOpenGLLab -configuration Release \
  -destination "platform=macOS,variant=Mac Catalyst,arch=$(uname -m)" \
  -derivedDataPath artifacts/catalyst/DerivedData \
  CODE_SIGNING_ALLOWED=NO "$@"
