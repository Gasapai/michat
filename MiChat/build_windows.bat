@echo off
rem Compila Mi Chat y genera dist\MiChat-Setup.exe
rem Requisitos: Qt 6 (MSVC 64-bit), CMake, Ninja e Inno Setup 6.
rem Ejecutar desde "x64 Native Tools Command Prompt for VS".
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.7.3\msvc2019_64
set PATH=%QT_DIR%\bin;%PATH%

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT_DIR% || exit /b 1
cmake --build build || exit /b 1

rmdir /s /q deploy 2>nul
mkdir deploy
copy build\MiChat.exe deploy\ || exit /b 1
windeployqt --release --no-translations deploy\MiChat.exe || exit /b 1

"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" installer\michat.iss || exit /b 1
echo.
echo Listo: dist\MiChat-Setup.exe
