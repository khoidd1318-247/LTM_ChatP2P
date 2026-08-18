@echo off
cd /d "%~dp0"
echo ========================================
echo   DANG BIEN DICH HE THONG CHAT P2P (MinGW)
echo ========================================
echo.

set GPP_CMD=g++

if not exist build mkdir build

echo [1/1] Dang bien dich Chat P2P App...
%GPP_CMD% -o build/chat_app.exe -std=c++17 src/frontend/main.cpp ^
    include/imgui/imgui.cpp ^
    include/imgui/imgui_draw.cpp ^
    include/imgui/imgui_tables.cpp ^
    include/imgui/imgui_widgets.cpp ^
    include/imgui/imgui_impl_glfw.cpp ^
    include/imgui/imgui_impl_opengl3.cpp ^
    -I./include ^
    -I./include/imgui ^
    -I./include/GLFW ^
    -L./lib ^
    -lglfw3 -lgdi32 -lopengl32 -lws2_32 -lmswsock -mwindows

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [LOI] Bien dich Chat App that bai! Vui long kiem tra lai.
    pause
    exit /b
)

echo [OK] Da tao xong build/chat_app.exe
echo.
echo ========================================
echo   DANG KHOI DONG UNG DUNG CHAT P2P
echo ========================================
start "" "build\chat_app.exe"