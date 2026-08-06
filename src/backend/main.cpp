#include "crow_all.h"
#include <iostream>
#include <mutex>
#include <unordered_set>

using namespace std;

int main() {
    // Khởi tạo ứng dụng Crow
    crow::SimpleApp app;

    // Biến khóa mtx để chống xung đột dữ liệu khi nhiều người cùng kết nối
    mutex mtx;

    // Danh sách lưu trữ địa chỉ của các máy khách đang kết nối
    unordered_set<crow::websocket::connection*> connected_clients;

    cout << "Loading Server..." << endl;

    // Mở cổng WebSocket tại đường dẫn /signaling
    CROW_WEBSOCKET_ROUTE(app, "/signaling")
    .onopen([&](crow::websocket::connection& conn) {
        lock_guard<mutex> lock(mtx);
        connected_clients.insert(&conn);
        cout << "[+] New client connected. Total: " 
             << connected_clients.size() << endl;
    })
    // LƯU Ý: Đã thêm tham số uint16_t code để tương thích với bản Crow mới
    .onclose([&](crow::websocket::connection& conn, const string& reason, uint16_t code) {
        lock_guard<mutex> lock(mtx);
        connected_clients.erase(&conn);
        cout << "[-] Client disconnected. Total: " 
             << connected_clients.size() << endl;
    })
    .onmessage([&](crow::websocket::connection& conn, const string& data, bool is_binary) {
        lock_guard<mutex> lock(mtx);
        cout << "[Relay] Forwarding signal data of " << data.length() << " bytes." << endl;

        // Gửi tin nhắn đến tất cả các client khác, ngoại trừ người gửi
        for (auto client : connected_clients) {
            if (client != &conn) {
                client->send_text(data);
            }
        }
    });

    // Chạy máy chủ ở cổng 8080 với chế độ đa luồng (multithreaded)
    app.port(8080).multithreaded().run();

    return 0;
}