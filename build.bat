@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64

cd /d "%~dp0"
if not exist build mkdir build
cd build

"C:\Program Files\CMake\bin\cmake.exe" .. -G "NMake Makefiles"
if errorlevel 1 (
    echo CMake configuration failed
    pause
    exit /b 1
)

nmake
if errorlevel 1 (
    echo Build failed
    pause
    exit /b 1
)

echo.
echo Build successful!
echo Library: %~dp0build\libvoice.a or voice.lib
pause
