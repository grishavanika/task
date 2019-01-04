@echo off
:: Specific local settings :(
:: Python, GCC, CMake
set path=C:\Programs\Python\Python36-64\;%path%;
set path=%path%;C:\Programs\mingw_gcc_7.3.0\bin;
set path=%path%;C:\Programs\CMake\bin

:: MSVC
cd tools
python generate_project.py --compiler=cl --platform=x64
cd ../build_cl_x64
cmake --build . --config Release
if errorlevel 1 goto exit
ctest -C Release -V
if errorlevel 1 goto exit
cd ..

:: Clang
cd tools
python generate_project.py --compiler=clang --platform=x64
cd ../build_clang_x64
cmake --build . --config Release
if errorlevel 1 goto exit
ctest -C Release -V
if errorlevel 1 goto exit
cd ..

:: GCC
cd tools
python generate_project.py --compiler=gcc --platform=x64
cd ../build_gcc_x64
cmake --build . --config Release
if errorlevel 1 goto exit
ctest -C Release -V
if errorlevel 1 goto exit
cd ..

:exit
	exit /b errorlevel
