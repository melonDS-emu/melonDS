cmake -DCMAKE_BUILD_TYPE=Debug -B build -DBUILD_STATIC=ON -DCMAKE_PREFIX_PATH=/mingw64/qt5-static
cmake --build build
cd ./build/
./khDaysMM.exe