# This is provided for convenience only and not currently tested in the CI
cmake -S . -B cmakebuild-unix -G Ninja
cmake --build cmakebuild-unix