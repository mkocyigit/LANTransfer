@echo off
REM "x64 Native Tools Command Prompt for VS" icinde calistirin (Visual Studio / Build Tools).
where cl >nul 2>nul
if %errorlevel%==0 (
  rc /nologo app.rc
  cl /nologo /EHsc /O2 /utf-8 /std:c++17 /DUNICODE /D_UNICODE main.cpp app.res /Fe:LANTransfer.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:NO
) else (
  windres app.rc -O coff -o app.res.o
  g++ -O2 -std=c++17 -municode -mwindows main.cpp app.res.o -o LANTransfer.exe -lws2_32 -liphlpapi -lcomctl32 -lole32 -luuid -lshell32 -ladvapi32 -ldwmapi -ld2d1 -ldwrite -static
)
