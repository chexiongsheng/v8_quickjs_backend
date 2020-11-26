mkdir build
cd build
cmake -DCMAKE_CXX_COMPILER=gcc -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON ..
cd ..
cmake --build build --config Release
