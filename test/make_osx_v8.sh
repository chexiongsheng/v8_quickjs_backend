mkdir -p build_v8 && cd build_v8
cmake -DJS_ENGINE=v8 -DLIB_NAME=wee8 -GXcode ../
cd ..
cmake --build build_v8 --config Release


