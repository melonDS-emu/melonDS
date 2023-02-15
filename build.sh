
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build -j$(nproc --all)
