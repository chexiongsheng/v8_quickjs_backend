mkdir build & pushd build
cmake -S ..\CMakeLists.win.txt -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -G "Visual Studio 15 2017" ..
popd
cmake --build build --config Release
pause
