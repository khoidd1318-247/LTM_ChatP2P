@echo off
cd /d "%~dp0"
echo ========================================
echo   DANG BIEN DICH HE THONG CHAT P2P (MinGW)
echo ========================================
echo.

set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
set GPP_CMD=C:\msys64\mingw64\bin\g++.exe

if not exist build mkdir build
if not exist Code\build mkdir Code\build

echo [1/1] Dang bien dich Chat P2P App...
%GPP_CMD% -std=c++17 Code/src/frontend/main.cpp ^
    include/imgui/imgui.cpp ^
    include/imgui/imgui_draw.cpp ^
    include/imgui/imgui_tables.cpp ^
    include/imgui/imgui_widgets.cpp ^
    include/imgui/imgui_impl_glfw.cpp ^
    include/imgui/imgui_impl_opengl3.cpp ^
    -I./include ^
    -I./include/imgui ^
    -I./include/GLFW ^
    -I./Code/include ^
    -L./lib ^
    -L./Code/lib ^
    -lglfw3 -lopengl32 -lgdi32 -lws2_32 -lmswsock ^
    -o Code/build/chat_app.exe

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [LOI] Bien dich Chat App that bai! Vui long kiem tra lai.
    pause
    exit /b
)

echo [OK] Da tao xong Code/build/chat_app.exe
echo.
echo ========================================
echo   DANG KHOI DONG UNG DUNG CHAT P2P
echo ========================================
start "" Code\build\chat_app.exe