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
#include <string>
#include <vector>
#include <thread>
#include <mutex>

using namespace std;

typedef websocketpp::client<websocketpp::config::asio_client> ws_client_t;

// Các biến toàn cục quản lý trạng thái
ws_client_t* active_client = nullptr; // Dùng con trỏ để có thể cấp phát mới liên tục
websocketpp::connection_hdl ws_hdl;
mutex chat_mutex; 
vector<string> chatHistory;
bool isConnected = false;
char username[128] = "";
char roomID[128] = "";
char messageBuf[1024] = "";

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(600, 700, "P2P Chat App", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
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
            ImGui::Text("=== P2P CHAT APP ===");
            ImGui::Spacing(); ImGui::Spacing();

            ImGui::Text("Username:");
            ImGui::InputText("##username", username, IM_ARRAYSIZE(username));
            
            ImGui::Spacing();
            
            ImGui::Text("RoomID (IP):");
            ImGui::InputText("##roomid", roomID, IM_ARRAYSIZE(roomID));
            
            ImGui::Spacing(); ImGui::Spacing();

            // KHI BẤM KẾT NỐI: Tạo một Client mạng hoàn toàn mới
            if (ImGui::Button("Connect", ImVec2(200, 40))) {
                if (strlen(username) > 0 && strlen(roomID) > 0) {
                    isConnected = true;
                    chatHistory.clear();
                    chatHistory.push_back("[System] Loading Sever...");
                    
                    // Cấp phát động client mới để tránh bị kẹt trạng thái cũ
                    active_client = new ws_client_t();
                    active_client->clear_access_channels(websocketpp::log::alevel::all);
                    active_client->init_asio();

                    // Gắn sự kiện nhận tin
                    active_client->set_message_handler([](websocketpp::connection_hdl hdl, ws_client_t::message_ptr msg) {
                        lock_guard<mutex> lock(chat_mutex);
                        string incoming = msg->get_payload();
                        string myRoomPrefix = "[" + string(roomID) + "] ";

                        if (incoming.find(myRoomPrefix) == 0) {
                            string cleanMsg = incoming.substr(myRoomPrefix.length());
                            chatHistory.push_back(cleanMsg);
                        }
                    });

                    // Gắn sự kiện mở kết nối
                    active_client->set_open_handler([](websocketpp::connection_hdl hdl) {
                        ws_hdl = hdl;
                        lock_guard<mutex> lock(chat_mutex);
                        chatHistory.push_back("[System] Connected to server 8080!");
                    });

                    // Gắn sự kiện đóng kết nối
                    active_client->set_close_handler([](websocketpp::connection_hdl hdl) {
                        lock_guard<mutex> lock(chat_mutex);
                        chatHistory.push_back("[System] Disconnected!");
                    });

                    // Chạy luồng mạng ngầm
                    thread([]() {
                        websocketpp::lib::error_code ec;
                        ws_client_t::connection_ptr con = active_client->get_connection("ws://127.0.0.1:8080/signaling", ec);
                        if (!ec) {
                            active_client->connect(con);
                            active_client->run(); 
                        }
                    }).detach();
                }
            }
        } 
        else {
            ImGui::Text("Welcome, %s! You're in RoomID: %s", username, roomID);
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            
            // KHI BẤM RỜI PHÒNG: Hủy hẳn client cũ đi để giải phóng tài nguyên mạng
            if (ImGui::Button("Leave")) {
                isConnected = false;
                if (active_client) {
                    websocketpp::lib::error_code ec;
                    active_client->close(ws_hdl, websocketpp::close::status::normal, "", ec);
                    active_client->stop();
                    delete active_client; // Xóa sạch sẽ vùng nhớ cũ
                    active_client = nullptr;
                }
                chatHistory.clear(); 
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

            ImGui::PushItemWidth(-70);
            bool isEnterPressed = ImGui::InputText("##InputBox", messageBuf, IM_ARRAYSIZE(messageBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            
            if (ImGui::Button("Send", ImVec2(60, 0)) || isEnterPressed) {
                if (strlen(messageBuf) > 0) {
                    string displayMessage = string(username) + ": " + messageBuf;
                    string networkMessage = "[" + string(roomID) + "] " + displayMessage;
                    
                    {
                        lock_guard<mutex> lock(chat_mutex);
                        chatHistory.push_back(displayMessage); 
                    }
                    
                    if (active_client) {
                        websocketpp::lib::error_code ec;
                        active_client->send(ws_hdl, networkMessage, websocketpp::frame::opcode::text, ec);
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

    if (active_client) {
        delete active_client;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}