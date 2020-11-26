mkdir build
cd build
cmake -DCMAKE_CXX_COMPILER=g++ -DJS_ENGINE=quickjs -DLIB_NAME=quickjs -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -G "CodeBlocks - Unix Makefiles" ..
cd ..
cmake --build build --config Release
