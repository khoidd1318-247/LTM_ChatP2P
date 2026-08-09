cmake_minimum_required(VERSION 3.15)
project(LTM_ChatP2P)

set(CMAKE_CXX_STANDARD 11)

# Thêm thư mục include
include_directories(include)
include_directories(include/GLFW)

# --- 1. BUILD MÁY CHỦ (BACKEND) ---
add_executable(server src/backend/main.cpp)
target_link_libraries(server wsock32 ws2_32)

# --- 2. BUILD GIAO DIỆN (FRONTEND) ---
# ImGui cần biên dịch cùng với mã nguồn
set(IMGUI_SOURCES
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/imgui_impl_glfw.cpp
    include/imgui/imgui_impl_opengl3.cpp
)

add_executable(chat_app src/frontend/main.cpp ${IMGUI_SOURCES})

# Trỏ tới file thư viện GLFW tĩnh của nhóm (libglfw3.a)
target_link_directories(chat_app PRIVATE lib)
target_link_libraries(chat_app glfw3 opengl32 gdi32 wsock32 ws2_32)
