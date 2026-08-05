CẤU TRÚC:
    p2p_chat_project/
    ├── .gitignore                  # Bỏ qua các thư mục build/, node_modules/
    ├── README.md                   # Hướng dẫn setup và build dự án
    ├── shared/                     # (Tùy chọn) Nơi chứa code dùng chung
    │   └── message_types.h         # Định nghĩa các hằng số, struct tin nhắn JSON dùng cho cả 2 bên
    │
    ├── backend/                    # SIGNALING SERVER (Máy chủ tín hiệu)
    │   ├── CMakeLists.txt          # Cấu hình build cho Backend (dùng GCC/Clang/MSVC)
    │   ├── src/                    # Mã nguồn C++
    │   │   ├── main.cpp            # Điểm bắt đầu (khởi động server)
    │   │   ├── signaling_server.cpp# Logic xử lý WebSocket (nhận/gửi SDP, ICE)
    │   │   └── peer_manager.cpp    # Quản lý danh sách các user đang online
    │   ├── include/                # Header files (.h, .hpp)
    │   │   ├── signaling_server.hpp
    │   │   └── peer_manager.hpp
    │   ├── third_party/            # Thư viện bên thứ 3 (nếu không dùng package manager)
    │   │   ├── nlohmann/           # Thư viện JSON cho C++ (rất cần thiết)
    │   │   └── uWebSockets/        # (Gợi ý) Thư viện WebSocket cực nhanh cho C++
    │   ├── tests/                  # Unit test cho logic backend
    │   └── build/                  # Thư mục chứa file thực thi sau khi chạy cmake
    │
    └── frontend/                   # WEB P2P CLIENT (Chạy trên trình duyệt)
        ├── CMakeLists.txt          # Cấu hình build cho Frontend (dùng Emscripten - emcc)
        ├── package.json            # Quản lý các tool web (như local server để test)
        ├── src/                    # Mã nguồn C++ (WebAssembly)
        │   ├── main.cpp            # Khởi tạo module Wasm, đăng ký các hàm Embind
        │   ├── chat_ui.cpp         # Xử lý logic DOM (thêm/xóa tin nhắn lên màn hình)
        │   └── p2p_logic.cpp       # Quản lý trạng thái kết nối WebRTC bằng C++
        ├── include/                # Header files cho phần Wasm
        │   ├── chat_ui.hpp
        │   └── p2p_logic.hpp
        ├── web_assets/             # Các file Web gốc (Sẽ được copy sang build/)
        │   ├── index.html          # Khung giao diện
        │   ├── style.css           # CSS định dạng
        │   └── webrtc_bridge.js    # Cầu nối JS (Thực thi các API WebRTC mà C++ không gọi trực tiếp được)
        └── build/                  # Nơi chứa kết quả: file .wasm, .js (do emcc sinh ra) và web_assets

CÀI ĐẶT EMSDK(dùng để biên dịch mã C++ thành ứng dụng web): https://emscripten.org/docs/getting_started/downloads.html#
    Sau khi cài xong, mở cmd mới và gõ "emcc --version" xem biến môi trường đã hoạt động chưa
    Nếu vẫn biến môi trường vẫn chưa hoạt động làm theo như sau:
        1. Bấm phím Windows trên bàn phím, gõ tìm kiếm chữ Environment Variables và chọn mục Edit the system environment variables.

        2. Trong cửa sổ System Properties, bấm vào nút Environment Variables... ở góc dưới cùng.

        3. Ở phần User variables (hoặc System variables), tìm biến có tên là Path, chọn nó và bấm nút Edit....

        4. Bấm nút New và dán lần lượt 2 đường dẫn sau vào (giả sử bạn cài emsdk ở D:\Coder\emsdk, hãy thay đổi nếu bạn cài ở ổ khác):

            D:\Coder\emsdk

            D:\Coder\emsdk\upstream\emscripten

        5. Bấm OK liên tục để lưu lại tất cả các bảng.

        6. Mở một cửa sổ cmd hoàn toàn mới và thử lại lệnh emcc --version.
