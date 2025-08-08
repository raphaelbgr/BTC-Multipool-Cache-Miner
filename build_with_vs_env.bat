call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -host_arch=x64 -arch=x64
cmake -S . -B build -G Ninja -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CUDA_COMPILER="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9/bin/nvcc.exe"
cmake --build build -j 8
ctest --test-dir build --output-on-failure
