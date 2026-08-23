@echo off
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
set GPP_CMD=C:\msys64\mingw64\bin\g++.exe

if not exist build mkdir build

echo Dang bien dich ung dung P2P Chat (ImGui version)...
%GPP_CMD% src/frontend/main.cpp include/imgui/imgui.cpp include/imgui/imgui_draw.cpp include/imgui/imgui_tables.cpp include/imgui/imgui_widgets.cpp include/imgui/imgui_impl_glfw.cpp include/imgui/imgui_impl_opengl3.cpp -std=c++17 -D_WIN32_WINNT=0x0601 -I./include -I./include/imgui -L./lib -lglfw3 -lopengl32 -lgdi32 -lws2_32 -lmswsock -mwindows -o build/chat_app.exe

if %ERRORLEVEL% NEQ 0 (
    echo [LOI] Bien dich that bai! Vui long kiem tra lai.
    pause
    exit /b 1
)

echo [OK] Da tao xong build/chat_app.exe
echo.
echo ==============================================
echo   BIEN DICH THANH CONG!
echo ==============================================
echo.
start "" "build\chat_app.exe"