mkdir build_qjs & pushd build_qjs
cmake -DJS_ENGINE=quickjs -DLIB_NAME=quickjs -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -G "Visual Studio 16 2019" -A x64 ..
popd
cmake --build build_qjs --config Release
pause
