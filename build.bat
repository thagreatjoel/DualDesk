@echo off
echo Cleaning build folder...
rmdir /s /q build
mkdir build
cd build

echo Running CMake...
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G "Visual Studio 18 2026" -A x64

echo Building...
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . --config Debug

echo.
echo Running DualDesk...
cd Debug
DualDesk.exe
pause