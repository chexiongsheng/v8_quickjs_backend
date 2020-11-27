mkdir -p build_qjs && cd build_qjs
cmake -DJS_ENGINE=quickjs -DLIB_NAME=quickjs -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON  -GXcode ../
cd ..
cmake --build build_qjs --config Release
