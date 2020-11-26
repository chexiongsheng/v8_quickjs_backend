mkdir build & pushd build
cmake -S ..\CMakeLists.win.txt -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -G "Visual Studio 16 2019" -A x64 ..
popd
cmake --build build --config Release
copy 
LIB.EXE /OUT:quickjs.lib ..\build\libquickjs.a Release\v8qjs.lib
pause
