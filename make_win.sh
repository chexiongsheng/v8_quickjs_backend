mkdir build
cd build
cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON ..
cd ..
cmake --build build --config Release
