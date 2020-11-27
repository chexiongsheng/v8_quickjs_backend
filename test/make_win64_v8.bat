mkdir build_v8 & pushd build_v8
cmake -DJS_ENGINE=v8 -DLIB_NAME=wee8 -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -G "Visual Studio 16 2019" -A x64 ..
popd
cmake --build build_v8 --config Release
pause
