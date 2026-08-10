#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_THREAD_
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_
#define _WEBSOCKETPP_CPP11_MEMORY_

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <ctime>
#include <cstring>

using namespace std;

typedef websocketpp::client<websocketpp::config::asio_client> ws_client_t;

// --- CẤU TRÚC DỮ LIỆU MỚI CHO TIN NHẮN ---
struct ChatMessage {
    string timestamp;
    string sender;
    string content;
    string replyToMsg;  // Lưu nội dung tin nhắn gốc nếu đây là tin Reply
};

// State Variables
ws_client_t* active_client = nullptr;
websocketpp::connection_hdl ws_hdl;
mutex chat_mutex; 
vector<ChatMessage> chatHistory; // Sử dụng struct mới
bool isConnected = false;

// Trạng thái cho UI mới của bạn
string replyingTo = ""; 
string forwardingMsg = "";

char serverIP[128] = "127.0.0.1";
char serverPort[32] = "8080";
char username[128] = "";
char roomID[128] = "";
char messageBuf[1024] = "";

// Helper: Get timestamp
string get_timestamp() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buf[32];
    sprintf(buf, "[%02d:%02d:%02d]", ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    return string(buf);
}

// Split string helper
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

// Hàm Load History đơn giản hóa (để khớp với struct mới)
void load_history(const string& room) {
    chatHistory.clear();
    // Tạm thời ẩn phần load file cũ để tránh lỗi parse chuỗi, 
    // nhóm cần thống nhất định dạng file JSON hoặc TXT cấu trúc mới sau.
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(650, 750, "LTM_ChatP2P - Client", NULL, NULL);
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

        if (!isConnected) {
            ImGui::Text("=== LTM CHAT APP ===");
            ImGui::Spacing(); ImGui::Spacing();

            ImGui::Text("Server IP:");
            ImGui::InputText("##serverip", serverIP, IM_ARRAYSIZE(serverIP));
            
            ImGui::Text("Server Port:");
            ImGui::InputText("##serverport", serverPort, IM_ARRAYSIZE(serverPort));
            ImGui::Separator();

            ImGui::Text("Username:");
            ImGui::InputText("##username", username, IM_ARRAYSIZE(username));
            
            ImGui::Spacing();
            
            ImGui::Text("RoomID:");
            ImGui::InputText("##roomid", roomID, IM_ARRAYSIZE(roomID));
            
            ImGui::Spacing(); ImGui::Spacing();

            if (ImGui::Button("Connect", ImVec2(200, 40))) {
                if (strlen(username) > 0 && strlen(roomID) > 0 && strlen(serverIP) > 0) {
                    isConnected = true;
                    load_history(string(roomID));
                    
                    lock_guard<mutex> lock(chat_mutex);
                    chatHistory.push_back({get_timestamp(), "[System]", "Connecting to Server...", ""});
                    
                    active_client = new ws_client_t();
                    active_client->clear_access_channels(websocketpp::log::alevel::all);
                    active_client->init_asio();

                    // On Message
                    active_client->set_message_handler([](websocketpp::connection_hdl hdl, ws_client_t::message_ptr msg) {
                        lock_guard<mutex> lock(chat_mutex);
                        string incoming = msg->get_payload();
                        vector<string> parts = split(incoming, '|');
                        
                        if (parts.size() >= 4) {
                            string action = parts[0];
                            string sender = parts[2];
                            string content = parts[3];
                            
                            ChatMessage newMsg;
                            newMsg.timestamp = get_timestamp();
                            newMsg.sender = (action == "SYS") ? "[System]" : ((action == "ERR") ? "[ERROR]" : sender);
                            newMsg.content = content;
                            
                            // Parse Reply tag nếu có
                            if (parts.size() >= 5 && parts[4].rfind("REPLY:", 0) == 0) {
                                newMsg.replyToMsg = parts[4].substr(6);
                            }
                            
                            chatHistory.push_back(newMsg);
                        }
                    });

                    // On Open
                    active_client->set_open_handler([](websocketpp::connection_hdl hdl) {
                        ws_hdl = hdl;
                        string joinMsg = "JOIN|" + string(roomID) + "|" + string(username) + "|";
                        websocketpp::lib::error_code ec;
                        active_client->send(ws_hdl, joinMsg, websocketpp::frame::opcode::text, ec);
                    });

                    // On Close & Fail
                    active_client->set_close_handler([](websocketpp::connection_hdl hdl) {
                        lock_guard<mutex> lock(chat_mutex);
                        chatHistory.push_back({get_timestamp(), "[System]", "Disconnected from Server.", ""});
                    });
                    active_client->set_fail_handler([](websocketpp::connection_hdl hdl) {
                        lock_guard<mutex> lock(chat_mutex);
                        chatHistory.push_back({get_timestamp(), "[System]", "Connection failed!", ""});
                    });

                    // Connect (Thread)
                    thread([]() {
                        string uri = "ws://" + string(serverIP) + ":" + string(serverPort) + "/signaling";
                        websocketpp::lib::error_code ec;
                        ws_client_t::connection_ptr con = active_client->get_connection(uri, ec);
                        if (!ec) {
                            active_client->connect(con);
                            active_client->run(); 
                        }
                    }).detach();
                }
            }
        } 
        else {
            ImGui::Text("User: %s | Room: %s", username, roomID);
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            
            if (ImGui::Button("Leave")) {
                isConnected = false;
                if (active_client) {
                    websocketpp::lib::error_code ec;
                    active_client->close(ws_hdl, websocketpp::close::status::normal, "", ec);
                    active_client->stop();
                    delete active_client; 
                    active_client = nullptr;
                }
            }
            ImGui::Separator();

            // KHU VỰC HIỂN THỊ TIN NHẮN (ĐÃ TÍCH HỢP AVATAR, REPLY, FORWARD)
            ImGui::BeginChild("ChatRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 60), true);
            {
                lock_guard<mutex> lock(chat_mutex);
                for (int i = 0; i < chatHistory.size(); ++i) {
                    auto& msg = chatHistory[i];
                    
                    ImGui::PushID(i);
                    // Avatar giả lập bằng chữ cái đầu
                    string avatarChar = msg.sender.empty() ? "?" : msg.sender.substr(0, 1);
                    ImGui::Button(avatarChar.c_str(), ImVec2(30, 30)); 
                    ImGui::SameLine();
                    
                    ImGui::BeginGroup(); 
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s %s", msg.timestamp.c_str(), msg.sender.c_str());
                    
                    if (!msg.replyToMsg.empty()) {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  | Replying to: %s", msg.replyToMsg.c_str());
                    }
                    
                    ImGui::TextWrapped("%s", msg.content.c_str());
                    
                    if (msg.sender != "[System]" && msg.sender != "[ERROR]") {
                        if (ImGui::SmallButton("Reply")) {
                            replyingTo = msg.sender + ": " + msg.content;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Forward")) {
                            // Tạm thời copy nội dung vào ô chat để gửi đi nơi khác
                            strncpy(messageBuf, msg.content.c_str(), sizeof(messageBuf));
                            messageBuf[sizeof(messageBuf) - 1] = '\0';
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            // KHU VỰC NHẬP LIỆU (ĐÃ TÍCH HỢP EMOJI)
            if (!replyingTo.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Replying: %s", replyingTo.c_str());
                ImGui::SameLine();
                if (ImGui::Button("X##CancelReply")) replyingTo = "";
            }

            if (ImGui::Button("😀 Emoji")) ImGui::OpenPopup("EmojiPicker");
            if (ImGui::BeginPopup("EmojiPicker")) {
                const char* emojis[] = { "(^-^)", "(T_T)", "(>_<)", "(@_@)", "<3", "(:", "):" };
                for (int i = 0; i < 7; i++) {
                    if (i % 4 != 0) ImGui::SameLine();
                    if (ImGui::Button(emojis[i], ImVec2(40, 30))) {
                        strncat(messageBuf, emojis[i], IM_ARRAYSIZE(messageBuf) - strlen(messageBuf) - 1);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::PushItemWidth(-70);
            bool isEnterPressed = ImGui::InputText("##InputBox", messageBuf, IM_ARRAYSIZE(messageBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            
            if (ImGui::Button("Send", ImVec2(60, 0)) || isEnterPressed) {
                if (strlen(messageBuf) > 0) {
                    string content = string(messageBuf);
                    string networkMsg = "MSG|" + string(roomID) + "|" + string(username) + "|" + content;
                    if (!replyingTo.empty()) {
                        networkMsg += "|REPLY:" + replyingTo; 
                    }

                    ChatMessage newMsg = {get_timestamp(), "Me", content, replyingTo};
                    
                    {
                        lock_guard<mutex> lock(chat_mutex);
                        chatHistory.push_back(newMsg); 
                    }
                    
                    if (active_client) {
                        websocketpp::lib::error_code ec;
                        active_client->send(ws_hdl, networkMsg, websocketpp::frame::opcode::text, ec);
                    }

                    messageBuf[0] = '\0';
                    replyingTo = "";
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

    if (active_client) {
        websocketpp::lib::error_code ec;
        active_client->close(ws_hdl, websocketpp::close::status::normal, "", ec);
        active_client->stop();
        delete active_client;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}