#include "crow_all.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <map>
#include <string>
#include <vector>
#include <ctime>
#include <sstream>

using namespace std;

// Hàm hỗ trợ tách chuỗi (Split string)
vector<string> split(const string& str, char delim) {
    vector<string> tokens;
    size_t start = 0, end = 0;
    while ((end = str.find(delim, start)) != string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

// Hàm lấy thời gian hiện tại
string get_timestamp() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buf[32];
    sprintf(buf, "%02d:%02d:%02d", ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    return string(buf);
}

// Hàm ghi log vào file server_log.txt và in ra console
void write_log(const string& msg) {
    ofstream ofs("server_log.txt", ios::app);
    if(ofs) {
        ofs << "[" << get_timestamp() << "] " << msg << endl;
    }
    cout << "[" << get_timestamp() << "] " << msg << endl;
}

int main() {
    crow::SimpleApp app;
    mutex mtx;

    // Room ID -> Danh sách các kết nối trong phòng đó
    map<string, unordered_set<crow::websocket::connection*>> rooms;
    // Theo dõi Connection đang ở Room nào
    map<crow::websocket::connection*, string> conn_room;
    // Theo dõi Connection thuộc Username nào (đã xử lý trùng lặp)
    map<crow::websocket::connection*, string> conn_user;

    write_log("Starting Server on port 8080...");

    CROW_WEBSOCKET_ROUTE(app, "/signaling")
    .onopen([&](crow::websocket::connection& conn) {
        lock_guard<mutex> lock(mtx);
        write_log("New connection established.");
    })
    .onclose([&](crow::websocket::connection& conn, const string& reason, uint16_t code) {
        lock_guard<mutex> lock(mtx);
        if (conn_room.count(&conn)) {
            string room = conn_room[&conn];
            string user = conn_user[&conn];
            
            // Xóa client khỏi phòng
            rooms[room].erase(&conn);
            conn_room.erase(&conn);
            conn_user.erase(&conn);

            write_log("Connection closed for user " + user + " in room " + room);
            
            // Thông báo cho cả phòng biết có người thoát
            string sys_msg = "SYS|" + room + "|Server|" + user + " da roi khoi phong.";
            for (auto client : rooms[room]) {
                client->send_text(sys_msg);
            }
        } else {
            write_log("Unidentified connection closed.");
        }
    })
    .onmessage([&](crow::websocket::connection& conn, const string& data, bool is_binary) {
        lock_guard<mutex> lock(mtx);
        
        // Gói tin chuẩn: ACTION|ROOM|USERNAME|CONTENT
        vector<string> parts = split(data, '|');
        
        // KIỂM TRA DỮ LIỆU: Báo lỗi nếu sai format
        if (parts.size() < 4) {
            write_log("ERR: Invalid format received.");
            conn.send_text("ERR|Global|Server|Invalid packet format. Required: ACTION|ROOM|USER|MSG");
            return;
        }

        string action = parts[0];
        string room = parts[1];
        string user = parts[2];
        string content = parts[3];

        if (action == "JOIN") {
            // Xử lý trùng tên: Gắn thêm 4 ký tự cuối của địa chỉ bộ nhớ vào tên
            stringstream ss;
            ss << &conn;
            string addr = ss.str();
            string short_id = addr.substr(addr.length() > 4 ? addr.length() - 4 : 0);
            
            user = user + "#" + short_id;
            
            rooms[room].insert(&conn);
            conn_room[&conn] = room;
            conn_user[&conn] = user;
            
            write_log("User " + user + " joined room: " + room);
            
            // Báo cho mọi người trong phòng
            string sys_msg = "SYS|" + room + "|Server|" + user + " da vao phong!";
            for (auto client : rooms[room]) {
                client->send_text(sys_msg);
            }
        } 
        else if (action == "MSG") {
            // Validation: Chưa vào phòng mà dám chat
            if (conn_room.count(&conn) == 0 || conn_room[&conn] != room) {
                conn.send_text("ERR|" + room + "|Server|You must join the room first.");
                return;
            }
            
            string full_user = conn_user[&conn];
            string msg_out = "MSG|" + room + "|" + full_user + "|" + content;
            
            // CHỈ GỬI CHO NHỮNG NGƯỜI TRONG PHÒNG ĐÓ (Trừ bản thân)
            for (auto client : rooms[room]) {
                if (client != &conn) {
                    client->send_text(msg_out);
                }
            }
            write_log("Room " + room + " | " + full_user + " sent a message.");
        }
    });

    app.port(8080).multithreaded().run();
    return 0;
}