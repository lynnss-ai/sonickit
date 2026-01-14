@echo off
setlocal

REM Voice Library - New Modules Build Script

set VOICE_DIR=%~dp0
set BUILD_DIR=%VOICE_DIR%build_standalone
set GCC=gcc

echo =========================================
echo Voice Library - New Modules Compilation
echo =========================================
echo.

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Compile miniaudio
echo [1/7] Compiling miniaudio...
%GCC% -c -O2 -I"%VOICE_DIR%third_party\miniaudio" "%VOICE_DIR%third_party\miniaudio\miniaudio.c" -o miniaudio.o
if errorlevel 1 goto :error

REM Compile effects
echo [2/7] Compiling effects module...
%GCC% -c -O2 -I"%VOICE_DIR%include" -I"%VOICE_DIR%" "%VOICE_DIR%src\dsp\effects.c" -o effects.o
if errorlevel 1 goto :error

REM Compile watermark
echo [3/7] Compiling watermark module...
%GCC% -c -O2 -I"%VOICE_DIR%include" -I"%VOICE_DIR%" "%VOICE_DIR%src\dsp\watermark.c" -o watermark.o
if errorlevel 1 goto :error

REM Compile diagnostics
echo [4/7] Compiling diagnostics module...
%GCC% -c -O2 -I"%VOICE_DIR%include" -I"%VOICE_DIR%" "%VOICE_DIR%src\utils\diagnostics.c" -o diagnostics.o
if errorlevel 1 goto :error

REM Compile datachannel
echo [5/7] Compiling datachannel module...
%GCC% -c -O2 -I"%VOICE_DIR%include" -I"%VOICE_DIR%" "%VOICE_DIR%src\network\datachannel.c" -o datachannel.o
if errorlevel 1 goto :error

REM Compile SIP core
echo [6/7] Compiling SIP core module...
%GCC% -c -O2 -I"%VOICE_DIR%include" -I"%VOICE_DIR%" "%VOICE_DIR%src\sip\sip_core.c" -o sip_core.o
if errorlevel 1 goto :error

REM Compile SIP UA
echo [7/7] Compiling SIP UA module...
%GCC% -c -O2 -I"%VOICE_DIR%include" -I"%VOICE_DIR%" "%VOICE_DIR%src\sip\sip_ua.c" -o sip_ua.o
if errorlevel 1 goto :error

REM Create static library
echo.
echo Creating static library...
ar rcs libvoice_new.a effects.o watermark.o diagnostics.o datachannel.o sip_core.o sip_ua.o
if errorlevel 1 goto :error

echo.
echo =========================================
echo Build successful!
echo Library: %BUILD_DIR%\libvoice_new.a
echo =========================================
goto :end

:error
echo.
echo Build failed!
exit /b 1

:end
endlocal
