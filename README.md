Repo: 

    LTM_ChatP2P-github/
    │
    ├── Code/                    # Thư mục chứa toàn bộ mã nguồn và thư viện chính
    │   ├── .vscode/             # Thư mục cấu hình của VS Code (tùy chọn)
    │   │   └── tasks.json       # File cấu hình task build tự động
    │   │
    │   ├── build/               # Thư mục chứa các file thực thi (.exe) sau khi biên dịch
    │   │   └── chat_app.exe     # File ứng dụng chat client (giao diện ImGui)
    │   │
    │   ├── include/             # Thư mục tập trung toàn bộ các Header/Thư viện ngoài
    │   │   ├── asio/            # Thư mục mã nguồn thư viện Asio
    │   │   ├── GLFW/            # Thư mục chứa header quản lý cửa sổ đồ họa GLFW
    │   │   ├── imgui/           # Thư mục chứa toàn bộ mã nguồn ImGui và ImGui Impl
    │   │   ├── websocketpp/     # Thư mục mã nguồn thư viện WebSocket++
    │   │   └── crow_all.h       # File header tổng hợp của thư viện Crow (Server)
    │   │
    │   ├── lib/                 # Thư mục chứa các thư viện liên kết tĩnh
    │   │   └── libglfw3.a       # File thư viện tĩnh của GLFW dùng khi build client
    │   │
    │   ├── src/                 # Thư mục chứa mã nguồn chính tự viết của dự án
    │   │   └── frontend/        
    │   │       └── main.cpp     # File mã nguồn chính của ứng dụng Chat Client
    │   │
    │   └── compile_and_run.bat  # ⚡ TOOL MỚI: Script tự động biên dịch và chạy app nhanh gọn lẹ
    │
    ├── .gitignore               # File cấu hình danh sách các file/thư mục bỏ qua không đẩy lên Git
    └── README.md                # File tài liệu hướng dẫn tổng quan về dự án

---

## Hướng dẫn Biên dịch (Build) ứng dụng

### 🛠 CÁCH 1: Dùng Tool tự động `compile_and_run.bat` (Khuyên dùng)
Để tiết kiệm thời gian, team đã có sẵn công cụ tự động hóa biên dịch. Bạn không cần phải mở VS Code hay dán lệnh thủ công dài dòng nữa.
1. Truy cập vào thư mục `Code/`
2. Nhấn đúp chuột (Double-click) vào file **`compile_and_run.bat`**.
3. Script sẽ tự động dọn dẹp file cũ, biên dịch mã nguồn C++ mới nhất và tự động bật app lên cho bạn test ngay lập tức. Cực kỳ nhàn rỗi!

### ⌨ CÁCH 2: Dùng câu lệnh thủ công (Manual Build)
Nếu bạn muốn tự gõ lệnh biên dịch, hãy mở Terminal, trỏ đường dẫn (`cd`) vào thư mục **`Code/`** và chạy câu lệnh sau:

```bash
g++ src/frontend/main.cpp include/imgui/imgui.cpp include/imgui/imgui_draw.cpp include/imgui/imgui_tables.cpp include/imgui/imgui_widgets.cpp include/imgui/imgui_impl_glfw.cpp include/imgui/imgui_impl_opengl3.cpp -std=c++17 -D_WIN32_WINNT=0x0601 -I./include -I./include/imgui -L./lib -lglfw3 -lopengl32 -lgdi32 -lws2_32 -lmswsock -mwindows -o build/chat_app.exe
```
