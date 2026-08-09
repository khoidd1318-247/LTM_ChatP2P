Repo: 
fix 2

    p2pchat/
    │
    ├── .vscode/                 # Thư mục cấu hình của VS Code (tùy chọn)
    │   └── tasks.json           # File cấu hình task build tự động
    │
    ├── build/                   # Thư mục chứa các file thực thi (.exe) sau khi biên dịch
    │   ├── chat_app.exe         # File ứng dụng chat client (giao diện ImGui)
    │   └── server.exe           # File máy chủ điều hướng tín hiệu (Backend)
    │
    ├── include/                 # Thư mục tập trung toàn bộ các Header/Thư viện ngoài
    │   ├── asio/                # Thư mục mã nguồn thư viện Asio
    │   ├── GLFW/                # Thư mục chứa header quản lý cửa sổ đồ họa GLFW
    │   ├── imgui/               # Thư mục chứa toàn bộ mã nguồn ImGui và ImGui Impl
    │   ├── websocketpp/         # Thư mục mã nguồn thư viện WebSocket++
    │   ├── asio.hpp             # File header độc lập của Asio
    │   └── crow_all.h           # File header tổng hợp của thư viện Crow (Server)
    │
    ├── lib/                     # Thư mục chứa các thư viện liên kết tĩnh
    │   └── libglfw3.a           # File thư viện tĩnh của GLFW dùng khi build client
    │
    ├── src/                     # Thư mục chứa mã nguồn chính tự viết của dự án
    │   ├── backend/             # Mã nguồn phía máy chủ
    │   │   └── main.cpp         # File mã nguồn chính của Signaling Server
    │   │
    │   └── frontend/            # Mã nguồn phía giao diện người dùng
    │       └── main.cpp         # File mã nguồn chính của ứng dụng Chat Client
    │
    ├── .gitignore               # File cấu hình danh sách các file/thư mục bỏ qua không đẩy lên Git
    └── README.md                # File tài liệu hướng dẫn tổng quan về dự án
Câu lệnh: 

    Câu lệnh build(trỏ tới folder tổng chứa tất cả file):
        backend: g++ src/backend/main.cpp -I./include -lws2_32 -lmswsock -o build/server.exe
        frontend: g++ src/frontend/main.cpp include/imgui/imgui.cpp include/imgui/imgui_draw.cpp include/imgui/imgui_tables.cpp include/imgui/imgui_widgets.cpp include/imgui/imgui_impl_glfw.cpp include/imgui/imgui_impl_opengl3.cpp -std=c++17 -I./include -I./include/imgui -L./lib -lglfw3 -lopengl32 -lgdi32 -lws2_32 -lmswsock -mwindows -o build/chat_app.exe
    Câu lệnh Run(trỏ tới folder build):
        Run server(backend): server.exe
        Run app(frontend): chat_app.exe
