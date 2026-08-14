#define ASIO_STANDALONE
#include <asio.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

using namespace std;
using asio::ip::tcp;

// --- GLOBAL VARIABLES & STATE ---
mutex chat_mutex;
vector<string> chatHistory;
char username[128] = "";
char targetIP[128] = "127.0.0.1";
char portBuf[16] = "8080";
char messageBuf[1024] = "";

enum AppState { IDLE, WAITING_FOR_PEER, CONNECTED };
AppState currentState = IDLE;

// Asio network objects
asio::io_context* io_context_ptr = nullptr;
tcp::socket* peer_socket = nullptr;
tcp::acceptor* peer_acceptor = nullptr;
thread* network_thread = nullptr;
asio::streambuf read_buffer;

void add_log(const string& msg) {
    lock_guard<mutex> lock(chat_mutex);
    chatHistory.push_back(msg);
}

// Clean up socket only (keeps UI active)
void close_socket_only() {
    if (peer_socket) {
        asio::error_code ec;
        peer_socket->shutdown(tcp::socket::shutdown_both, ec);
        peer_socket->close(ec);
    }
}

// Full cleanup for resetting to main menu
void stop_network() {
    if (io_context_ptr) io_context_ptr->stop();
    close_socket_only();

    if (peer_socket) { delete peer_socket; peer_socket = nullptr; }
    if (peer_acceptor) { asio::error_code ec; peer_acceptor->close(ec); delete peer_acceptor; peer_acceptor = nullptr; }
    if (network_thread && network_thread->joinable()) { network_thread->join(); delete network_thread; network_thread = nullptr; }
    if (io_context_ptr) { delete io_context_ptr; io_context_ptr = nullptr; }

    currentState = IDLE;
}

// Asynchronous read loop for incoming messages
void async_read_loop() {
    if (!peer_socket || !peer_socket->is_open()) return;

    asio::async_read_until(*peer_socket, read_buffer, '\n', [](const asio::error_code& error, size_t bytes) {
        if (!error) {
            istream is(&read_buffer);
            string line;
            getline(is, line);
            if (!line.empty()) add_log(line);
            async_read_loop();
        }
        else {
            if (error == asio::error::eof || error == asio::error::connection_reset) {
                add_log("[System] Peer disconnected. You can still type in local chat.");
            }
            else if (error != asio::error::operation_aborted) {
                add_log("[System] Network connection lost.");
            }
            close_socket_only();
        }
        });
}

// ==========================================
// CALLBACK BẮT SỰ KIỆN GÕ PHÍM CHO BỘ LỌC IP
// ==========================================
int IPFilterCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        ImWchar c = data->EventChar;

        // 1. CHẶN NGAY LẬP TỨC: Nếu không phải là số (0-9) hoặc dấu chấm (.), trả về 1 để HỦY KÝ TỰ
        if ((c < '0' || c > '9') && c != '.') {
            return 1; // 1 = Bỏ qua ký tự vừa gõ (chặn tại bàn phím)
        }

        // 2. CHẶN NẾU LÀ DẤU CHẤM LIÊN TIẾP (VD: 192..168)
        if (c == '.' && data->CursorPos > 0 && data->Buf[data->CursorPos - 1] == '.') {
            return 1;
        }

        // 3. GIỚI HẠN NHÓM SỐ <= 255
        // Giả lập chuỗi sẽ ra sao nếu cho phép ký tự này chèn vào
        string current(data->Buf, data->Buf + data->CursorPos);
        current += (char)c;

        // Tìm octet cuối cùng đang nhập
        size_t lastDot = current.find_last_of('.');
        string segment = (lastDot == string::npos) ? current : current.substr(lastDot + 1);

        if (!segment.empty() && segment != ".") {
            try {
                int val = stoi(segment);
                if (val > 255) return 1; // Nếu lớn hơn 255 -> Chặn không cho gõ phím đó
            }
            catch (...) {
                return 1;
            }
        }
    }
    return 0; // 0 = Chấp nhận ký tự
}

// HOST MODE
void start_hosting(int port) {
    stop_network();
    chatHistory.clear();
    try {
        io_context_ptr = new asio::io_context();
        peer_acceptor = new tcp::acceptor(*io_context_ptr, tcp::endpoint(tcp::v4(), port));
        peer_socket = new tcp::socket(*io_context_ptr);

        currentState = WAITING_FOR_PEER;
        add_log("[System] Opening Port " + to_string(port) + " waiting for peer...");

        peer_acceptor->async_accept(*peer_socket, [](const asio::error_code& error) {
            if (!error) {
                add_log("[System] Peer connected! Start chatting.");
                currentState = CONNECTED;
                async_read_loop();
            }
            else if (error != asio::error::operation_aborted) {
                add_log("[System] Host error: " + error.message());
            }
            });

        network_thread = new thread([]() { io_context_ptr->run(); });
    }
    catch (exception& e) {
        add_log(string("[System] Error: ") + e.what());
        currentState = IDLE;
    }
}

// JOIN MODE
void start_connecting(string ip, int port) {
    stop_network();
    chatHistory.clear();
    try {
        io_context_ptr = new asio::io_context();
        peer_socket = new tcp::socket(*io_context_ptr);

        currentState = WAITING_FOR_PEER;
        add_log("[System] Connecting to " + ip + ":" + to_string(port) + "...");

        tcp::endpoint endpoint(asio::ip::make_address(ip), port);
        peer_socket->async_connect(endpoint, [](const asio::error_code& error) {
            if (!error) {
                add_log("[System] Connected successfully! Start chatting.");
                currentState = CONNECTED;
                async_read_loop();
            }
            else if (error != asio::error::operation_aborted) {
                add_log("[System] Connection failed: " + error.message());
            }
            });

        network_thread = new thread([]() { io_context_ptr->run(); });
    }
    catch (exception& e) {
        add_log(string("[System] Error: ") + e.what());
        currentState = IDLE;
    }
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(600, 700, "P2P Chat App (True P2P)", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        // UI CHO TRẠNG THÁI IDLE (Menu chính)
        if (currentState == IDLE) {
            ImGui::Text("=== TRUE P2P CHAT APP ===");
            ImGui::Spacing(); ImGui::Spacing();

            ImGui::Text("UserName:");
            ImGui::InputText("##username", username, IM_ARRAYSIZE(username));

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::Text("--- OPTION 1: HOST A ROOM ---");
            ImGui::Text("Port:");

            // -----------------------------------------------------------------------
            // CHẶN PORT: Dùng cờ `ImGuiInputTextFlags_CharsDecimal`
            // Chỉ chấp nhận ký tự số từ 0-9. Bất kỳ phím chữ/ký tự đặc biệt nào khi nhấn
            // sẽ bị hệ thống ImGui nuốt/chặn ngay tại thời điểm gõ (Hardware level event).
            // -----------------------------------------------------------------------
            ImGui::InputText("##hostport", portBuf, IM_ARRAYSIZE(portBuf), ImGuiInputTextFlags_CharsDecimal);

            if (ImGui::Button("Host Room", ImVec2(200, 40))) {
                if (strlen(portBuf) > 0) start_hosting(atoi(portBuf));
            }

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::Text("--- OPTION 2: JOIN A ROOM ---");
            ImGui::Text("Peer IP Address:");

            // -----------------------------------------------------------------------
            // CHẶN IP: Dùng cờ `ImGuiInputTextFlags_CallbackCharFilter` 
            // Kết hợp hàm IPFilterCallback để vừa chỉ cho phép số/dấu chấm,
            // vừa kiểm tra trực tiếp điều kiện <= 255 từng octet khi đang gõ.
            // -----------------------------------------------------------------------
            ImGui::InputText("##targetip", targetIP, IM_ARRAYSIZE(targetIP),
                ImGuiInputTextFlags_CallbackCharFilter, IPFilterCallback);

            if (ImGui::Button("Join Room", ImVec2(200, 40))) {
                if (strlen(targetIP) > 0 && strlen(portBuf) > 0) {
                    start_connecting(targetIP, atoi(portBuf));
                }
            }
        }
        // UI CHO GIAO DIỆN PHÒNG CHAT
        else {
            ImGui::Text("Hello, %s! (P2P Mode)", strlen(username) > 0 ? username : "Anonymous");
            ImGui::SameLine(ImGui::GetWindowWidth() - 170);

            if (ImGui::Button("Leave Room")) {
                stop_network();
            }
            ImGui::Separator();

            ImGui::BeginChild("ChatRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 10), true);
            {
                lock_guard<mutex> lock(chat_mutex);
                for (const auto& msg : chatHistory) {
                    ImGui::TextWrapped("%s", msg.c_str());
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            // Khung nhập tin nhắn vẫn giữ hoạt động local khi ngắt mạng
            ImGui::PushItemWidth(-70);
            bool isEnterPressed = ImGui::InputText("##InputBox", messageBuf, IM_ARRAYSIZE(messageBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();
            ImGui::SameLine();

            if (ImGui::Button("Send", ImVec2(60, 0)) || isEnterPressed) {
                if (strlen(messageBuf) > 0) {
                    string display_name = (strlen(username) > 0) ? string(username) : "Anonymous";

                    if (peer_socket && peer_socket->is_open()) {
                        string out_msg = display_name + ": " + messageBuf + "\n";
                        asio::error_code ec;
                        asio::write(*peer_socket, asio::buffer(out_msg), ec);

                        if (!ec) {
                            add_log(display_name + ": " + messageBuf);
                        }
                        else {
                            add_log(display_name + ": " + messageBuf + " [Not Sent - Peer Disconnected]");
                            close_socket_only();
                        }
                    }
                    else {
                        add_log(display_name + ": " + messageBuf + " [Local Only]");
                    }

                    messageBuf[0] = '\0';
                    ImGui::SetKeyboardFocusHere(-1);
                }
            }
        }
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    stop_network();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}