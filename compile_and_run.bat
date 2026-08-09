@echo off
echo ==============================================
echo   DANG BIEN DICH HE THONG CHAT P2P (MinGW)
echo ==============================================
echo.

set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
set GPP_CMD=C:\msys64\mingw64\bin\g++.exe

if not exist build mkdir build

echo [1/2] Dang bien dich Backend (Server)...
%GPP_CMD% src/backend/main.cpp -I./include -lws2_32 -lmswsock -o build/server.exe
if %ERRORLEVEL% NEQ 0 (
    echo [LOI] Bien dich Backend that bai! Vui long kiem tra lai.
    pause
    exit /b
)
echo [OK] Da tao xong build/server.exe

echo.
echo [2/2] Dang bien dich Frontend (Chat App)...
%GPP_CMD% src/frontend/main.cpp include/imgui/imgui.cpp include/imgui/imgui_draw.cpp include/imgui/imgui_widgets.cpp include/imgui/imgui_tables.cpp include/imgui/imgui_impl_glfw.cpp include/imgui/imgui_impl_opengl3.cpp -I./include -I./include/imgui -L./lib -lglfw3 -lopengl32 -lgdi32 -lws2_32 -lmswsock -o build/chat_app.exe
if %ERRORLEVEL% NEQ 0 (
    echo [LOI] Bien dich Frontend that bai!
    pause
    exit /b
)
echo [OK] Da tao xong build/chat_app.exe

echo.
echo ==============================================
echo   BIEN DICH THANH CONG!
echo ==============================================
echo De chay chuong trinh:
echo 1. Vao thu muc 'build'
echo 2. Chay 'server.exe' truoc
echo 3. Chay 'chat_app.exe' sau
echo.
pause
